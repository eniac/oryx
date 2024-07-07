#include "secret_share.hpp"

#include <random>

void formatTupleTest() {
    int id_range = 16;
    SecretShareTupleHelper helper(id_range, kTieBreakerRange);
    int max_degree = 5;
    int detect_length_k = 5;
    int node_id = 1;
    vector<uint32_t> edges = {2, 3};
    unique_ptr<SecretShareTuple> t =
        helper.formatEdgeTuple(node_id, edges, max_degree, detect_length_k);
    vector<unique_ptr<SecretShareTuple>> ss = helper.splitTuple(t);
    vector<SecretShareTuple*> ss_recon = {ss[0].get(), ss[1].get(),
                                          ss[2].get()};
    unique_ptr<SecretShareTuple> recon_t =
        helper.reconTuple(ss_recon, max_degree, detect_length_k);
    MYASSERT(*t.get() == *recon_t.get());
    t->print();
    recon_t->print();
    ss[0]->print();
    ss[1]->print();
    ss[2]->print();

    vector<uint32_t> path = {1, 2, 3, 4, 5};
    unique_ptr<SecretShareTuple> pt =
        helper.formatPathTuple(path, max_degree, detect_length_k);
    vector<unique_ptr<SecretShareTuple>> pss = helper.splitTuple(pt);
    vector<SecretShareTuple*> pss_recon = {pss[0].get(), pss[1].get(),
                                           pss[2].get()};
    unique_ptr<SecretShareTuple> recon_pt =
        helper.reconTuple(pss_recon, max_degree, detect_length_k);
    MYASSERT(*pt.get() == *recon_pt.get());
    pt->print();
    recon_pt->print();
    pss[0]->print();
    pss[1]->print();
    pss[2]->print();
}

void shuffleMicroTest() {
    int id_range = 8;
    SecretShareTupleHelper helper(id_range, kTieBreakerRange);
    int max_degree = 3;
    int detect_length_k = 3;
    int node_id = 1;
    int parallel_num = 8;
    int seed1 = 123;
    int seed2 = 345;

    vector<uint32_t> e1 = {1, 2};
    vector<uint32_t> e2 = {3, 4};
    unique_ptr<SecretShareTuple> t1 =
        helper.formatEdgeTuple(node_id, e1, max_degree, detect_length_k);
    unique_ptr<SecretShareTuple> t2 =
        helper.formatEdgeTuple(node_id, e2, max_degree, detect_length_k);

    Server server(SERVER_ID::S3, id_range, max_degree, detect_length_k,
                  parallel_num, /*seed12_permute=*/0, /*seed23_permute=*/1,
                  /*seed13_permute=*/2, /*seedA=*/3, /*seedB=*/0,
                  kTieBreakerRange);
    server.insertSecrtShareTuple(*t1.get(), *t2.get());
    server.insertSecrtShareTuple(*t2.get(), *t1.get());
    server.insertSecrtShareTuple(*t2.get(), *t1.get());
    server.insertSecrtShareTuple(*t2.get(), *t1.get());
    server.insertSecrtShareTuple(*t2.get(), *t1.get());
    ServerSecretShares ss1 = server.createRandomness(seed1);
    ServerSecretShares ss2 = server.createRandomness(seed2);
    cout << "The first shares" << endl;
    ss1.print();
    cout << "The second shares" << endl;
    ss2.print();
    cout << "The dest shares" << endl;
    server.getFirstServerSecretShares()->print();

    vector<ServerSecretShares*> svec = {server.getFirstServerSecretShares(),
                                        &ss1, &ss2};

    unique_ptr<ServerSecretShares> xor_res =
        server.xorSecretShareTupleAndCreateDst(svec);
    server.xorSecretShareTuple(server.getFirstServerSecretShares(), svec);
    cout << "Server share after XOR result" << endl;
    server.getFirstServerSecretShares()->print();
    cout << "XOR after XOR result" << endl;
    xor_res->print();
    bool cmp1 = (*xor_res.get() == *server.getFirstServerSecretShares());
    MYASSERT(cmp1);

    cout << "After shuffling" << endl;
    server.shuffleSecretShares(*server.getFirstServerSecretShares(), seed1);
    server.getFirstServerSecretShares()->print();
    server.shuffleSecretShares(*xor_res.get(), seed1);
    MYASSERT(*xor_res.get() == *server.getFirstServerSecretShares());
}

