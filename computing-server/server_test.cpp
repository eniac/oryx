#include "server.hpp"

// const int kBitLen = 23;
const int kMaxDegreeRange = 20;
const int kRandRange = 200;
const int kMinRandRange = 200;
const int kMinMaxDegree = 10;

// const int kNumMedian = 7;

uint64_t kNodeNum = (1 << 15);
uint64_t kNodeDegree = (1 << 3);

// const int kTieBreakerBit = 25;
// const uint64_t kTieBreakerRange = (1 << kTieBreakerBit);

void loadGraphTest() {
    uint32_t id_range = (1 << kBitLen);
    int bit_len = kBitLen;
    SecretShareTupleHelper helper(id_range, kTieBreakerRange);
    int max_degree = 10;
    int detect_length_k = 2;
    int total_detect_length = 4;
    // int node_id = 1;
    int parallel_num = 8;
    int seed12 = 1;
    int seed23 = 2;
    int seed31 = 3;
    int seedA = 4;
    int seedB = 5;
    string ip_1 = "127.0.0.1:5555";
    string ip_2 = "127.0.0.1:5555";
    string ip_3 = "127.0.0.1:5555";
    uint64_t batch_process_num = 0;

    NetServer server1(
        SERVER_ID::S1, id_range, bit_len, max_degree, detect_length_k,
        total_detect_length, parallel_num, parallel_num, parallel_num,
        /*filter_parallel_num=*/parallel_num,
        /*extract_path_parallel_num=*/parallel_num,
        /*sort_elem_threshold=*/1, /*num_to_find_median=*/7,
        /*seed12_permute=*/seed12,
        /*seed23_permute=*/0,
        /*seed31_permute=*/seed31, /*seedA=*/seedA,
        /*seedB=*/seedB, ip_1, ip_2, ip_3, batch_process_num, batch_process_num,
        batch_process_num, batch_process_num, kTieBreakerBit);
    NetServer server2(SERVER_ID::S2, id_range, bit_len, max_degree,
                      detect_length_k, total_detect_length, parallel_num,
                      parallel_num, parallel_num,
                      /*filter_parallel_num=*/parallel_num,
                      /*extract_path_parallel_num=*/parallel_num,
                      /*sort_elem_threshold=*/1,
                      /*num_to_find_median=*/7,
                      /*seed12_permute=*/seed12,
                      /*seed23_permute=*/seed23,
                      /*seed31_permute=*/0, /*seedA=*/0, /*seedB=*/seedB, ip_1,
                      ip_2, ip_3, batch_process_num, batch_process_num,
                      batch_process_num, batch_process_num, kTieBreakerBit);
    NetServer server3(SERVER_ID::S3, id_range, bit_len, max_degree,
                      detect_length_k, total_detect_length, parallel_num,
                      parallel_num, parallel_num,
                      /*filter_parallel_num=*/parallel_num,
                      /*extract_path_parallel_num=*/parallel_num,
                      /*sort_elem_threshold=*/1, /*num_to_find_median=*/7,
                      /*seed12_permute=*/0,
                      /*seed23_permute=*/seed23,
                      /*seed31_permute=*/seed31, /*seedA=*/seedA, /*seedB=*/0,
                      ip_1, ip_2, ip_3, batch_process_num, batch_process_num,
                      batch_process_num, batch_process_num, kTieBreakerBit);

    server1.loadSecretShare("/users/kezhon0/cycle-codes/demo-graph/out1_small",
                            "/users/kezhon0/cycle-codes/demo-graph/out2_small");
    server2.loadSecretShare("/users/kezhon0/cycle-codes/demo-graph/out2_small",
                            "/users/kezhon0/cycle-codes/demo-graph/out3_small");
    server3.loadSecretShare("/users/kezhon0/cycle-codes/demo-graph/out3_small",
                            "/users/kezhon0/cycle-codes/demo-graph/out1_small");
    vector<ServerSecretShares *> s_vec1 = {
        server1.getFirstServerSecretShares(),
        server2.getFirstServerSecretShares(),
        server3.getFirstServerSecretShares()};
    unique_ptr<ServerSecretShares> recon1 =
        helper.reconServerShares(s_vec1, max_degree, detect_length_k);

    vector<ServerSecretShares *> s_vec2 = {
        server1.getSecondServerSecretShares(),
        server2.getSecondServerSecretShares(),
        server3.getSecondServerSecretShares()};
    unique_ptr<ServerSecretShares> recon2 =
        helper.reconServerShares(s_vec2, max_degree, detect_length_k);
    cout << recon1->svec.size() << endl;
    cout << recon2->svec.size() << endl;
    cout << recon1->size() << endl;
    MYASSERT(*recon1.get() == *recon2.get());
    // bool cmp = (*recon1.get() == *recon2.get());
    // cout << "cmp res: " << cmp << endl;
    recon1->print();
}

