import subprocess
import matplotlib.pyplot as plt

# Set parameters
server_address = "shaoyu.site"
url_path = "/"
concurrency_levels = [50, 100, 200, 300, 500, 700, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000]  # Concurrency levels list
num_connections = 1000  # Number of connections per test

# Store results in lists
throughputs = []

# Test concurrency vs. throughput
for concurrency in concurrency_levels:
    command = f"httperf --client=0/1 --server={server_address} --port=9007 --uri={url_path} --num-conns={num_connections} --rate={concurrency} --timeout=1"
    output = subprocess.check_output(command, shell=True)
    # Extract concurrency and throughput data
    throughput_line = [line for line in output.decode().split("\n") if "Request rate" in line][0]
    #print(throughput_line.split()[2])
    throughput = float(throughput_line.split()[2])
    throughputs.append(throughput)

# Plot concurrency vs. throughput graph
plt.figure(1)
plt.plot(concurrency_levels, throughputs, 'o-')
plt.xlabel('Concurrency (req/s)')
plt.ylabel('Throughput (req/s)')
plt.title('Concurrency vs. Throughput')
for i, j in zip(concurrency_levels, throughputs):
    plt.text(i, j, f'{j:.2f}', ha='center', va='bottom', rotation=90)
plt.savefig('concurrency_throughput.png')  # Save as PNG file