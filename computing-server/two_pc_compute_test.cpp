#include "two_pc_compute.hpp"

#include <sstream>
#include <unordered_map>

// const int kBitLen = 10;
const int kMaxDegreeRange = 20;
const int kDebugNum = 20;
// const int kTieBreakerBit = 25;

// the shares correspond to
// {(node_id:1,neighbors:{4,7}),(0,0,0),(0,0,0),
//  (node_id:2,neighbors:{3,4}),(0,0,0)}
void passThread1(int port) {
    TwoPcEngine engine(ALICE, "127.0.0.1", port, kBitLen, kTieBreakerBit);
    int max_degree = 2;
    stringstream ss;
    cout << port << endl;
    ss << "1_" << port << ".txt";
    ofstream out(ss.str());
    MYASSERT(out.is_open());
    vector<uint32_t> secret_shares = {7, 3, 2, 3, 4, 2, 1, 5,
                                      2, 6, 1, 2, 1, 2, 3};
    vector<uint32_t> start_neighbors = {0, 0};
    for (uint64_t i = 0; i < secret_shares.size(); i++) {
        out << i << " original " << secret_shares[i] << endl;
    }
    cout << "running as alice\n";
    vector<uint32_t> updated_local_shares;
    engine.obliviousNeighborPass(secret_shares, start_neighbors, max_degree, 0,
                                 secret_shares.size() / (max_degree + 1),
                                 updated_local_shares);
    for (uint64_t i = 0; i < updated_local_shares.size(); i++) {
        out << i << " " << updated_local_shares[i] << endl;
    }
    out.close();
    cout << "run finish\n";
}

void passThread2(int port) {
    TwoPcEngine engine(BOB, "127.0.0.1", port, kBitLen, kTieBreakerBit);
    int max_degree = 2;
    stringstream ss;
    cout << port << endl;
    ss << "2_" << port << ".txt";
    ofstream out(ss.str());
    MYASSERT(out.is_open());
    vector<uint32_t> secret_shares = {6, 7, 5, 3, 4, 2, 1, 5,
                                      2, 4, 2, 6, 1, 2, 3};
    vector<uint64_t> index = {1, 2, 10, 11};
    for (auto const &i : index) {
        secret_shares[i] = (secret_shares[i] + port - 12345) % (1 << kBitLen);
    }
    vector<uint32_t> start_neighbors = {0, 0};
    for (uint64_t i = 0; i < secret_shares.size(); i++) {
        out << i << " original " << secret_shares[i] << endl;
    }
    cout << "running as bob\n";
    vector<uint32_t> updated_local_shares;
    engine.obliviousNeighborPass(secret_shares, start_neighbors, max_degree, 0,
                                 secret_shares.size() / (max_degree + 1),
                                 updated_local_shares);
    for (uint64_t i = 0; i < updated_local_shares.size(); i++) {
        out << i << " " << updated_local_shares[i] << endl;
    }
    out.close();
    cout << "run finish\n";
}

void parallelObliviousNeighborSinglePassTestMannul(int party) {
    uint64_t num_threads = 5;
    int start_port = 12345;
    vector<thread> t_vec;
    if (party == ALICE) {
        for (uint64_t i = 0; i < num_threads; i++) {
            thread t(&passThread1, start_port);
            start_port++;
            t_vec.push_back(move(t));
        }
    } else {
        for (uint64_t i = 0; i < num_threads; i++) {
            thread t(&passThread2, start_port);
            start_port++;
            t_vec.push_back(move(t));
        }
    }
    for (auto &t : t_vec) {
        t.join();
    }
}

