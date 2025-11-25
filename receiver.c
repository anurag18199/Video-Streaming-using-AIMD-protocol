#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 8888
#define BUFFER_SIZE 2048 // Must be > Sender PACKET_SIZE + HEADER
#define WINDOW_SIZE 100  // Calculate loss over last N packets

typedef struct {
    unsigned int seq_num;
    unsigned int timestamp;
    // Data follows immediately
} Header;

int main() {
    int sockfd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Create Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Loss Calculation Variables
    unsigned int expected_seq = 0;
    int received_count = 0;
    int lost_count = 0;
    int total_packets_window = 0;

    // Don't print text to stdout, only video data! Use stderr for logs.
    fprintf(stderr, "[Receiver] Listening on port %d...\n", PORT);
    fprintf(stderr, "[Receiver] Pipe this output to ffplay!\n");

    while (1) {
        int len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                           (struct sockaddr *)&client_addr, &addr_len);
        
        if (len > 0) {
            // 1. Parse Header
            unsigned int seq_num, timestamp;
            // Extract first 8 bytes manually to avoid struct padding issues across machines
            memcpy(&seq_num, buffer, 4);
            memcpy(&timestamp, buffer + 4, 4);
            seq_num = ntohl(seq_num);
            timestamp = ntohl(timestamp);

            int data_len = len - 8; // 8 bytes header

            // 2. Calculate Loss (Logic to check packet dropping)
            if (expected_seq != 0 && seq_num > expected_seq) {
                int gap = seq_num - expected_seq;
                lost_count += gap;
                total_packets_window += gap;
                // fprintf(stderr, "Lost %d packets\n", gap);
            }
            
            expected_seq = seq_num + 1;
            received_count++;
            total_packets_window++;

            // 3. Send Feedback every WINDOW_SIZE packets
            if (total_packets_window >= WINDOW_SIZE) {
                float loss_rate = (float)lost_count / total_packets_window;
                
                // Prepare feedback packet
                char feedback_msg[64];
                snprintf(feedback_msg, sizeof(feedback_msg), "LOSS %f", loss_rate);
                
                // Send feedback to sender
                sendto(sockfd, feedback_msg, strlen(feedback_msg), 0, 
                       (const struct sockaddr *)&client_addr, addr_len);

                // Reset window
                lost_count = 0;
                total_packets_window = 0;
            }

            // 4. Write Video Data to Pipe (Standard Output)
            if (data_len > 0) {
                fwrite(buffer + 8, 1, data_len, stdout);
                fflush(stdout);
            }
        }
    }

    close(sockfd);
    return 0;
}