void shuffleE2E(uint64_t shuffle_tuple_num) {
    cout << "\nBegin E2E\n";
    int id_range = 8;
    SecretShareTupleHelper helper(id_range, kTieBreakerRange);
    int max_degree = 10;
    int detect_length_k = 3;
    int node_id = 1;
    int parallel_num = 32;
    int seed12 = 1;
    int seed23 = 2;
    int seed31 = 3;
    int seedA = 4;
    int seedB = 5;

    Server server1(SERVER_ID::S1, id_range, max_degree, detect_length_k,
                   parallel_num, /*seed12_permute=*/seed12,
                   /*seed23_permute=*/0,
                   /*seed31_permute=*/seed31, /*seedA=*/seedA, /*seedB=*/seedB,
                   kTieBreakerRange);
    Server server2(SERVER_ID::S2, id_range, max_degree, detect_length_k,
                   parallel_num, /*seed12_permute=*/seed12,
                   /*seed23_permute=*/seed23,
                   /*seed31_permute=*/0, /*seedA=*/0, /*seedB=*/seedB,
                   kTieBreakerRange);
    Server server3(SERVER_ID::S3, id_range, max_degree, detect_length_k,
                   parallel_num, /*seed12_permute=*/0,
                   /*seed23_permute=*/seed23,
                   /*seed31_permute=*/seed31, /*seedA=*/seedA, /*seedB=*/0,
                   kTieBreakerRange);

    ServerSecretShares original_tuple;
    cout << "creating inputs\n";
    for (int i = 0; i < shuffle_tuple_num; i++) {
        vector<uint32_t> e = {static_cast<uint32_t>(rand() % id_range),
                              static_cast<uint32_t>(rand() % id_range)};
        unique_ptr<SecretShareTuple> t =
            helper.formatEdgeTuple(node_id, e, max_degree, detect_length_k);
        t->set_tie_breaker_id(i);
        vector<unique_ptr<SecretShareTuple>> ts = helper.splitTuple(t);

        server1.insertSecrtShareTuple(*ts[0].get(), *ts[1].get());
        server2.insertSecrtShareTuple(*ts[1].get(), *ts[2].get());
        server3.insertSecrtShareTuple(*ts[2].get(), *ts[0].get());

        original_tuple.insert(t);
    }
    cout << "finish creating inputs\n";

    // cout << "original" << endl;
    // original_tuple.print();
    auto start = system_clock::now();

    unique_ptr<ServerSecretShares> x2 = server1.shuffle_s1_step1();
    unique_ptr<ServerSecretShares> y1 = server2.shuffle_s2_step1();
    unique_ptr<ServerSecretShares> c1 = server2.shuffle_s2_step2(*x2.get());
    unique_ptr<ServerSecretShares> c2 = server3.shuffle_s3_step1(*y1.get());
    server2.shuffle_s2_step3(*c1.get(), *c2.get());
    server3.shuffle_s3_step2(*c1.get(), *c2.get());
    auto end = system_clock::now();
    cout << "Takes "
         << duration_cast<std::chrono::duration<double>>(end - start).count()
         << " seconds with " << shuffle_tuple_num
         << " tuples, max_degree: " << max_degree
         << " detect length k: " << detect_length_k << endl;

    MYASSERT(*server1.getSecondServerSecretShares() ==
             *server2.getFirstServerSecretShares());
    cout << "after shuffle ss2 equals\n";
    MYASSERT(*server2.getSecondServerSecretShares() ==
             *server3.getFirstServerSecretShares());
    cout << "after shuffle ss3 equals\n";
    MYASSERT(*server3.getSecondServerSecretShares() ==
             *server1.getFirstServerSecretShares());
    cout << "after shuffle ss1 equals\n";

    vector<ServerSecretShares*> s_vec1 = {server1.getFirstServerSecretShares(),
                                          server2.getFirstServerSecretShares(),
                                          server3.getFirstServerSecretShares()};

    vector<ServerSecretShares*> s_vec2 = {
        server1.getSecondServerSecretShares(),
        server2.getSecondServerSecretShares(),
        server3.getSecondServerSecretShares()};
    unique_ptr<ServerSecretShares> recon1 =
        helper.reconServerShares(s_vec1, max_degree, detect_length_k);
    unique_ptr<ServerSecretShares> recon2 =
        helper.reconServerShares(s_vec2, max_degree, detect_length_k);

    MYASSERT(*recon1.get() == *recon2.get());
    server1.shuffleSecretShares(original_tuple, seed12);
    server1.shuffleSecretShares(original_tuple, seed31);
    MYASSERT(!(*recon1.get() == original_tuple));
    cout << "shuffled is different from original as expected\n";
    server1.shuffleSecretShares(original_tuple, seed23);
    MYASSERT(*recon1.get() == original_tuple);
    // cout << "recon1" << endl;
    // recon1->print();
    // cout << "recon2" << endl;
    // recon2->print();
    // cout << "original" << endl;
    // original_tuple.print();
    cout << "finish e2e passed\n";
}

