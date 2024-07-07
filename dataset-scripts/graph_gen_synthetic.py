import random
import sys, getopt

outfile = 'synthetic-graph.txt'
total_nodes = 1000
avg_degree = 2
high_degree = 300
high_degree_nodes = 5

random.seed(10)

fout = open(outfile, "w")
for i in range(0, total_nodes):
    existing_neighbors = {i+1}
    if i < (total_nodes - high_degree_nodes):
        for _ in range(avg_degree):
            neighbor = random.randint(1, total_nodes)
            while (neighbor in existing_neighbors):
                neighbor = random.randint(1, total_nodes)
            fout.write(str(i+1))
            fout.write(' ')
            fout.write(str(neighbor))
            fout.write('\n')
            fout.flush()
            existing_neighbors.add(neighbor)

    else:
        for _ in range(high_degree):
            neighbor = random.randint(1, total_nodes)
            while (neighbor in existing_neighbors):
                neighbor = random.randint(1, total_nodes)
            fout.write(str(i+1))
            fout.write(' ')
            fout.write(str(neighbor))
            fout.write('\n')
            fout.flush()
            existing_neighbors.add(neighbor)

