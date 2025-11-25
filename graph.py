import pandas as pd
import matplotlib.pyplot as plt
import sys
import os


LOG_FILE = 'aimd_log.csv'
OUTPUT_FILE = 'aimd_graph_python.png'

def generate_graph():
    if not os.path.exists(LOG_FILE):
        print(f"Error: '{LOG_FILE}' not found. Run the C sender first to generate data.")
        sys.exit(1)

    # Read the CSV data
    try:
        df = pd.read_csv(LOG_FILE)
    except pd.errors.EmptyDataError:
        print("Error: Log file is empty.")
        sys.exit(1)

    # Create the figure and primary axis
    fig, ax1 = plt.subplots(figsize=(12, 6))

    # --- Plot 1: Sending Rate (Left Axis) ---
    color = 'tab:blue'
    ax1.set_xlabel('Time (ms)')
    ax1.set_ylabel('Sending Rate (packets/sec)', color=color)
    ax1.plot(df['TimeMS'], df['Rate'], color=color, linewidth=2, label='Throughput')
    ax1.tick_params(axis='y', labelcolor=color)
    ax1.grid(True, which='both', linestyle='--', linewidth=0.5)

    # --- Plot 2: Packet Loss (Right Axis) ---
    ax2 = ax1.twinx()  # Instantiate a second axes that shares the same x-axis
    color = 'tab:red'
    ax2.set_ylabel('Packet Loss Ratio', color=color)
    
    # We filter for non-zero loss to make the graph cleaner
    loss_data = df[df['Loss'] > 0]
    if not loss_data.empty:
        ax2.bar(loss_data['TimeMS'], loss_data['Loss'], width=200, color=color, alpha=0.5, label='Loss Event')
    
    ax2.tick_params(axis='y', labelcolor=color)
    ax2.set_ylim(0, 1.0) # Loss is usually 0.0 to 1.0

    # Titles and Layout
    plt.title('AIMD Algorithm Performance: Rate vs. Loss')
    fig.tight_layout()  # Otherwise the right y-label is slightly clipped

    # Save the plot
    plt.savefig(OUTPUT_FILE)
    print(f"Graph saved successfully to '{OUTPUT_FILE}'")
    
    # Optional: Show the plot immediately if a display is available
    # plt.show()

if __name__ == "__main__":
    generate_graph()