void serializeTest() {
    cout << "\nBegin serialization test\n";
    uint32_t id_range = (1 << 31);
    SecretShareTupleHelper helper(id_range, kTieBreakerRange);
    int max_degree = 10;
    int detect_length_k = 4;
    int node_id = 1;
    int parallel_num = 16;
    int run_times = 1;
    int tuple_elem_num = (1 << 20);

    srand(time(0));

    for (int j = 0; j < run_times; j++) {
        ServerSecretShares ss;
        ServerSecretShares ss1;
        ServerSecretShares ss2;
        ServerSecretShares ss3;
        for (int i = 0; i < tuple_elem_num; i++) {
            vector<uint32_t> e = {static_cast<uint32_t>(rand() % id_range),
                                  static_cast<uint32_t>(rand() % id_range)};
            unique_ptr<SecretShareTuple> t =
                helper.formatEdgeTuple(node_id, e, max_degree, detect_length_k);
            t->set_tie_breaker_id(i);
            vector<unique_ptr<SecretShareTuple>> ts = helper.splitTuple(t);

            string serialize_str = t->serialize();
            SecretShareTuple deserialize_t(serialize_str, max_degree,
                                           detect_length_k);
            MYASSERT(*t.get() == deserialize_t);

            string serialize_str1 = ts[0]->serialize();
            SecretShareTuple deserialize_t1(serialize_str1, max_degree,
                                            detect_length_k);
            MYASSERT(*ts[0].get() == deserialize_t1);

            string serialize_str2 = ts[1]->serialize();
            SecretShareTuple deserialize_t2(serialize_str2, max_degree,
                                            detect_length_k);
            MYASSERT(*ts[1].get() == deserialize_t2);

            string serialize_str3 = ts[2]->serialize();
            SecretShareTuple deserialize_t3(serialize_str3, max_degree,
                                            detect_length_k);
            MYASSERT(*ts[2].get() == deserialize_t3);

            ss.insert(t);
            ss1.insert(ts[0]);
            ss2.insert(ts[1]);
            ss3.insert(ts[2]);
        }
        system_clock::time_point serialize_start, serialize_end,
            deserialize_start, deserialize_end;
        double serialize_total = 0.0;
        double deserialize_total = 0.0;

        serialize_start = system_clock::now();
        string ss_serialize_str = ss.serializeParallel(parallel_num);
        serialize_end = system_clock::now();
        serialize_total += duration_cast<std::chrono::duration<double>>(
                               serialize_end - serialize_start)
                               .count();

        deserialize_start = system_clock::now();
        ServerSecretShares recon_ss(ss_serialize_str, max_degree,
                                    detect_length_k, parallel_num);
        deserialize_end = system_clock::now();
        deserialize_total += duration_cast<std::chrono::duration<double>>(
                                 deserialize_end - deserialize_start)
                                 .count();

        MYASSERT(ss == recon_ss);

        serialize_start = system_clock::now();
        string ss_serialize_str1 = ss1.serializeParallel(parallel_num);
        serialize_end = system_clock::now();
        serialize_total += duration_cast<std::chrono::duration<double>>(
                               serialize_end - serialize_start)
                               .count();

        deserialize_start = system_clock::now();
        ServerSecretShares recon_ss1(ss_serialize_str1, max_degree,
                                     detect_length_k, parallel_num);
        deserialize_end = system_clock::now();
        deserialize_total += duration_cast<std::chrono::duration<double>>(
                                 deserialize_end - deserialize_start)
                                 .count();

        MYASSERT(ss1 == recon_ss1);

        serialize_start = system_clock::now();
        string ss_serialize_str2 = ss2.serializeParallel(parallel_num);
        serialize_end = system_clock::now();
        serialize_total += duration_cast<std::chrono::duration<double>>(
                               serialize_end - serialize_start)
                               .count();

        deserialize_start = system_clock::now();
        ServerSecretShares recon_ss2(ss_serialize_str2, max_degree,
                                     detect_length_k, parallel_num);
        deserialize_end = system_clock::now();
        deserialize_total += duration_cast<std::chrono::duration<double>>(
                                 deserialize_end - deserialize_start)
                                 .count();

        MYASSERT(ss2 == recon_ss2);

        serialize_start = system_clock::now();
        string ss_serialize_str3 = ss3.serializeParallel(parallel_num);
        serialize_end = system_clock::now();
        serialize_total += duration_cast<std::chrono::duration<double>>(
                               serialize_end - serialize_start)
                               .count();

        deserialize_start = system_clock::now();
        ServerSecretShares recon_ss3(ss_serialize_str3, max_degree,
                                     detect_length_k, parallel_num);
        deserialize_end = system_clock::now();
        deserialize_total += duration_cast<std::chrono::duration<double>>(
                                 deserialize_end - deserialize_start)
                                 .count();

        MYASSERT(ss3 == recon_ss3);

        vector<ServerSecretShares *> recon_vec = {&ss1, &ss2, &ss3};
        unique_ptr<ServerSecretShares> recon_from_shares =
            helper.reconServerShares(recon_vec, max_degree, detect_length_k);
        MYASSERT(*recon_from_shares.get() == ss);
        cout << j
             << "th test passed, serialize avg time: " << serialize_total / 4
             << ", deserialize avg time: " << deserialize_total / 4 << " with "
             << parallel_num << " threads" << endl;
    }
    cout << "end seriaize test\n";
}