void generateRandSecretShareForNeighborPassing(
    int max_degree, int detect_length_k, unique_ptr<ServerSecretShares> &s1,
    unique_ptr<ServerSecretShares> &s2,
    unique_ptr<ServerSecretShares> &anwser) {
    SecretShareTupleHelper helper((1 << kBitLen), kTieBreakerRange);
    MYASSERT(s1->size() == 0);
    MYASSERT(s2->size() == 0);
    MYASSERT(anwser->size() == 0);
    int rand_nodes_num = kNodeNum;
    // cout << "max_degree: " << max_degree << endl;
    // cout << "node num: " << rand_nodes_num << endl;
    for (int i = 0; i < rand_nodes_num; i++) {
        // node id cannot be zero so set to i+1
        int node_id = (i + 1) % (1 << kBitLen);
        if (node_id == 0) {
            node_id = 1;
        }

        vector<uint32_t> current_neighbors;

        // create neighbors for the node_id.
        int rand_degree = rand() % max_degree;
        for (int j = 0; j < rand_degree; j++) {
            uint32_t neighbor_val = rand() % (1 << kBitLen);
            current_neighbors.push_back(neighbor_val);
            // cout << " current neighbor " << j << " is " << neighbor_val <<
            // endl;
        }

        unique_ptr<SecretShareTuple> et = helper.formatEdgeTuple(
            node_id, current_neighbors, max_degree, detect_length_k);

        // assign tie_breaker_id
        et->set_tie_breaker_id(i);

        vector<unique_ptr<SecretShareTuple>> ets = helper.splitTupleIntoTwo(et);
        s1->insert(ets[0]);
        s2->insert(ets[1]);

        anwser->insert(et);

        // pad zeroes to get answer
        for (int i = current_neighbors.size(); i < max_degree; i++) {
            current_neighbors.push_back(0);
        }

        // create rand_tuple_num of the current node id.
        int rand_tuple_num = kNodeDegree;
        // cout << "rand tuple num: " << rand_tuple_num << endl;
        for (int j = 0; j < rand_tuple_num; j++) {
            // need to be max_degree+1 as there needs a node id as well.
            vector<uint32_t> path;
            for (int k = 0; k < detect_length_k; k++) {
                path.push_back(rand() % (1 << kBitLen));
            }
            unique_ptr<SecretShareTuple> pt =
                helper.formatPathTuple(path, max_degree, detect_length_k);
            vector<unique_ptr<SecretShareTuple>> pts =
                helper.splitTupleIntoTwo(pt);
            s1->insert(pts[0]);
            s2->insert(pts[1]);

            pt->updateNeighbors(current_neighbors);
            anwser->insert(pt);
        }
    }
}

void runServerNeighborPassThread(unique_ptr<ServerSecretShares> &ss, string ip,
                                 int party, int neighbor_pass_parallel_num,
                                 int max_degree, int detect_length_k,
                                 uint64_t batch_process_num) {
    uint32_t id_range = (1 << kBitLen);
    int bit_len = kBitLen;
    // int node_id = 1;
    int parallel_num = 8;
    int seed12 = 1;
    // int seed23 = 2;
    int seed31 = 3;
    int seedA = 4;
    int seedB = 5;
    string ip_1 = "127.0.0.1:5555";
    string ip_2 = "127.0.0.1:5555";
    string ip_3 = "127.0.0.1:5555";
    int total_detect_length = 100;

    NetServer server(SERVER_ID::S1, id_range, bit_len, max_degree,
                     detect_length_k, total_detect_length, parallel_num,
                     neighbor_pass_parallel_num, neighbor_pass_parallel_num,
                     /*filter_parallel_num=*/parallel_num,
                     /*extract_path_parallel_num=*/parallel_num,
                     /*sort_elem_threshold=*/1,
                     /*num_to_find_median=*/7,
                     /*seed12_permute=*/seed12,
                     /*seed23_permute=*/0,
                     /*seed31_permute=*/seed31, /*seedA=*/seedA,
                     /*seedB=*/seedB, ip_1, ip_2, ip_3, batch_process_num,
                     batch_process_num, batch_process_num, batch_process_num,
                     kTieBreakerBit);
    cout << "secret share size: " << ss->size() << endl;
    cout << "max_degree: " << max_degree << endl;

    cout << "party " << party << " started computing\n";
    server.obliviousNeighborPass(ss, "127.0.0.1", party);
    cout << "party " << party << " finished computing\n";
}

