#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8888
#define PACKET_SIZE 1400 
#define HEADER_SIZE 8

// AIMD Parameters
#define ALPHA 100         
#define BETA 0.5          
#define MIN_RATE 50       
#define MAX_RATE 5000     

// Network Simulation
#define ARTIFICIAL_LOSS_PROB 0.02 

typedef struct {
    unsigned int seq_num;
    unsigned int timestamp;
    unsigned char data[PACKET_SIZE];
} Packet;

unsigned long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <video_file.h264>\n", argv[0]);
        return 1;
    }

    // --- LOGGING SETUP ---
    FILE *log_fp = fopen("aimd_log.csv", "w");
    if (!log_fp) {
        perror("Could not open log file");
        return 1;
    }
    // Write CSV Header
    fprintf(log_fp, "TimeMS,Rate,Loss\n");
    unsigned long start_time = get_time_us();
    // ---------------------

    int sockfd;
    struct sockaddr_in receiver_addr, src_addr;
    socklen_t addr_len = sizeof(src_addr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(PORT);
    receiver_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("File open error");
        return 1;
    }

    unsigned int current_rate = 500; 
    unsigned int seq_num = 0;
    Packet pkt;
    int bytes_read;
    
    printf("[Sender] Starting Stream with Logging...\n");

    char feedback_buf[128];
    float reported_loss = 0.0;

    while ((bytes_read = fread(pkt.data, 1, PACKET_SIZE, fp)) > 0) {
        
        pkt.seq_num = htonl(seq_num++); 
        pkt.timestamp = htonl((unsigned int)(get_time_us() / 1000)); 

        // Artificial Packet Loss Logic
        double random_val = (double)rand() / RAND_MAX;
        if (random_val > ARTIFICIAL_LOSS_PROB) {
            sendto(sockfd, &pkt, bytes_read + HEADER_SIZE, 0, 
                   (const struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
        }

        if (current_rate < MIN_RATE) current_rate = MIN_RATE;
        unsigned int sleep_us = 1000000 / current_rate;
        usleep(sleep_us);

        // Check for Feedback
        int n = recvfrom(sockfd, feedback_buf, sizeof(feedback_buf), 0, 
                         (struct sockaddr *)&src_addr, &addr_len);
        
        if (n > 0) {
            feedback_buf[n] = '\0';
            if (strncmp(feedback_buf, "LOSS", 4) == 0) {
                sscanf(feedback_buf, "LOSS %f", &reported_loss);
                
                // --- AIMD ALGORITHM ---
                if (reported_loss > 0.05) { 
                    current_rate = (unsigned int)(current_rate * BETA);
                    if(current_rate < MIN_RATE) current_rate = MIN_RATE;
                    fprintf(stderr, "\n[AIMD] Loss: %.2f%% | Rate: %d (DROP)", reported_loss*100, current_rate);
                } else {
                    current_rate += ALPHA;
                    if(current_rate > MAX_RATE) current_rate = MAX_RATE;
                    if (seq_num % 50 == 0) 
                        fprintf(stderr, "\r[AIMD] Loss: %.2f%% | Rate: %d (RISE)", reported_loss*100, current_rate);
                }

                // --- LOGGING DATA TO FILE ---
                // Time relative to start (ms)
                unsigned long relative_time = (get_time_us() - start_time) / 1000;
                fprintf(log_fp, "%lu,%d,%.4f\n", relative_time, current_rate, reported_loss);
                // ----------------------------
            }
        }
    }

    printf("\n[Sender] Done. Data saved to 'aimd_log.csv'.\n");
    fclose(fp);
    fclose(log_fp);
    close(sockfd);
    return 0;
}