void generateRandSecretShareForNeighborPassing(
    int max_degree, int rand_range, vector<uint32_t> &s1, vector<uint32_t> &s2,
    vector<uint32_t> &expected_passed_neighbors) {
    MYASSERT(s1.empty());
    MYASSERT(s2.empty());
    MYASSERT(expected_passed_neighbors.empty());
    int rand_nodes_num = rand() % rand_range;
    // cout << "max_degree: " << max_degree << endl;
    // cout << "node num: " << rand_nodes_num << endl;
    for (int i = 0; i < rand_nodes_num; i++) {
        // node id cannot be zero so set to i+1
        int node_id = (i + 1) % (1 << kBitLen);
        if (node_id == 0) {
            node_id = 1;
        }
        int rand_node_id_s1 = rand() % (1 << kBitLen);
        int rand_node_id_s2 = node_id ^ rand_node_id_s1;
        s1.push_back(rand_node_id_s1);
        s2.push_back(rand_node_id_s2);

        vector<uint32_t> current_neighbors(max_degree);

        // create neighbors for the node_id.
        for (int j = 0; j < max_degree; j++) {
            uint32_t neighbor_val = rand() % (1 << kBitLen);
            uint32_t neighbor_s1 = rand() % (1 << kBitLen);
            uint32_t neighbor_s2 = neighbor_s1 ^ neighbor_val;
            s1.push_back(neighbor_s1);
            s2.push_back(neighbor_s2);
            current_neighbors[j] = neighbor_val;
            // cout << " current neighbor " << j << " is " << neighbor_val <<
            // endl;
        }

        // create rand_tuple_num of the current node id.
        int rand_tuple_num = rand() % rand_range;
        // cout << "rand tuple num: " << rand_tuple_num << endl;
        for (int j = 0; j < rand_tuple_num; j++) {
            // need to be max_degree+1 as there needs a node id as well.
            for (int k = 0; k < max_degree + 1; k++) {
                uint32_t rand_share = rand() % (1 << kBitLen);
                s1.push_back(rand_share);
                s2.push_back(rand_share);
            }
        }

        // produce the answer
        for (int j = 0; j < (rand_tuple_num + 1); j++) {
            for (int k = 0; k < max_degree; k++) {
                expected_passed_neighbors.push_back(current_neighbors[k]);
            }
        }
    }
}

void parallelObliviousNeighborSinglePassThread(
    int thread_id, int party, int port, int max_degree,
    const vector<uint32_t> &secret_shares, vector<vector<uint32_t>> &res) {
    TwoPcEngine engine(party, "127.0.0.1", port, kBitLen, kTieBreakerBit);
    vector<uint32_t> start_neighbors(max_degree, 0);
    cout << "running as " << party << endl;
    engine.initEngine();
    engine.obliviousNeighborPass(secret_shares, start_neighbors, max_degree, 0,
                                 secret_shares.size() / (max_degree + 1),
                                 res[thread_id]);
    engine.terminateEngine();
}

void parallelObliviousNeighborSinglePassTest() {
    int num_threads = 32;
    int start_port = 12345;
    vector<thread> t_vec;
    int rand_range = 500;
    srand(time(0));

    vector<vector<uint32_t>> original_s1_map(num_threads);
    vector<vector<uint32_t>> original_s2_map(num_threads);
    vector<vector<uint32_t>> passed_neighbors1(num_threads);
    vector<vector<uint32_t>> passed_neighbors2(num_threads);
    vector<vector<uint32_t>> expected_neighbors(num_threads);

    for (int i = 0; i < num_threads; i++) {
        int max_degree = rand() % kMaxDegreeRange;
        if (max_degree == 0) {
            max_degree = 1;
        }
        vector<uint32_t> secret_share1;
        vector<uint32_t> secret_share2;
        vector<uint32_t> answer;
        generateRandSecretShareForNeighborPassing(
            max_degree, rand_range, secret_share1, secret_share2, answer);
        original_s1_map[i] = secret_share1;
        original_s2_map[i] = secret_share2;
        expected_neighbors[i] = answer;

        thread t1(&parallelObliviousNeighborSinglePassThread, i, ALICE,
                  start_port, max_degree, ref(original_s1_map[i]),
                  ref(passed_neighbors1));
        thread t2(&parallelObliviousNeighborSinglePassThread, i, BOB,
                  start_port, max_degree, ref(original_s2_map[i]),
                  ref(passed_neighbors2));
        start_port++;
        t_vec.push_back(move(t1));
        t_vec.push_back(move(t2));
    }
    for (auto &t : t_vec) {
        t.join();
    }

    for (int i = 0; i < num_threads; i++) {
        MYASSERT(passed_neighbors1[i].size() == passed_neighbors2[i].size());
        vector<uint32_t> recon_neighbors;
        for (uint64_t j = 0; j < passed_neighbors1[i].size(); j++) {
            recon_neighbors.push_back(passed_neighbors1[i][j] ^
                                      passed_neighbors2[i][j]);
        }
        MYASSERT(expected_neighbors[i].size() == recon_neighbors.size());
        cout << "num of total tuples: " << recon_neighbors.size() << endl;
        for (uint64_t j = 0; j < recon_neighbors.size(); j++) {
            if (expected_neighbors[i][j] != recon_neighbors[j]) {
                cout << "failed at " << j << " " << expected_neighbors[i][j]
                     << " " << recon_neighbors[j] << endl;
            }
            MYASSERT(expected_neighbors[i][j] == recon_neighbors[j]);
        }
        cout << "finish comparing: " << i << " succeed!\n";
    }
    cout << "experiments finished\n";
}

