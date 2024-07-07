import pandas as pd
import numpy as np
import sys, getopt

file_name = '/users/kezhon0/cycle-codes/process-ibm-graph/raw_graph.txt'
max_degree = 10

try:
    opts, args = getopt.getopt(sys.argv[1:],'i:d:')
    print(opts)
except getopt.GetoptError:
    print('usage: -i INPUT_RAW_GRAPH_FILE -d MAX_DEGREE')
    sys.exit(2)

for opt, arg in opts:
    if opt == '-i':
        file_name = arg
    elif opt == '-d':
        max_degree = int(arg)

print(file_name)
print(max_degree)

data = pd.read_csv(file_name, sep=' ', header=None)
data_len = len(data)
print('edge num', data_len)
data.columns = ['src', 'dst']
print(data)

src_nodes = data.iloc[:, 0]
dst_nodes = data.iloc[:, 1]

all_nodes = pd.concat([src_nodes, dst_nodes])
print(all_nodes)
all_nodes = sorted(all_nodes.unique())
print(all_nodes[0], all_nodes[-1], len(all_nodes))

src_freq = src_nodes.value_counts().to_frame().reset_index()
src_freq.columns = ['src','src_freq']
dst_freq = dst_nodes.value_counts().to_frame().reset_index()
dst_freq.columns = ['dst','dst_freq']

print(src_freq)
data = data.join(src_freq.set_index('src'), on='src')
print(data)
data = data.join(dst_freq.set_index('dst'), on='dst')
print(data)

rm_index = data[(data['src_freq'] > max_degree) | (data['dst_freq'] > max_degree)].index
data.drop(rm_index, inplace=True)
print(len(data))

src_nodes = data['src']
dst_nodes = data['dst']
unique_nodes = np.unique(np.append(src_nodes.unique(), dst_nodes.unique()))

idx_dict = {}
idx = 1
for v in unique_nodes:
    idx_dict[v] = idx
    idx += 1

outfile = 'degree_limit_to_10_graph.txt'
fout = open(outfile, "w")
graph_len = len(data)
print("Number of edges", graph_len)
print("Number of unique nodes", len(unique_nodes))
for i in range(graph_len):
    src = data['src'].iat[i]
    dst = data['dst'].iat[i]
    fout.write(str(idx_dict[src]))
    fout.write(' ')
    fout.write(str(idx_dict[dst]))
    fout.write('\n')
    fout.flush()
fout.close()