void serverToServerNeighborPassTest(int detect_length_k, int max_degree) {
    srand(time(0));
    int neighbor_pass_parallel_num = 16;
    vector<uint64_t> batch_process_nums = {
        (1 << 2),  (1 << 6),  (1 << 7),  (1 << 8),  (1 << 9),
        (1 << 10), (1 << 11), (1 << 12), (1 << 14), 0};
    for (uint64_t i = 0; i < batch_process_nums.size(); i++) {
        unique_ptr<ServerSecretShares> s1 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> s2 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> answer =
            make_unique<ServerSecretShares>();
        generateRandSecretShareForNeighborPassing(max_degree, detect_length_k,
                                                  s1, s2, answer);

        // for (uint64_t i = 0; i < secret_share1.size(); i++) {
        //     cout << "[before] s1 " << i << " " << secret_share1[i] << endl;
        //     cout << "[before] s2 " << i << " " << secret_share2[i] << endl;
        // }

        // for (uint64_t i = 0; i < anwser.size(); i++) {
        //     cout << "anwser " << i << " " << anwser[i] << endl;
        // }
        auto start = system_clock::now();

        thread t1(&runServerNeighborPassThread, ref(s1), "127.0.0.1", ALICE,
                  neighbor_pass_parallel_num, max_degree, detect_length_k,
                  batch_process_nums[i]);

        thread t2(&runServerNeighborPassThread, ref(s2), "127.0.0.1", BOB,
                  neighbor_pass_parallel_num, max_degree, detect_length_k,
                  batch_process_nums[i]);

        t1.join();
        t2.join();

        // for (uint64_t i = 0; i < secret_share1.size(); i++) {
        //     cout << "[after] s1 " << i << " " << secret_share1[i] << endl;
        //     cout << "[after] s2 " << i << " " << secret_share2[i] << endl;
        // }

        // MYASSERT(anwser.size() == secret_share1.size());
        // for (uint64_t id = 0; id < anwser.size(); id++) {
        //     uint32_t recon = secret_share1[id] ^ secret_share2[id];
        //     if (recon != anwser[id]) {
        //         cout << id << " is not same" << endl;
        //         cout << "recon: " << recon << endl;
        //         cout << "anwser: " << anwser[id] << endl;
        //     }
        //     MYASSERT(recon == anwser[id]);
        // }
        auto end = system_clock::now();

        SecretShareTupleHelper helper((1 << kBitLen), kTieBreakerRange);
        unique_ptr<ServerSecretShares> res = make_unique<ServerSecretShares>();
        for (uint64_t i = 0; i < answer->size(); i++) {
            vector<SecretShareTuple *> recon_vec = {s1->svec[i].get(),
                                                    s2->svec[i].get()};
            unique_ptr<SecretShareTuple> rt = helper.reconTupleFromTwo(
                recon_vec, max_degree, detect_length_k);
            res->insert(rt);
        }

        // cout << "print res\n";
        // res->print();

        // cout << "print answer\n";
        // answer->print();
        MYASSERT(*res.get() == *answer.get());
        cout << "passed\n";
        cout << "finished server to server neighbor passing test, round: " << i
             << endl;
        cout << "Tested " << answer->size() << " elements, "
             << "max degree: " << max_degree << endl;
        cout
            << "Takes "
            << duration_cast<std::chrono::duration<double>>(end - start).count()
            << " seconds, with batch process num: " << batch_process_nums[i]
            << endl;
    }
}

// void sortTestThreadSlow(unique_ptr<ServerSecretShares> &ss, int party,
//                         int detect_length_k, int max_degree, int
//                         parallel_num, uint64_t sort_elem_threshold) {
//     uint32_t id_range = (1 << kBitLen);
//     int bit_len = kBitLen;
//     int seed12 = 1;
//     int seed31 = 3;
//     int seedA = 4;
//     int seedB = 5;
//     string ip_1 = "127.0.0.1:5555";
//     string ip_2 = "127.0.0.1:5555";
//     string ip_3 = "127.0.0.1:5555";
//     int total_detect_length = 100;
//     uint64_t batch_process_num = 0;

//     NetServer server(
//         SERVER_ID::S1, id_range, bit_len, max_degree, detect_length_k,
//         total_detect_length, parallel_num, parallel_num, parallel_num,
//         /*filter_parallel_num=*/parallel_num,
//         /*extract_path_parallel_num=*/parallel_num, sort_elem_threshold,
//         /*num_to_find_median=*/7,
//         /*seed12_permute=*/seed12,
//         /*seed23_permute=*/0,
//         /*seed31_permute=*/seed31, /*seedA=*/seedA,
//         /*seedB=*/seedB, ip_1, ip_2, ip_3, batch_process_num,
//         batch_process_num, batch_process_num);
//     server.obliviousSortSlow(ss, 0, ss->size(), "127.0.0.1", party);
// }

void sortTestThread(unique_ptr<ServerSecretShares> &ss, int party,
                    int detect_length_k, int max_degree, int parallel_num,
                    uint64_t sort_elem_threshold, uint64_t batch_process_num) {
    uint32_t id_range = (1 << kBitLen);
    int bit_len = kBitLen;
    int seed12 = 1;
    int seed31 = 3;
    int seedA = 4;
    int seedB = 5;
    string ip_1 = "127.0.0.1:5555";
    string ip_2 = "127.0.0.1:5555";
    string ip_3 = "127.0.0.1:5555";
    int total_detect_length = 100;

    NetServer server(
        SERVER_ID::S1, id_range, bit_len, max_degree, detect_length_k,
        total_detect_length, parallel_num, parallel_num, parallel_num,
        /*filter_parallel_num=*/parallel_num,
        /*extract_path_parallel_num=*/parallel_num, sort_elem_threshold,
        /*num_to_find_median=*/kNumMedian,
        /*seed12_permute=*/seed12,
        /*seed23_permute=*/0,
        /*seed31_permute=*/seed31, /*seedA=*/seedA,
        /*seedB=*/seedB, ip_1, ip_2, ip_3, batch_process_num, batch_process_num,
        batch_process_num, batch_process_num, kTieBreakerBit);
    server.obliviousSort(ss, "127.0.0.1", party);
}

