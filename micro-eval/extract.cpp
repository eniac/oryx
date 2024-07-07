#include "eval_server.hpp"

int main(int argc, char **argv) {
    string config_file = string(argv[1]);
    cout << "[config file name]: " << config_file << endl;
    map<string, string> config_map = parseConfig(config_file);

    int bit_len = kBitLen;
    uint32_t id_range = (1 << kBitLen);

    enum SERVER_ID server_id =
        accessServerIdFromConfig(config_map, "server_id");
    int local_compute_parallel_num =
        accessIntFromConfig(config_map, "local_compute_parallel_num");
    int neighbor_pass_parallel_num =
        accessIntFromConfig(config_map, "neighbor_pass_parallel_num");
    int sort_parallel_num =
        accessIntFromConfig(config_map, "sort_parallel_num");
    int filter_parallel_num =
        accessIntFromConfig(config_map, "filter_parallel_num");
    int extract_path_parallel_num =
        accessIntFromConfig(config_map, "extract_path_parallel_num");
    int sort_elem_threshold =
        accessIntFromConfig(config_map, "sort_elem_threshold");
    int num_to_find_median =
        accessIntFromConfig(config_map, "num_to_find_median");
    int seed12_permute = accessIntFromConfig(config_map, "seed12_permute");
    int seed23_permute = accessIntFromConfig(config_map, "seed23_permute");
    int seed31_permute = accessIntFromConfig(config_map, "seed31_permute");
    int seedA = accessIntFromConfig(config_map, "seedA");
    int seedB = accessIntFromConfig(config_map, "seedB");
    string ip_1 = accessStrFromConfig(config_map, "ip_1");
    string ip_2 = accessStrFromConfig(config_map, "ip_2");
    string ip_3 = accessStrFromConfig(config_map, "ip_3");

    int max_degree = accessIntFromConfig(config_map, "max_degree");
    int detect_length_k = accessIntFromConfig(config_map, "detect_length_k");
    int total_detect_length =
        accessIntFromConfig(config_map, "total_detect_length");

    uint64_t filter_batch_num =
        accessUint64FromConfig(config_map, "filter_batch_num");
    uint64_t neighbor_pass_batch_num =
        accessUint64FromConfig(config_map, "neighbor_pass_batch_num");
    uint64_t extract_batch_num =
        accessUint64FromConfig(config_map, "extract_batch_num");
    uint64_t partition_batch_num =
        accessUint64FromConfig(config_map, "partition_batch_num");
    uint64_t nodes_num = accessUint64FromConfig(config_map, "nodes_num");
    uint64_t path_num = accessUint64FromConfig(config_map, "path_num");
    int run_times = accessIntFromConfig(config_map, "run_times");

    cout << "bit len: " << bit_len << endl;
    cout << "id range: " << id_range << endl;

    EvalServer server(server_id, id_range, bit_len, max_degree, detect_length_k,
                      total_detect_length,
                      /*local_compute_parallel_num=*/local_compute_parallel_num,
                      /*neighbor_pass_parallel_num=*/neighbor_pass_parallel_num,
                      /*sort_parallel_num=*/sort_parallel_num,
                      /*filter_parallel_num=*/filter_parallel_num,
                      /*extract_path_parallel_num=*/extract_path_parallel_num,
                      /*sort_elem_threshold=*/sort_elem_threshold,
                      /*num_to_find_median=*/num_to_find_median,
                      /*seed12_permute=*/seed12_permute,
                      /*seed23_permute=*/seed23_permute,
                      /*seed31_permute=*/seed31_permute, /*seedA=*/seedA,
                      /*seedB=*/seedB, ip_1, ip_2, ip_3,
                      /*filter_batch_num=*/filter_batch_num,
                      /*neighbor_pass_batch_num=*/neighbor_pass_batch_num,
                      /*extract_batch_num=*/extract_batch_num,
                      /*partition_batch_num=*/partition_batch_num,
                      /*tie_breaker_bit=*/kTieBreakerBit);
    cout << "server init\n";
    server.establishConnection();

    unique_ptr<ServerSecretShares> back_up_local_shares =
        server.prepareSecretSharesForExtract(max_degree, detect_length_k,
                                             nodes_num, path_num,
                                             local_compute_parallel_num);

    for (int i = 0; i < run_times; i++) {
        unique_ptr<ServerSecretShares> local_shares =
            make_unique<ServerSecretShares>();
        local_shares->copyFromServerSecretShares(back_up_local_shares);
        auto start = system_clock::now();
        unique_ptr<ServerSecretShares> edges =
            make_unique<ServerSecretShares>();
        unique_ptr<ServerSecretShares> paths =
            make_unique<ServerSecretShares>();
        server.extractWrapper(local_shares, edges, paths);
        auto end = system_clock::now();
        cout
            << "[TIME] round " << i << " extract takes: "
            << duration_cast<std::chrono::duration<double>>(end - start).count()
            << " seconds, in total " << nodes_num * (1 + path_num)
            << " tuples, percentage of path tuples is: "
            << static_cast<double>(path_num) / (path_num + 1) << "%" << endl;
    }

    server.closeConnection();
    return 0;
}