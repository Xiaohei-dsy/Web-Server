import subprocess
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import interp1d

# Set parameters
server_address = "shaoyu.site"
url_path = "/"
load_levels = [1000, 2000, 3000, 4000, 5000]  # Load levels list
rate = 700  # Requests per second rate

# Store results in lists
latencies = []

# Test load vs. latency
for load in load_levels:
    command = f"httperf --server {server_address} --port 9007 --uri {url_path} --num-conns {load} --rate {rate}"
    output = subprocess.check_output(command, shell=True)
    # Extract load and latency data
    latency_line = [line for line in output.decode().split("\n") if "Reply time [ms]:" in line][0]
    #print(latency_line.split()[4])
    latency = float(latency_line.split()[4])
    latencies.append(latency)


# Interpolate to generate a smooth curve
x_new = np.linspace(min(load_levels), max(load_levels), 300)  # Create more data points
f = interp1d(load_levels, latencies, kind='cubic')  # choose the interpolation method
y_smooth = f(x_new)

# Plot load vs. latency graph
plt.figure(2)
plt.plot(x_new, y_smooth)
plt.scatter(load_levels, latencies)
plt.xlabel('Load (req)')
plt.ylabel('Latency (ms)')
plt.title('Load vs. Latency')
for i, j in zip(load_levels, latencies):
    plt.text(i, j, f'{j:.2f}', ha='center', va='bottom', rotation=90)  # Adjust rotation angle and position
plt.savefig('load_latency.png')  # Save as PNG file
