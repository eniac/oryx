import pandas as pd
import numpy as np
import random, sys, getopt

file_name = '/users/kezhon0/cycle-codes/process-ibm-graph/degree_limit_to_10_graph.txt'
sample_percentage = 0.5

try:
    opts, args = getopt.getopt(sys.argv[1:],'i:s:')
    print(opts)
except getopt.GetoptError:
    print('usage: -i INPUT_GRAPH_FILE -s SAMPLE_PERCENTAGE')
    sys.exit(2)

for opt, arg in opts:
    if opt == '-i':
        file_name = arg
    elif opt == '-s':
        sample_percentage = float(arg)

print(file_name)
print(sample_percentage)

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
print('smallest node', all_nodes[0], 'largest node', all_nodes[-1], len(all_nodes))

sample_size = int(len(data) * sample_percentage)
sampled_edges_index = random.sample(range(len(data)), sample_size)
sampled_edges_index = set(sampled_edges_index)
print('finish sampling edges index')

outfile = 'sampled_'+str(sample_percentage*100)+'_percent_graph.txt'
fout = open(outfile, "w")
graph_len = len(data)
edges_num = 0

idx_dict = {}
idx = 1
for i in range(graph_len):
    src = data['src'].iat[i]
    dst = data['dst'].iat[i]
    if (i not in sampled_edges_index):
        continue
    if src not in idx_dict:
        idx_dict[src] = idx
        idx += 1
    if dst not in idx_dict:
        idx_dict[dst] = idx
        idx += 1

    fout.write(str(idx_dict[src]))
    fout.write(' ')
    fout.write(str(idx_dict[dst]))
    fout.write('\n')
    fout.flush()
    edges_num+=1
fout.close()
print('edges num:', edges_num, 'nodes num:', len(idx_dict), 'sampled edge num:',sample_size)
