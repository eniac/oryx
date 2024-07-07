import pandas as pd
import numpy as np
from multiprocessing import Manager, Process
import collections
import matplotlib.pyplot as plt

file_name = '/users/kezhon0/trans_3000p2_list.txt'

data = pd.read_csv(file_name)
data_len = len(data)

src_nodes = data.iloc[:, 1] + data.iloc[:, 2]
dst_nodes = data.iloc[:, 3] + data.iloc[:, 4]

valid_trans = (src_nodes != dst_nodes)
src_nodes = valid_trans*src_nodes
src_nodes = src_nodes.mask(src_nodes.eq('')).dropna()
dst_nodes = valid_trans*dst_nodes
dst_nodes = dst_nodes.mask(dst_nodes.eq('')).dropna()

print('total src nodes', len(src_nodes))
assert(len(src_nodes) == len(dst_nodes))

unique_nodes = np.unique(np.append(src_nodes.unique(), dst_nodes.unique()))

graph = pd.concat([src_nodes, dst_nodes], axis=1)
print(graph)
idx_dict = {}
idx = 1
for v in unique_nodes:
    idx_dict[v] = idx
    idx += 1

outfile = 'raw_graph.txt'
fout = open(outfile, "w")
graph_len = len(graph)
print("before dedup", graph_len)
graph = graph.drop_duplicates()
graph_len = len(graph)
print("after dedup", graph_len)
for i in range(graph_len):
    src = graph.iloc[i,0]
    dst = graph.iloc[i,1]
    fout.write(str(idx_dict[src]))
    fout.write(' ')
    fout.write(str(idx_dict[dst]))
    fout.write('\n')
    fout.flush()
fout.close()

