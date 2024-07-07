import re
import sys

# Define a regular expression pattern to extract port and length information
pattern = re.compile(
    r'(\d+:\d+:\d+\.\d+) IP [\d.]+\.(\d+) > [\d.]+\.(\d+): .*? length (\d+)')

# Specify the path to your tcpdump output file
file_path = sys.argv[1]

traffic_by_port = {'mpc': 0, 'non_mpc': 0}

matched_line = 0

non_mpc_port = [5555,5556,5557]

min_mpc_port = 12345
max_mpc_port = 12345+512
total_length = 0

# Open and read the tcpdump output file
with open(file_path, 'r') as file:
    for line in file:
        match = pattern.search(line)
        if match:
            source_port = int(match.group(2))
            destination_port = int(match.group(3))
            length = int(match.group(4))

            # Append the port and length values to the respective lists
            non_mpc = False
            mpc = False
            if (source_port in non_mpc_port) or (destination_port in non_mpc_port):
                traffic_by_port['non_mpc'] += length
                non_mpc = True

            if ((source_port >= min_mpc_port) and (source_port <= max_mpc_port)) or ((destination_port >= min_mpc_port) and (destination_port <= max_mpc_port)):
                traffic_by_port['mpc'] += length
                mpc = True

            matched_line += 1
            total_length += length

            if not (non_mpc and (not mpc)):
                print("error line:", line)
        else:
            print('unmatched line', line)

print(traffic_by_port)
print('total length', total_length)
print("matched lines:", matched_line)
