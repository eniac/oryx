#include "graph_holder.hpp"

const uint32_t id_range = (1 << 23);

int main(int argc, char* argv[]) {
    int max_degree = 10;
    string graph_file = "/users/kezhon0/cycle-codes/demo-graph/demo.txt";
    string output1 = "out1_small";
    string output2 = "out2_small";
    string output3 = "out3_small";
    string out_dir = "/mydata/";

    int opt;
    while ((opt = getopt(argc, argv, "d:i:o:")) != -1) {
        if (opt == 'd') {
            max_degree = atoi(optarg);
        } else if (opt == 'i') {
            graph_file = string(optarg);
        } else if (opt == 'o') {
            out_dir = string(optarg);
        }
    }

    output1 = out_dir + output1;
    output2 = out_dir + output2;
    output3 = out_dir + output3;

    cout << output1 << " " << output2 << " " << output3 << endl;

    GraphHolder graph_holder(max_degree, id_range);
    graph_holder.loadGraphAndDumpShares(graph_file, output1, output2, output3);
    return 0;
}