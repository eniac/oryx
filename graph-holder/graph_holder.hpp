#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "secret-share/secret_share.hpp"

const int kDetectLength = 2;
// const uint64_t kTieBreakerRange = (1 << 25);

class GraphHolder {
   public:
    unordered_map<uint32_t, vector<uint32_t>> edges;

    vector<unique_ptr<SecretShareTuple>> edge_tuples;
    vector<unique_ptr<SecretShareTuple>> path_tuples;

    ServerSecretShares s1_vec;
    ServerSecretShares s2_vec;
    ServerSecretShares s3_vec;

    unordered_set<uint32_t> unique_nodes;

    int max_degree;
    uint32_t id_range;
    unique_ptr<SecretShareTupleHelper> helper;

    GraphHolder(int max_degree_, uint32_t id_range_)
        : max_degree(max_degree_), id_range(id_range_) {
        helper =
            make_unique<SecretShareTupleHelper>(id_range, kTieBreakerRange);
    }

    // load graph data from file
    void load_graph(string graph_file_name) {
        ifstream fs(graph_file_name);
        MYASSERT(fs.is_open());
        string line;
        uint32_t src, dst;
        uint32_t max_node_id = 0;
        while (getline(fs, line)) {
            stringstream ss(line);
            ss >> src >> dst;
            unique_nodes.insert(src);
            unique_nodes.insert(dst);
            MYASSERT(src <= id_range);
            MYASSERT(src > 0);
            MYASSERT(dst <= id_range);
            MYASSERT(dst > 0);
            max_node_id = max(max_node_id, src);
            max_node_id = max(max_node_id, dst);

            if (edges.find(src) == edges.end()) {
                edges[src] = vector<uint32_t>();
            }
            edges[src].push_back(dst);
        }
        MYASSERT(max_node_id < id_range);
        MYASSERT(unique_nodes.size() < id_range);
        MYASSERT(max_node_id == unique_nodes.size());

        // in case some nodes have no out-going edges, create an empty
        for (const auto& node : unique_nodes) {
            if (edges.find(node) == edges.end()) {
                edges[node] = vector<uint32_t>();
            }
        }

        MYASSERT(edges.size() == unique_nodes.size());
        cout << "load graph finish\n";
    }

    // Format tuples
    void formatSecretShares() {
        for (auto it = edges.begin(); it != edges.end(); ++it) {
            uint32_t src = it->first;
            vector<uint32_t> src_edges = it->second;
            unique_ptr<SecretShareTuple> et = helper->formatEdgeTuple(
                src, src_edges, max_degree, kDetectLength);
            edge_tuples.push_back(move(et));
            for (const auto dst : src_edges) {
                unique_ptr<SecretShareTuple> pt = helper->formatPathTuple(
                    {src, dst}, max_degree, kDetectLength);
                path_tuples.push_back(move(pt));
            }
        }
        cout << "edge tuple count: " << edge_tuples.size() << endl;
        cout << "path tuple count: " << path_tuples.size() << endl;
        cout << "formatting finish" << endl;
    }

    // create secret shares
    void createSecretShares() {
        for (uint64_t i = 0; i < edge_tuples.size(); i++) {
            if (i % 100000 == 0) {
                cout << "formatting edge tuple: " << i << " "
                     << edge_tuples.size() << endl;
            }
            vector<unique_ptr<SecretShareTuple>> ss =
                helper->splitTuple(edge_tuples[i]);
            s1_vec.insert(ss[0]);
            s2_vec.insert(ss[1]);
            s3_vec.insert(ss[2]);
        }
        for (uint64_t i = 0; i < path_tuples.size(); i++) {
            if (i % 100000 == 0) {
                cout << "formatting path tuple: " << i << " "
                     << path_tuples.size() << endl;
            }
            vector<unique_ptr<SecretShareTuple>> ss =
                helper->splitTuple(path_tuples[i]);
            s1_vec.insert(ss[0]);
            s2_vec.insert(ss[1]);
            s3_vec.insert(ss[2]);
        }
        cout << "finish creating secret shares" << endl;
    }

    // dump three secret shares to three files
    void dumpSecretShares(string file1, string file2, string file3) {
        string s1_str = s1_vec.serialize();
        ofstream out1(file1);
        MYASSERT(out1.is_open());
        out1 << s1_str;
        out1.close();
        // unique_ptr<ServerSecretShares> recon_s1 =
        //     make_unique<ServerSecretShares>(s1_str, max_degree,
        //                                     /*detect_length_k=*/2);
        // MYASSERT(*recon_s1.get() == s1_vec);
        s1_str.clear();

        string s2_str = s2_vec.serialize();
        ofstream out2(file2);
        MYASSERT(out2.is_open());
        out2 << s2_str;
        out2.close();
        // unique_ptr<ServerSecretShares> recon_s2 =
        //     make_unique<ServerSecretShares>(s2_str, max_degree,
        //                                     /*detect_length_k=*/2);
        // MYASSERT(*recon_s2.get() == s2_vec);
        s2_str.clear();

        string s3_str = s3_vec.serialize();
        ofstream out3(file3);
        MYASSERT(out3.is_open());
        out3 << s3_str;
        out3.close();
        // unique_ptr<ServerSecretShares> recon_s3 =
        //     make_unique<ServerSecretShares>(s3_str, max_degree,
        //                                     /*detect_length_k=*/2);
        // MYASSERT(*recon_s3.get() == s3_vec);
        s3_str.clear();
        cout << "finish dumping files\n";
    }

    void loadGraphAndDumpShares(string input, string outfile1, string outfile2,
                                string outfile3) {
        load_graph(input);
        formatSecretShares();
        createSecretShares();
        dumpSecretShares(outfile1, outfile2, outfile3);
    }
};