void endToEndSortSmallTest() {
    uint32_t id_range = (1 << kBitLen);
    int detect_length_k = 2;
    int max_degree = 3;
    int parallel_num = 2;
    uint64_t sort_elem_threshold = 0;

    SecretShareTupleHelper helper(id_range, kTieBreakerRange);

    // input tuples:
    // 0: (1,2)
    // 0: (3,1)
    // edge 2: (2,3,4)
    // 0: (5,1)
    // edge 1: (1,2)
    // 0: (3,2)

    // vector<uint32_t> p1 = {1, 2};
    unique_ptr<SecretShareTuple> t1 =
        helper.formatPathTuple({1, 2}, max_degree, detect_length_k);
    t1->set_tie_breaker_id(1);

    unique_ptr<SecretShareTuple> t2 =
        helper.formatPathTuple({3, 1}, max_degree, detect_length_k);
    t2->set_tie_breaker_id(2);

    unique_ptr<SecretShareTuple> t3 =
        helper.formatEdgeTuple(2, {2, 3, 4}, max_degree, detect_length_k);
    t3->set_tie_breaker_id(3);

    unique_ptr<SecretShareTuple> t4 =
        helper.formatPathTuple({5, 1}, max_degree, detect_length_k);
    t4->set_tie_breaker_id(4);

    unique_ptr<SecretShareTuple> t5 =
        helper.formatEdgeTuple(1, {1, 2}, max_degree, detect_length_k);
    t5->set_tie_breaker_id(5);

    unique_ptr<SecretShareTuple> t6 =
        helper.formatPathTuple({3, 2}, max_degree, detect_length_k);
    t6->set_tie_breaker_id(6);

    unique_ptr<ServerSecretShares> s1 = make_unique<ServerSecretShares>();
    unique_ptr<ServerSecretShares> s2 = make_unique<ServerSecretShares>();

    t1->print();
    t2->print();
    t3->print();
    t4->print();
    t5->print();
    t6->print();

    vector<unique_ptr<SecretShareTuple>> ts1 = helper.splitTupleIntoTwo(t1);
    vector<unique_ptr<SecretShareTuple>> ts2 = helper.splitTupleIntoTwo(t2);
    vector<unique_ptr<SecretShareTuple>> ts3 = helper.splitTupleIntoTwo(t3);
    vector<unique_ptr<SecretShareTuple>> ts4 = helper.splitTupleIntoTwo(t4);
    vector<unique_ptr<SecretShareTuple>> ts5 = helper.splitTupleIntoTwo(t5);
    vector<unique_ptr<SecretShareTuple>> ts6 = helper.splitTupleIntoTwo(t6);

    s1->insert(ts1[0]);
    s1->insert(ts2[0]);
    s1->insert(ts3[0]);
    s1->insert(ts4[0]);
    s1->insert(ts5[0]);
    s1->insert(ts6[0]);

    s2->insert(ts1[1]);
    s2->insert(ts2[1]);
    s2->insert(ts3[1]);
    s2->insert(ts4[1]);
    s2->insert(ts5[1]);
    s2->insert(ts6[1]);

    thread thread1(&sortTestThread, ref(s1), ALICE, detect_length_k, max_degree,
                   parallel_num, sort_elem_threshold, 0);
    thread thread2(&sortTestThread, ref(s2), BOB, detect_length_k, max_degree,
                   parallel_num, sort_elem_threshold, 0);

    thread1.join();
    thread2.join();

    unique_ptr<ServerSecretShares> sorted = make_unique<ServerSecretShares>();
    for (uint64_t i = 0; i < s1->size(); i++) {
        vector<SecretShareTuple *> recon_vec = {s1->svec[i].get(),
                                                s2->svec[i].get()};
        unique_ptr<SecretShareTuple> r =
            helper.reconTupleFromTwo(recon_vec, max_degree, detect_length_k);
        sorted->insert(r);
    }

    cout << "sorted\n";
    sorted->print();

    // unique_ptr<ServerSecretShares> anwser =
    // make_unique<ServerSecretShares>(); anwser->insert(t1);
    // anwser->insert(t2);
    // anwser->insert(t3);
    // anwser->insert(t4);
    // anwser->insert(t5);
    // anwser->insert(t6);

    // cout << "before sort\n";
    // anwser->print();
    // cout << "finish print before sort\n";

    // // MYASSERT(!(*anwser.get() == *sorted.get()));

    // anwser->sortTuple();
    // cout << "print anwser\n";
    // anwser->print();
    // MYASSERT(*anwser.get() == *sorted.get());

    MYASSERT(sorted->checkSorted());
    cout << "passed equality test\n";
}

