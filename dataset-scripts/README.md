# Usage
+  [transform_raw_data_into_graph.py](./transform_raw_data_into_graph.py) is
   used to read the raw file data of ibm dataset and creates the graph file in
   the format of lines of `node1 node2`. Also duplicated edges are dropped.
   + To download raw dataset file `gdown
     https://drive.google.com/uc?id=1NujhAE3XOLj1o0Mk51nTVqsZmJSl0s4P`.
   
+ [gen_with_degree_limit.py](./gen_with_degree_limit.py) is used to create a
  graph with limits on the degree on the raw graph data.
  + Usage `python3 gen_with_degree_limit -i INPUT_RAW_GRAPH_FILE -d MAX_DEGREE`.
  + To download graph with degree limit of 10, `gdown
    https://drive.google.com/uc?id=1-D6Hchkrj_ZbV6PivYLcBwIncD54QV8v`.

+ [sample_percentage_from_degree_limit_graph.py](./sample_percentage_from_degree_limit_graph.py)
  samples some edges of a given percentage from a given graph file.
  + Usage `python3 sample_percentage_from_degree_limit_graph.py -i
    INPUT_GRAPH_FILE -s SAMPLE_PERCENTAGE`.

+ [graph_gen_synthetic.py](./graph_gen_synthetic.py) generates the synthetic
  small graph for end-to-end evaluation over small graph.