void resortTest() {
    cout << "begin resort test" << endl;
    int id_range = 8;
    SecretShareTupleHelper helper(id_range, kTieBreakerRange);
    int max_degree = 3;
    int detect_length_k = 3;
    int node_id = 1;
    int parallel_num = 8;

    vector<uint32_t> e1 = {1, 2};
    vector<uint32_t> e2 = {3, 4};
    vector<uint32_t> e3 = {5, 6};
    vector<uint32_t> e4 = {7, 8};
    vector<uint32_t> e5 = {9, 10};
    vector<uint32_t> e6 = {11, 12};

    unique_ptr<SecretShareTuple> t1 =
        helper.formatEdgeTuple(node_id, e1, max_degree, detect_length_k);
    unique_ptr<SecretShareTuple> t2 =
        helper.formatEdgeTuple(node_id, e2, max_degree, detect_length_k);
    unique_ptr<SecretShareTuple> t3 =
        helper.formatEdgeTuple(node_id, e3, max_degree, detect_length_k);
    unique_ptr<SecretShareTuple> t4 =
        helper.formatEdgeTuple(node_id, e4, max_degree, detect_length_k);
    unique_ptr<SecretShareTuple> t5 =
        helper.formatEdgeTuple(node_id, e5, max_degree, detect_length_k);
    unique_ptr<SecretShareTuple> t6 =
        helper.formatEdgeTuple(node_id, e6, max_degree, detect_length_k);

    ServerSecretShares ss;
    ss.insert(t1);
    ss.insert(t2);
    ss.insert(t3);
    ss.insert(t4);
    ss.insert(t5);
    ss.insert(t6);

    vector<uint64_t> small = {4, 1};
    vector<uint64_t> large = {5, 2, 3};
    ss.resortByPartition(small, large, 1, 6, 4);
    ss.print();

    cout << "print 2\n";
    small = {2, 0, 1};
    large = {};
    ss.resortByPartition(small, large, 0, 3, 1);
    ss.print();

    cout << "end resort test" << endl;
}

void extractNeighbosTest() {
    uint32_t id_range = (1 << 20);
    SecretShareTupleHelper helper(id_range, kTieBreakerRange);
    int max_degree = 3;
    unique_ptr<SecretShareTuple> t =
        helper.formatEdgeTuple(3, {1, 2}, max_degree, 3);
    vector<uint32_t> secret_shares;
    t->extractAndAppendNodeAndNeighbos(secret_shares);

    vector<uint32_t> answer1 = {3, 1, 2, 0};
    for (uint64_t i = 0; i < answer1.size(); i++) {
        MYASSERT(secret_shares[i] == answer1[i]);
    }

    vector<uint32_t> updated_neighbors = {7, 8, 9};
    t->updateNeighbors(updated_neighbors);

    vector<uint32_t> new_neighbors;
    t->extractAndAppendNodeAndNeighbos(new_neighbors);

    vector<uint32_t> answer2 = {3, 7, 8, 9};
    for (uint64_t i = 0; i < updated_neighbors.size(); i++) {
        MYASSERT(new_neighbors[i] == answer2[i]);
    }
    cout << "passed test\n";
}

int main(int argc, char **argv) {
    int run_id = 0;
    if (argc >= 2) {
        run_id = atoi(argv[1]);
    }
    switch (run_id) {
        case 1: {
            cout << "formatTupleTest\n";
            formatTupleTest();
            break;
        }
        case 2: {
            cout << "run shuffleMicroTest\n";
            shuffleMicroTest();
            break;
        }
        case 3: {
            cout << "run shuffleE2E\n";
            uint64_t shuffle_tuple_num = (1 << 20);
            if (argc >= 3) {
                shuffle_tuple_num = (1 << atoi(argv[2]));
            }
            shuffleE2E(shuffle_tuple_num);
            break;
        }
        case 4: {
            cout << "run serializeTest\n";
            serializeTest();
            break;
        }
        case 5: {
            cout << "resort test\n";
            resortTest();
            break;
        }
        case 6: {
            cout << "extract and update neighbor test\n";
            extractNeighbosTest();
            break;
        }
        default:
            break;
    }
    // formatTupleTest();
    // shuffleMicroTest();
    // shuffleE2E();
    // serializeTest();
    // resortTest();

    return 0;
}