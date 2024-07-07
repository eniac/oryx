#include <unistd.h>

#include "graphstore.hpp"

int main(int argc, char* argv[]) {
    string graph_file =
        "/users/kezhon0/cycle-codes/graph-scripts/degree-limit-graph.txt";

    int opt;
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        if (opt == 'i') {
            graph_file = string(optarg);
        }
    }

    // std::string outfile = "output.txt";
    // std::ofstream fout(outfile);
    GraphStore gs(graph_file);

    gs.load_graph();
    int cycle_num = 0;
    cycle_num = gs.count_cycle(2);
    cout << "number of cycles of length two: " << cycle_num << std::endl;
    cycle_num = gs.count_cycle(3);
    cout << "number of triangles: " << cycle_num << std::endl;
    cycle_num = gs.count_cycle(4);
    cout << "number of 4-cycle: " << cycle_num << std::endl;
    cycle_num = gs.count_cycle(5);
    cout << "number of 5-cycle: " << cycle_num << std::endl;
    cycle_num = gs.count_cycle(6);
    cout << "number of 6-cycle: " << cycle_num << std::endl;
    // fout.flush();
    // fout.close();
    return 0;
}