void createRandomTuples(int max_degree, int rand_range, int detect_length_k,
                        unique_ptr<ServerSecretShares> &s1,
                        unique_ptr<ServerSecretShares> &s2,
                        unique_ptr<ServerSecretShares> &sorted) {
    // // for debugging only
    // rand_nodes_num = 2;

    SecretShareTupleHelper helper((1 << kBitLen), kTieBreakerRange);

    uint64_t rand_nodes_num = kNodeNum;
    cout << "using " << rand_nodes_num << " nodes in total\n";
    for (uint32_t node_id = 1; node_id <= rand_nodes_num; node_id++) {
        uint64_t rand_path_num = kNodeDegree;

        // cout << node_id << " has " << rand_path_num << " paths\n";
        // create edge tuple first
        vector<uint32_t> edges;
        int rand_edge_num = rand() % (max_degree + 1);
        for (int i = 0; i < rand_edge_num; i++) {
            uint32_t rand_neighbor = rand() % rand_nodes_num;
            if (rand_neighbor == 0) {
                rand_neighbor = 1;
            }
            edges.push_back(rand_neighbor);
        }
        unique_ptr<SecretShareTuple> et =
            helper.formatEdgeTuple(node_id, edges, max_degree, detect_length_k);
        et->set_tie_breaker_id((node_id - 1) * rand_path_num);

        vector<unique_ptr<SecretShareTuple>> ets = helper.splitTupleIntoTwo(et);
        s1->insert(ets[0]);
        s2->insert(ets[1]);

        sorted->insert(et);

        for (uint64_t i = 0; i < rand_path_num; i++) {
            vector<uint32_t> path;
            for (int i = 0; i < detect_length_k - 1; i++) {
                uint32_t rand_neighbor = rand() % rand_nodes_num;
                if (rand_neighbor == 0) {
                    rand_neighbor = 1;
                }
                path.push_back(rand_neighbor);
            }
            path.push_back(node_id);

            unique_ptr<SecretShareTuple> t =
                helper.formatPathTuple(path, max_degree, detect_length_k);
            t->set_tie_breaker_id((node_id - 1) * rand_path_num + i);
            vector<unique_ptr<SecretShareTuple>> ts =
                helper.splitTupleIntoTwo(t);
            s1->insert(ts[0]);
            s2->insert(ts[1]);
            sorted->insert(t);
        }
    }
}

void sortEndToEndTest(uint64_t sort_elem_threshold) {
    uint32_t id_range = (1 << kBitLen);
    int rand_range = kRandRange;
    int detect_length_k = 2;
    int parallel_num = 16;
    int seedA = 4;
    int seedB = 5;
    int max_degree = 10;

    Server server(SERVER_ID::S1, id_range, max_degree, detect_length_k,
                  parallel_num, /*seed12_permute=*/1,
                  /*seed23_permute=*/0,
                  /*seed31_permute=*/3, /*seedA=*/seedA, /*seedB=*/seedB,
                  kTieBreakerRange);
    SecretShareTupleHelper helper((1 << kBitLen), kTieBreakerRange);

    srand(time(0));

    vector<uint64_t> batch_process_nums = {
        (1 << 2),  (1 << 6),  (1 << 7),  (1 << 8),  (1 << 9),
        (1 << 10), (1 << 11), (1 << 12), (1 << 14), 0};

    for (uint64_t round = 0; round < batch_process_nums.size(); round++) {
        unique_ptr<ServerSecretShares> s1 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> s2 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> sorted =
            make_unique<ServerSecretShares>();

        createRandomTuples(max_degree, rand_range, detect_length_k, s1, s2,
                           sorted);

        int shuffle_seed = 10;
        server.shuffleSecretShares(*s1.get(), shuffle_seed);
        server.shuffleSecretShares(*s2.get(), shuffle_seed);

        auto start = system_clock::now();
        thread thread1(&sortTestThread, ref(s1), ALICE, detect_length_k,
                       max_degree, parallel_num, sort_elem_threshold,
                       batch_process_nums[round]);
        thread thread2(&sortTestThread, ref(s2), BOB, detect_length_k,
                       max_degree, parallel_num, sort_elem_threshold,
                       batch_process_nums[round]);

        thread1.join();
        thread2.join();
        auto end = system_clock::now();

        cout
            << round << " round takes "
            << duration_cast<std::chrono::duration<double>>(end - start).count()
            << " seconds to sort " << s1->size()
            << " tuples, with batch num: " << batch_process_nums[round] << endl;

        unique_ptr<ServerSecretShares> res = make_unique<ServerSecretShares>();
        for (uint64_t i = 0; i < s1->size(); i++) {
            vector<SecretShareTuple *> recon_vec = {s1->svec[i].get(),
                                                    s2->svec[i].get()};
            unique_ptr<SecretShareTuple> r = helper.reconTupleFromTwo(
                recon_vec, max_degree, detect_length_k);
            res->insert(r);
        }

        // cout << "original val\n";
        // sorted->print();

        // cout << "res val\n";
        // res->print();
        MYASSERT(res->checkSorted());
        cout << "passed equaty check test\n";

        // cout << "before shuffling\n";
        // sorted->print();
        // cout << "end output before shuffling\n";

        // server.shuffleSecretShares(*sorted.get(), shuffle_seed);

        // cout << "after shuffling\n";
        // sorted->print();
        // cout << "end output after shuffling\n";

        // cout << "output res\n";
        // res->print();
        // cout << "finish output res\n";

        // sorted->sortTuple();
        // cout << "after stable sort\n";
        // sorted->print();
        // cout << "end output after stable sort\n";

        // bool equal = (*res.get() == *sorted.get());
        // MYASSERT(equal);
    }
}