void createTwoSecretShare(vector<uint32_t> &input, vector<uint32_t> &s1,
                          vector<uint32_t> &s2) {
    MYASSERT(s1.empty());
    MYASSERT(s2.empty());
    for (uint64_t i = 0; i < input.size(); i++) {
        uint32_t r1 = rand() % (1 << kBitLen);
        uint32_t r2 = r1 ^ input[i];
        s1.push_back(r1);
        s2.push_back(r2);
    }
}

void createTwoSecretShare_64(vector<uint64_t> &input, vector<uint64_t> &s1,
                             vector<uint64_t> &s2) {
    MYASSERT(s1.empty());
    MYASSERT(s2.empty());
    for (uint64_t i = 0; i < input.size(); i++) {
        uint64_t r1 = ((static_cast<uint64_t>(rand()) << 32) + rand()) %
                      (1 << kTieBreakerBit);
        uint64_t r2 = r1 ^ input[i];
        s1.push_back(r1);
        s2.push_back(r2);
    }
}

void partitionTestThread(int party, int port, vector<uint32_t> &node_val_shares,
                         vector<uint32_t> &end_node_shares,
                         vector<uint64_t> &tie_breaker_shares,
                         uint32_t pivot_val_share,
                         uint32_t pivot_end_node_share,
                         uint64_t pivot_tie_breaker_share,
                         vector<uint64_t> &smaller_index,
                         vector<uint64_t> &larger_index) {
    TwoPcEngine engine(party, "127.0.0.1", port, kBitLen, kTieBreakerBit);
    engine.initEngine();

    engine.obliviousPartition(
        node_val_shares, end_node_shares, tie_breaker_shares, pivot_val_share,
        pivot_end_node_share, pivot_tie_breaker_share, smaller_index,
        larger_index, 0, 0, node_val_shares.size());

    smaller_index.clear();
    larger_index.clear();
    engine.obliviousPartition(
        node_val_shares, end_node_shares, tie_breaker_shares, pivot_val_share,
        pivot_end_node_share, pivot_tie_breaker_share, smaller_index,
        larger_index, 0, 0, node_val_shares.size());
    engine.terminateEngine();
}

void partitionTest() {
    // (e: 4, 0), (p: 4, 4), (p: 3, 3), (p: 2, 2), (e: 1, 0), (p: 1, 1)
    // (e: 2, 0), (p: 1, 1), (p: 4, 4), (e: 3, 0)
    vector<uint32_t> node_val = {4, 4, 3, 2, 1, 1, 2, 1, 4, 3};
    vector<uint32_t> end_nodes = {0, 4, 3, 2, 0, 1, 0, 1, 4, 0};
    vector<uint64_t> tie_breakers = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10};

    uint32_t pivot_val_s1 = 1;
    uint32_t pivot_val_s2 = 2;
    uint32_t pivot_end_node_s1 = 2;
    uint32_t pivot_end_node_s2 = 1;
    uint64_t pivot_tie_breaker_s1 = 1;
    uint64_t pivot_tie_breaker_s2 = 2;
    int port = 12345;
    vector<uint32_t> node_val_s1, node_val_s2, end_nodes_s1, end_nodes_s2;
    createTwoSecretShare(node_val, node_val_s1, node_val_s2);
    createTwoSecretShare(end_nodes, end_nodes_s1, end_nodes_s2);

    vector<uint64_t> tie_breaker_s1, tie_breaker_s2;
    createTwoSecretShare_64(tie_breakers, tie_breaker_s1, tie_breaker_s2);
    vector<uint64_t> sindex1, sindex2, lindex1, lindex2;

    thread t1(&partitionTestThread, ALICE, port, ref(node_val_s1),
              ref(end_nodes_s1), ref(tie_breaker_s1), pivot_val_s1,
              pivot_end_node_s1, pivot_tie_breaker_s1, ref(sindex1),
              ref(lindex1));
    thread t2(&partitionTestThread, BOB, port, ref(node_val_s2),
              ref(end_nodes_s2), ref(tie_breaker_s2), pivot_val_s2,
              pivot_end_node_s2, pivot_tie_breaker_s2, ref(sindex2),
              ref(lindex2));

    t1.join();
    t2.join();

    vector<uint64_t> s_answer = {2, 3, 4, 5, 6, 7, 9};
    vector<uint64_t> l_answer = {0, 1, 8};
    MYASSERT(sindex1.size() == sindex2.size());
    MYASSERT(sindex1.size() == s_answer.size());

    for (uint64_t i = 0; i < sindex1.size(); i++) {
        MYASSERT(sindex1[i] == sindex2[i]);
        MYASSERT(sindex1[i] == s_answer[i]);
    }

    MYASSERT(lindex1.size() == lindex2.size());
    MYASSERT(lindex1.size() == l_answer.size());
    for (uint64_t i = 0; i < lindex1.size(); i++) {
        MYASSERT(lindex1[i] == lindex2[i]);
        MYASSERT(lindex1[i] == l_answer[i]);
    }

    cout << "passed\n";
}