void createPathForFilter(int detect_length_k, uint64_t path_num,
                         vector<bool> &is_valid_vec, vector<bool> &is_cycle_vec,
                         unique_ptr<ServerSecretShares> &path_tuples,
                         unique_ptr<ServerSecretShares> &s1,
                         unique_ptr<ServerSecretShares> &s2) {
    SecretShareTupleHelper helper((1 << kBitLen), kTieBreakerRange);
    int max_degree = 1;
    for (uint64_t i = 0; i < path_num; i++) {
        vector<uint32_t> path;
        for (int i = 0; i < detect_length_k; i++) {
            uint32_t node = rand() % ((1 << kBitLen) - 1) + 1;
            while (find(path.begin(), path.end(), node) != path.end()) {
                node = rand() % ((1 << kBitLen) - 1) + 1;
            }
            path.push_back(node);
        }
        // randomize last elements
        bool is_valid = rand() % 2;
        bool is_cycle = rand() % 2;
        is_valid_vec.push_back(is_valid);
        is_cycle_vec.push_back(is_cycle);
        if (!is_valid) {
            int repeat_node_id = rand() % (detect_length_k) + 1;
            if (repeat_node_id == detect_length_k) {
                path.push_back(0);
            } else {
                path.push_back(path[repeat_node_id]);
            }
        } else if (is_cycle) {
            path.push_back(path[0]);
        } else {
            // valid but non-cycle
            uint32_t last_node = rand() % ((1 << kBitLen) - 1) + 1;
            // if last node repeats with existing nodes
            while (find(path.begin(), path.end(), last_node) != path.end()) {
                last_node = rand() % ((1 << kBitLen) - 1) + 1;
            }
            path.push_back(last_node);
        }
        unique_ptr<SecretShareTuple> pt =
            helper.formatFilledPathTuple(path, max_degree, detect_length_k);
        pt->set_tie_breaker_id(i);

        vector<unique_ptr<SecretShareTuple>> pts = helper.splitTupleIntoTwo(pt);
        path_tuples->insert(pt);
        s1->insert(pts[0]);
        s2->insert(pts[1]);
    }
}

void filterTestThread(uint64_t batch_process_num, int detect_length_k,
                      unique_ptr<ServerSecretShares> &ss, int party,
                      vector<bool> &is_valid, vector<bool> &is_cycle) {
    uint32_t id_range = (1 << kBitLen);
    int bit_len = kBitLen;
    int seed12 = 1;
    int seed31 = 3;
    int seedA = 4;
    int seedB = 5;
    string ip_1 = "127.0.0.1:5555";
    string ip_2 = "127.0.0.1:5555";
    string ip_3 = "127.0.0.1:5555";
    int max_degree = 10;
    int parallel_num = 16;
    int total_detect_length = 100;

    NetServer server(
        SERVER_ID::S1, id_range, bit_len, max_degree, detect_length_k,
        total_detect_length, parallel_num, parallel_num, parallel_num,
        /*filter_parallel_num=*/parallel_num,
        /*extract_path_parallel_num=*/parallel_num,
        /*sort_elem_threshold=*/7,
        /*num_to_find_median=*/kNumMedian,
        /*seed12_permute=*/seed12,
        /*seed23_permute=*/0,
        /*seed31_permute=*/seed31, /*seedA=*/seedA,
        /*seedB=*/seedB, ip_1, ip_2, ip_3, batch_process_num, batch_process_num,
        batch_process_num, batch_process_num, kTieBreakerBit);
    server.oblivousFilter(ss, "127.0.0.1", party, is_valid, is_cycle);
}