void sortTestThread(int party, int port, vector<uint32_t> &node_val_shares,
                    vector<uint32_t> &end_node_shares,
                    vector<uint64_t> &tie_breaker_shares,
                    vector<uint64_t> &sorted_index) {
    TwoPcEngine engine(party, "127.0.0.1", port, kBitLen, kTieBreakerBit);
    engine.initEngine();

    engine.sortElemsDirectly(node_val_shares, end_node_shares,
                             tie_breaker_shares, 0, sorted_index);

    engine.terminateEngine();
}

void sortTest() {
    // (e: 4, 0), (p: 4, 4), (p: 3, 3), (p: 2, 2), (e: 1, 0), (p: 1, 1)
    // (e: 2, 0), (p: 1, 1), (p: 4, 4), (e: 3, 0)
    vector<uint32_t> node_val = {4, 4, 3, 2, 1, 1, 2, 1, 4, 3};
    vector<uint32_t> end_nodes = {0, 4, 3, 2, 0, 1, 0, 1, 4, 0};
    vector<uint64_t> tie_breakers = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int port = 12345;
    vector<uint32_t> node_val_s1, node_val_s2, end_nodes_s1, end_nodes_s2;
    vector<uint64_t> tie_breaker_s1, tie_breaker_s2;
    createTwoSecretShare(node_val, node_val_s1, node_val_s2);
    createTwoSecretShare(end_nodes, end_nodes_s1, end_nodes_s2);
    createTwoSecretShare_64(tie_breakers, tie_breaker_s1, tie_breaker_s2);
    vector<uint64_t> sindex1, sindex2;

    thread t1(&sortTestThread, ALICE, port, ref(node_val_s1), ref(end_nodes_s1),
              ref(tie_breaker_s1), ref(sindex1));
    thread t2(&sortTestThread, BOB, port, ref(node_val_s2), ref(end_nodes_s2),
              ref(tie_breaker_s2), ref(sindex2));

    t1.join();
    t2.join();

    vector<uint64_t> answer = {4, 5, 7, 6, 3, 9, 2, 0, 1, 8};
    MYASSERT(answer.size() == sindex1.size());
    MYASSERT(answer.size() == sindex2.size());

    for (uint64_t i = 0; i < answer.size(); i++) {
        cout << sindex1[i] << " " << sindex2[i] << " " << answer[i] << endl;
        MYASSERT(answer[i] == sindex1[i]);
        MYASSERT(answer[i] == sindex2[i]);
    }
    cout << "passed\n";
}

void filterThread(vector<uint32_t> &secret_share, int party,
                  int detect_length_k, uint64_t path_num,
                  vector<bool> &is_valid, vector<bool> &is_cycle) {
    TwoPcEngine engine(party, "127.0.0.1", 12345, kBitLen, kTieBreakerBit);
    engine.initEngine();
    is_valid = vector<bool>(path_num, false);
    is_cycle = vector<bool>(path_num, false);
    engine.obliviousFilter(secret_share, 0, path_num, detect_length_k, is_valid,
                           is_cycle);
    engine.terminateEngine();
}