void filterTest(uint64_t path_num, int detect_length_k) {
    vector<uint64_t> batch_process_nums = {
        (1 << 2),  (1 << 6),  (1 << 7),  (1 << 8),  (1 << 9),
        (1 << 10), (1 << 11), (1 << 12), (1 << 14), 0};
    for (int round = 0; round < static_cast<int>(batch_process_nums.size());
         round++) {
        vector<bool> is_valid, is_cycle;
        unique_ptr<ServerSecretShares> path_tuples =
            make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> s1 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> s2 = make_unique<ServerSecretShares>();

        createPathForFilter(detect_length_k, path_num, is_valid, is_cycle,
                            path_tuples, s1, s2);

        auto start = system_clock::now();
        vector<bool> is_valid1, is_valid2, is_cycle1, is_cycle2;
        thread t1(&filterTestThread, batch_process_nums[round], detect_length_k,
                  ref(s1), ALICE, ref(is_valid1), ref(is_cycle1));
        thread t2(&filterTestThread, batch_process_nums[round], detect_length_k,
                  ref(s2), BOB, ref(is_valid2), ref(is_cycle2));

        t1.join();
        t2.join();
        auto end = system_clock::now();

        MYASSERT(is_valid1.size() == is_valid.size());
        MYASSERT(is_cycle1.size() == is_cycle.size());
        MYASSERT(is_valid2.size() == is_valid.size());
        MYASSERT(is_cycle2.size() == is_cycle.size());

        for (uint64_t i = 0; i < is_valid.size(); i++) {
            if (is_valid1[i] != is_valid[i]) {
                cout << i << " " << is_valid1[i] << " " << is_valid[i] << endl;
                path_tuples->svec[i]->print();
            }
            MYASSERT(is_valid1[i] == is_valid[i]);
            MYASSERT(is_valid2[i] == is_valid[i]);
            if (is_valid[i]) {
                MYASSERT(is_cycle[i] == is_cycle1[i]);
                MYASSERT(is_cycle[i] == is_cycle2[i]);
            }
        }
        cout
            << round << " round takes "
            << duration_cast<std::chrono::duration<double>>(end - start).count()
            << " seconds to filter " << path_num << " paths"
            << " batch process num: " << batch_process_nums[round] << endl;
    }
    cout << "passed filter test\n";
}

void createTuplesForExtract(int detect_length_k, int max_degree,
                            unique_ptr<ServerSecretShares> &edges,
                            unique_ptr<ServerSecretShares> &paths,
                            unique_ptr<ServerSecretShares> &s1,
                            unique_ptr<ServerSecretShares> &s2) {
    SecretShareTupleHelper helper((1 << kBitLen), kTieBreakerRange);

    int rand_nodes_num = kNodeNum;
    // cout << "max_degree: " << max_degree << endl;
    // cout << "node num: " << rand_nodes_num << endl;
    for (int i = 0; i < rand_nodes_num; i++) {
        // node id cannot be zero so set to i+1
        int node_id = (i + 1) % (1 << kBitLen);
        if (node_id == 0) {
            node_id = 1;
        }

        vector<uint32_t> current_neighbors;

        // create neighbors for the node_id.
        int rand_degree = rand() % max_degree;
        for (int j = 0; j < rand_degree; j++) {
            uint32_t neighbor_val = rand() % (1 << kBitLen);
            current_neighbors.push_back(neighbor_val);
            // cout << " current neighbor " << j << " is " << neighbor_val <<
            // endl;
        }

        unique_ptr<SecretShareTuple> et = helper.formatEdgeTuple(
            node_id, current_neighbors, max_degree, detect_length_k);

        et->set_tie_breaker_id(i * kNodeDegree);

        vector<unique_ptr<SecretShareTuple>> ets = helper.splitTupleIntoTwo(et);
        s1->insert(ets[0]);
        s2->insert(ets[1]);

        edges->insert(et);

        // create rand_tuple_num of the current node id.
        int rand_tuple_num = kNodeDegree;
        // cout << "rand tuple num: " << rand_tuple_num << endl;
        for (int j = 0; j < rand_tuple_num; j++) {
            // need to be max_degree+1 as there needs a node id as well.
            vector<uint32_t> path;
            for (int k = 0; k < detect_length_k; k++) {
                path.push_back(rand() % (1 << kBitLen));
            }
            unique_ptr<SecretShareTuple> pt =
                helper.formatPathTuple(path, max_degree, detect_length_k);
            pt->set_tie_breaker_id(i * kNodeDegree + j);
            vector<unique_ptr<SecretShareTuple>> pts =
                helper.splitTupleIntoTwo(pt);
            s1->insert(pts[0]);
            s2->insert(pts[1]);

            paths->insert(pt);
        }
    }
}

void extractTestThread(unique_ptr<ServerSecretShares> &ss, int party,
                       int detect_length_k, int max_degree, int parallel_num,
                       unique_ptr<ServerSecretShares> &edge_shares,
                       unique_ptr<ServerSecretShares> &path_shares,
                       uint64_t batch_process_num) {
    uint32_t id_range = (1 << kBitLen);
    int bit_len = kBitLen;
    int seed12 = 1;
    int seed31 = 3;
    int seedA = 4;
    int seedB = 5;
    string ip_1 = "127.0.0.1:5555";
    string ip_2 = "127.0.0.1:5555";
    string ip_3 = "127.0.0.1:5555";
    int total_detect_length = 100;

    NetServer server(
        SERVER_ID::S1, id_range, bit_len, max_degree, detect_length_k,
        total_detect_length, parallel_num, parallel_num, parallel_num,
        /*filter_parallel_num=*/parallel_num,
        /*extract_path_parallel_num=*/parallel_num,
        /*sort_elem_threshold=*/1,
        /*num_to_find_median=*/kNumMedian,
        /*seed12_permute=*/seed12,
        /*seed23_permute=*/0,
        /*seed31_permute=*/seed31, /*seedA=*/seedA,
        /*seedB=*/seedB, ip_1, ip_2, ip_3, batch_process_num, batch_process_num,
        batch_process_num, batch_process_num, kTieBreakerBit);
    server.obliviousExtractPathAndEdges(ss, "127.0.0.1", party, edge_shares,
                                        path_shares);
}

void extractTest() {
    srand(time(0));
    int detect_length_k = 3;
    int max_degree = 10;
    int parallel_num = 16;

    vector<uint64_t> batch_process_nums = {
        (1 << 2),  (1 << 6),  (1 << 7),  (1 << 8),  (1 << 9),
        (1 << 10), (1 << 11), (1 << 12), (1 << 14), 0};
    for (uint64_t round = 0; round < batch_process_nums.size(); round++) {
        unique_ptr<ServerSecretShares> edges =
            make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> paths =
            make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> s1 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> s2 = make_unique<ServerSecretShares>();
        createTuplesForExtract(detect_length_k, max_degree, edges, paths, s1,
                               s2);

        unique_ptr<ServerSecretShares> es1 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> es2 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> ps1 = make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> ps2 = make_unique<ServerSecretShares>();

        auto start = system_clock::now();
        thread t1(&extractTestThread, ref(s1), ALICE, detect_length_k,
                  max_degree, parallel_num, ref(es1), ref(ps1),
                  batch_process_nums[round]);
        thread t2(&extractTestThread, ref(s2), BOB, detect_length_k, max_degree,
                  parallel_num, ref(es2), ref(ps2), batch_process_nums[round]);

        t1.join();
        t2.join();
        auto end = system_clock::now();

        MYASSERT(es1->size() == edges->size());
        MYASSERT(es2->size() == edges->size());
        MYASSERT(ps1->size() == paths->size());
        MYASSERT(ps2->size() == paths->size());

        SecretShareTupleHelper helper((1 << kBitLen), kTieBreakerRange);
        unique_ptr<ServerSecretShares> recon_edges =
            make_unique<ServerSecretShares>();
        for (uint64_t i = 0; i < edges->size(); i++) {
            vector<SecretShareTuple *> recon_vec = {es1->svec[i].get(),
                                                    es2->svec[i].get()};
            unique_ptr<SecretShareTuple> rt = helper.reconTupleFromTwo(
                recon_vec, max_degree, detect_length_k);
            recon_edges->insert(rt);
        }

        unique_ptr<ServerSecretShares> recon_paths =
            make_unique<ServerSecretShares>();
        for (uint64_t i = 0; i < paths->size(); i++) {
            vector<SecretShareTuple *> recon_vec = {ps1->svec[i].get(),
                                                    ps2->svec[i].get()};
            unique_ptr<SecretShareTuple> rt = helper.reconTupleFromTwo(
                recon_vec, max_degree, detect_length_k);
            recon_paths->insert(rt);
        }

        MYASSERT(*edges.get() == *recon_edges.get());
        MYASSERT(*paths.get() == *recon_paths.get());
        cout
            << round << " round takes "
            << duration_cast<std::chrono::duration<double>>(end - start).count()
            << " seconds to extract " << edges->size() + paths->size()
            << " tuples, with batch process num: " << batch_process_nums[round]
            << endl;
        cout << "passed\n";
    }
}

int main(int argc, char **argv) {
    int run_id = 0;
    if (argc >= 2) {
        run_id = atoi(argv[1]);
    }
    switch (run_id) {
        case 1: {
            cout << "load graph test\n";
            loadGraphTest();
            break;
        }
        case 2: {
            cout << "run serverToServerNeighborPassTest\n";
            int detect_length_k = 3;
            int max_degree = 10;
            if (argc >= 3) {
                detect_length_k = atoi(argv[2]);
            }
            if (argc >= 4) {
                max_degree = atoi(argv[3]);
            }
            serverToServerNeighborPassTest(detect_length_k, max_degree);
            break;
        }
        case 3: {
            cout << "run sort test with small data set\n";
            endToEndSortSmallTest();
            break;
        }
        case 4: {
            cout << "run sort test with random data set\n";
            uint64_t sort_elem_threshold = 9;
            if (argc >= 3) {
                sort_elem_threshold = atoi(argv[2]);
            }
            sortEndToEndTest(sort_elem_threshold);
            break;
        }
        case 5: {
            cout << "run filter test\n";
            uint64_t filter_elem_num = (1 << 23);
            int detect_length_k = 3;
            if (argc >= 3) {
                filter_elem_num = (1 << atoi(argv[2]));
            }
            if (argc >= 4) {
                detect_length_k = atoi(argv[3]);
            }
            filterTest(filter_elem_num, detect_length_k);
            break;
        }
        case 6: {
            cout << "run extractTest\n";
            extractTest();
            break;
        }
        default:
            break;
    }

    return 0;
}