void filterSmallTest() {
    uint64_t path_num = 4;
    int detect_length_k = 3;
    vector<uint32_t> path1 = {2, 1, 3, 0};
    vector<uint32_t> path2 = {2, 1, 3, 1};
    vector<uint32_t> path3 = {2, 1, 3, 2};
    vector<uint32_t> path4 = {2, 1, 3, 4};

    vector<uint32_t> paths = {2, 1, 3, 0, 2, 1, 3, 1, 2, 1, 3, 2, 2, 1, 3, 4};
    vector<uint32_t> s1, s2;
    vector<bool> is_valid1, is_cycle1, is_valid2, is_cycle2;

    for (uint64_t i = 0; i < paths.size(); i++) {
        uint32_t r1 = rand() % (1 << kBitLen);
        uint32_t r2 = paths[i] ^ r1;
        s1.push_back(r1);
        s2.push_back(r2);
    }

    thread t1(&filterThread, ref(s1), ALICE, detect_length_k, path_num,
              ref(is_valid1), ref(is_cycle1));
    thread t2(&filterThread, ref(s2), BOB, detect_length_k, path_num,
              ref(is_valid2), ref(is_cycle2));
    t1.join();
    t2.join();

    MYASSERT(!is_valid1[0]);
    MYASSERT(!is_valid1[1]);
    MYASSERT(is_valid1[2]);
    MYASSERT(is_valid1[3]);
    MYASSERT(is_cycle1[2]);
    MYASSERT(!is_cycle1[3]);

    MYASSERT(!is_valid2[0]);
    MYASSERT(!is_valid2[1]);
    MYASSERT(is_valid2[2]);
    MYASSERT(is_valid2[3]);
    MYASSERT(is_cycle2[2]);
    MYASSERT(!is_cycle2[3]);

    cout << "passed\n";
}

void extractPathThread(vector<uint32_t> &secret_share, int party,
                       vector<uint64_t> &edge_tuple_indices,
                       uint64_t begin_index, uint64_t end_index) {
    TwoPcEngine engine(party, "127.0.0.1", 12345, kBitLen, kTieBreakerBit);
    engine.initEngine();
    engine.obliviousExtractPath(secret_share, edge_tuple_indices, begin_index,
                                end_index, 0, end_index - begin_index);
    engine.terminateEngine();
}

void extractPathSmallTest() {
    uint64_t begin_index = 1;
    uint64_t end_index = 6;
    vector<uint32_t> nodes = {0, 1, 2, 0, 3};
    vector<uint32_t> s1, s2;

    srand(time(0));
    for (uint64_t i = 0; i < nodes.size(); i++) {
        uint32_t r1 = rand() % (1 << kBitLen);
        uint32_t r2 = nodes[i] ^ r1;
        s1.push_back(r1);
        s2.push_back(r2);
    }

    vector<uint64_t> id1, id2;
    thread t1(&extractPathThread, ref(s1), ALICE, ref(id1), begin_index,
              end_index);
    thread t2(&extractPathThread, ref(s2), BOB, ref(id2), begin_index,
              end_index);
    t1.join();
    t2.join();

    vector<uint64_t> answer = {2, 3, 5};

    MYASSERT(id1.size() == answer.size());
    MYASSERT(id2.size() == answer.size());
    for (uint64_t i = 0; i < answer.size(); i++) {
        MYASSERT(id1[i] == answer[i]);
        MYASSERT(id2[i] == answer[i]);
    }

    cout << "passed\n";
}

int main(int argc, char **argv) {
    int run_id = 0;
    if (argc >= 2) {
        run_id = atoi(argv[1]);
    }
    switch (run_id) {
        case 1: {
            cout << "run parallel oblivious neighbor passing\n";
            parallelObliviousNeighborSinglePassTest();
            break;
        }
        case 2: {
            cout << "run partition test\n";
            partitionTest();
            break;
        }
        case 3: {
            cout << "run sort small elements test\n";
            sortTest();
            break;
        }
        case 4: {
            cout << "run filterSmallTest\n";
            filterSmallTest();
            break;
        }
        case 5: {
            cout << "run extractPathSmallTest\n";
            extractPathSmallTest();
            break;
        }
        default:
            break;
    }
    return 0;
}