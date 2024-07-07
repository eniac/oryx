#ifndef COMPUTE_SERVER_HPP
#define COMPUTE_SERVER_HPP

#include <sstream>

#include "secret-share/secret_share.hpp"
#include "two_pc_compute.hpp"

class NetServer {
   protected:
    unique_ptr<Server> compute_server;

    SERVER_ID server_id;
    int bit_len;
    uint32_t id_range;
    int max_degree;
    int detect_length_k;
    int total_detect_length;

    // parallism
    int local_compute_parallel_num;
    int neighbor_pass_parallel_num;
    int sort_parallel_num;
    int filter_parallel_num;
    int extract_path_parallel_num;

    uint64_t sort_elem_threshold;
    int num_elem_to_find_median;

    /*ip address of the three computing server*/
    string ip_1;
    string ip_2;
    string ip_3;

    /*when the batch processing num is set to 0, it has no effect of batching*/
    /*just all data is loaded into memory once with no batching*/
    uint64_t filter_batch_num;
    uint64_t neighbor_pass_batch_num;
    uint64_t extract_batch_num;
    uint64_t partition_batch_num;

    int tie_breaker_bit;
    uint64_t tie_breaker_range;

    /*fd of connection*/
    int fd_1;
    int fd_2;
    int fd_3;

    string ip_no_port_1;
    string ip_no_port_2;
    string ip_no_port_3;

    int rand_median_seed = 100;
    int start_port = 12345;
    // for simplicity fix the random
    int reassign_randomness_seed = 2345;

    int tmp_max_degree;

    void setMaxDegreeToOne() {
        this->tmp_max_degree = this->max_degree;
        this->max_degree = 1;
        compute_server->set_max_degree(1);
    }

    void resetMaxDegree() {
        this->max_degree = this->tmp_max_degree;
        compute_server->set_max_degree(this->max_degree);
    }

    // TODO: the last thread should pass its local secret share to another
    // server (pass neighbors across different servers).
    //
    // This function updates the corresponding parts in `ss`.
    void obliviousNeighborPassThread(
        int thread_id, int port, string ip, int party, uint64_t start_index,
        uint64_t end_index, unique_ptr<ServerSecretShares>& ss,
        vector<uint32_t>& found_neighbor_shares,
        vector<uint32_t>& current_thread_task_finished_by_prev_thread,
        vector<bool>& encountered_edge_shares) {
        MYASSERT(end_index > start_index);
        MYASSERT(found_neighbor_shares.size() ==
                 (uint64_t)(max_degree * neighbor_pass_parallel_num));
        MYASSERT(current_thread_task_finished_by_prev_thread.size() ==
                 (uint64_t)neighbor_pass_parallel_num);
        MYASSERT(encountered_edge_shares.size() ==
                 (uint64_t)neighbor_pass_parallel_num);
        // cout << "[THREAD] " << thread_id << ", start index: " << start_index
        //      << ", end index: " << end_index << endl;
        TwoPcEngine engine(party, ip, port, bit_len, this->tie_breaker_bit);
        engine.initEngine();

        // `secret_shares` is formatted as N tuples, where each tuple contains
        // `(max_degree+1)` elements. The first is the node_id and remaining
        // `max_degree` nodes are the neighbors.
        vector<uint32_t> secret_shares;
        for (uint64_t i = start_index; i < end_index; i++) {
            ss->svec[i]->extractAndAppendNodeAndNeighbos(secret_shares);
        }

        // find neighbors
        vector<uint32_t> neighbor_shares;
        for (int i = 0; i < max_degree; i++) {
            neighbor_shares.push_back(secret_shares[i + 1]);
        }
        bool if_encounter_edge_share = false;
        uint64_t tuple_num_find_neighbor_of_end = end_index - start_index;
        if (this->neighbor_pass_batch_num == 0 ||
            this->neighbor_pass_batch_num >= tuple_num_find_neighbor_of_end) {
            engine.obliviousFindNeighbors(
                secret_shares, max_degree, neighbor_shares,
                if_encounter_edge_share, 0, tuple_num_find_neighbor_of_end);
        } else {
            cout << "find neighbors in batch mode\n";
            uint64_t begin_vec_index_find_neighbor_of_end = 0;
            while (begin_vec_index_find_neighbor_of_end <
                   tuple_num_find_neighbor_of_end) {
                uint64_t remained_tuple = tuple_num_find_neighbor_of_end -
                                          begin_vec_index_find_neighbor_of_end;
                uint64_t processed_num_tuple =
                    min(this->neighbor_pass_batch_num, remained_tuple);
                engine.obliviousFindNeighbors(
                    secret_shares, max_degree, neighbor_shares,
                    if_encounter_edge_share,
                    begin_vec_index_find_neighbor_of_end, processed_num_tuple);
                begin_vec_index_find_neighbor_of_end += processed_num_tuple;
            }
        }

        // the last thread do not need to write found neighbors
        // TODO: this logic needs to be replaced when extending to multiple
        // server sets.
        if (thread_id < neighbor_pass_parallel_num - 1) {
            // write found neighbors to `found_neighbor_shares`
            copy(
                neighbor_shares.begin(), neighbor_shares.end(),
                found_neighbor_shares.begin() + (thread_id + 1) * (max_degree));
            encountered_edge_shares[thread_id + 1] = if_encounter_edge_share;

            // set the current thread finishes for thread_id+1.
            current_thread_task_finished_by_prev_thread[(thread_id + 1)] = 1;
        }
        // cout << "[THREAD] " << thread_id << " finish finding neighbors\n";

        // TODO: for server (2,3), server (1,3), when the computation is split
        // into three different server paris running in parallel, the first
        // thread in server (2,3) needs to wait for the results from (1,2) and
        // (1,3) needs to wait for the dependency of server (1,3)

        // start checking if all previous threads has been finished writing
        for (int i = 1; i <= thread_id; i++) {
            while (*((volatile bool*)&(
                       current_thread_task_finished_by_prev_thread[i])) == 0)
                ;
        }

        // cout << "[THREAD] " << thread_id
        //      << " finish waiting neighbors from previous thread, start
        //      finding "
        //         "real neighbors"
        //      << endl;

        vector<uint32_t> start_neighbor_shares;
        if (thread_id <= 1) {
            start_neighbor_shares = vector<uint32_t>(max_degree);
            copy(found_neighbor_shares.begin() + (thread_id * (max_degree)),
                 found_neighbor_shares.begin() + (thread_id + 1) * (max_degree),
                 start_neighbor_shares.begin());
        } else {
            vector<uint32_t> previous_thread_neighbor_shares(thread_id *
                                                             max_degree);
            vector<bool> previous_if_encountered_edge_shares(thread_id);
            // copy the found neighbors of thread 1 to thread_id.
            // These results are written by previous threads (0 to thread_id-1).
            copy(found_neighbor_shares.begin() + max_degree,
                 found_neighbor_shares.begin() + (thread_id + 1) * max_degree,
                 previous_thread_neighbor_shares.begin());
            copy(encountered_edge_shares.begin() + 1,
                 encountered_edge_shares.begin() + (thread_id + 1),
                 previous_if_encountered_edge_shares.begin());
            engine.obliviousFindNeighborsOfEdgeTuple(
                thread_id, max_degree, previous_thread_neighbor_shares,
                previous_if_encountered_edge_shares, start_neighbor_shares);
        }

        // cout << "[THREAD] " << thread_id
        //      << " finish finding real neighbors, start passing neighbors\n";

        uint64_t tuple_num_neighbor_pass = end_index - start_index;
        vector<uint32_t> updated_secret_shares;
        if (this->neighbor_pass_batch_num == 0 ||
            this->neighbor_pass_batch_num >= tuple_num_neighbor_pass) {
            engine.obliviousNeighborPass(
                secret_shares, start_neighbor_shares, max_degree, 0,
                tuple_num_find_neighbor_of_end, updated_secret_shares);
        } else {
            cout << "run neighbor pass in batch mode\n";
            uint64_t curr_neighbor_pass_begin_index = 0;
            while (curr_neighbor_pass_begin_index <
                   tuple_num_find_neighbor_of_end) {
                uint64_t remained_neighbor_pass_num =
                    tuple_num_find_neighbor_of_end -
                    curr_neighbor_pass_begin_index;
                uint64_t neighbor_pass_process_num = min(
                    remained_neighbor_pass_num, this->neighbor_pass_batch_num);
                engine.obliviousNeighborPass(
                    secret_shares, start_neighbor_shares, max_degree,
                    curr_neighbor_pass_begin_index, neighbor_pass_process_num,
                    updated_secret_shares);
                curr_neighbor_pass_begin_index += neighbor_pass_process_num;
            }
        }

        // cout << "[THREAD] " << thread_id
        //      << " finish passing neighbors, start copying results\n";

        engine.terminateEngine();

        uint64_t processed_element_num = (end_index - start_index);
        MYASSERT(static_cast<uint64_t>(processed_element_num * max_degree) ==
                 updated_secret_shares.size());
        // copy result back
        for (uint64_t i = 0; i < processed_element_num; i++) {
            vector<uint32_t> updated_neighbors_for_this_tuple(max_degree);
            copy(updated_secret_shares.begin() + (i * max_degree),
                 updated_secret_shares.begin() + (i + 1) * max_degree,
                 updated_neighbors_for_this_tuple.begin());
            ss->svec[i + start_index]->updateNeighbors(
                updated_neighbors_for_this_tuple);
        }

        // cout << "[THREAD] " << thread_id
        //      << " finish all neighbor passing tasks\n";
    }

    // // TODO: take ss as the input and swap directly without returning the
    // // smaller_index and larger_index?
    // void partitionThread(int port, string another_server_ip, int party,
    //                      vector<uint32_t>& node_val_shares,
    //                      vector<uint32_t>& end_node_shares,
    //                      uint32_t pivot_val_share,
    //                      uint32_t pivot_end_node_share,
    //                      vector<uint64_t>& smaller_index,
    //                      vector<uint64_t>& larger_index, uint64_t
    //                      begin_index) {
    //     TwoPcEngine engine(party, another_server_ip, port, bit_len);
    //     engine.initEngine();
    //     engine.obliviousPartition(node_val_shares, end_node_shares,
    //                               pivot_val_share, pivot_end_node_share,
    //                               smaller_index, larger_index, begin_index);
    //     engine.terminateEngine();

    //     // for (uint64_t i = 0; i < smaller_index.size(); i++) {
    //     //     smaller_index[i] += begin_index;
    //     // }

    //     // for (uint64_t i = 0; i < larger_index.size(); i++) {
    //     //     larger_index[i] += begin_index;
    //     // }
    // }

   public:
    NetServer(SERVER_ID server_id_, uint32_t id_range_, int bit_len_,
              int max_degree_, int detect_length_k_, int total_detect_length_,
              int local_compute_parallel_num_, int neighbor_pass_parallel_num_,
              int sort_parallel_num_, int filter_parallel_num_,
              int extract_path_parallel_num_, uint64_t sort_elem_threshold_,
              int num_elem_to_find_median_, int seed12_permute_,
              int seed23_permute_, int seed31_permute_, int seedA_, int seedB_,
              /*ip address of the three computing server*/
              string ip_1_, string ip_2_, string ip_3_,
              /*num of tuples processed in each batch*/
              uint64_t filter_batch_num_, uint64_t neighbor_pass_batch_num_,
              uint64_t extract_batch_num_, uint64_t partition_batch_num_,
              int tie_breaker_bit_)
        : server_id(server_id_),
          bit_len(bit_len_),
          id_range(id_range_),
          max_degree(max_degree_),
          detect_length_k(detect_length_k_),
          total_detect_length(total_detect_length_),
          local_compute_parallel_num(local_compute_parallel_num_),
          neighbor_pass_parallel_num(neighbor_pass_parallel_num_),
          sort_parallel_num(sort_parallel_num_),
          filter_parallel_num(filter_parallel_num_),
          extract_path_parallel_num(extract_path_parallel_num_),
          sort_elem_threshold(sort_elem_threshold_),
          num_elem_to_find_median(num_elem_to_find_median_),
          ip_1(ip_1_),
          ip_2(ip_2_),
          ip_3(ip_3_),
          filter_batch_num(filter_batch_num_),
          neighbor_pass_batch_num(neighbor_pass_batch_num_),
          extract_batch_num(extract_batch_num_),
          partition_batch_num(partition_batch_num_),
          tie_breaker_bit(tie_breaker_bit_) {
        // extract the ip into ip address only without port
        ip_no_port_1 = split(ip_1, ":")[0];
        ip_no_port_2 = split(ip_2, ":")[0];
        ip_no_port_3 = split(ip_3, ":")[0];
        tie_breaker_range = (1 << tie_breaker_bit);

        compute_server = make_unique<Server>(
            server_id_, id_range_, max_degree_, detect_length_k_,
            local_compute_parallel_num_, seed12_permute_, seed23_permute_,
            seed31_permute_, seedA_, seedB_, this->tie_breaker_range);
    }

    void loadSecretShare(string share_file1, string share_file2) {
        // load the first file
        ifstream in1(share_file1);
        MYASSERT(in1.is_open());
        stringstream ss1;
        ss1 << in1.rdbuf();
        in1.close();
        string serialized_str1 = ss1.str();
        cout << serialized_str1.size() << endl;
        compute_server->loadFirstServerSecretShares(serialized_str1);
        serialized_str1.clear();
        cout << "finish load the first share\n";

        // load the second file
        ifstream in2(share_file2);
        MYASSERT(in2.is_open());
        stringstream ss2;
        ss2 << in2.rdbuf();
        in2.close();
        string serialized_str2 = ss2.str();
        compute_server->loadSecondServerSecretShares(serialized_str2);
        serialized_str2.clear();
        cout << "finish load the second share\n";

        // assign ids
        compute_server->assignTieBreakerIds();
    }

    ServerSecretShares* getFirstServerSecretShares() {
        return compute_server->getFirstServerSecretShares();
    }

    ServerSecretShares* getSecondServerSecretShares() {
        return compute_server->getSecondServerSecretShares();
    }

    // ss will be updated with the passed neighbors.
    // ss must be sorted before running neighborpassing
    void obliviousNeighborPass(unique_ptr<ServerSecretShares>& ss,
                               string another_server_ip, int party) {
        // for (uint64_t i = 0; i < secret_shares.size(); i++) {
        //     cout << "original " << i << " " << secret_shares[i] << endl;
        // }
        vector<bool> thread_task_status(neighbor_pass_parallel_num, false);
        // MYASSERT(secret_shares.size() % (max_degree + 1) == 0);
        uint64_t element_num = ss->size();
        uint64_t elements_processed_per_thread =
            element_num / neighbor_pass_parallel_num;
        vector<thread> t_vec;
        // structured as `neighbor_pass_parallel_num` tuples, where each tuple
        // contains `max_degree` elements.
        //
        // The max_degree elements are local shares of the found neighbors.
        vector<uint32_t> found_neighbor_shares(
            neighbor_pass_parallel_num * (max_degree), 0);

        vector<bool> encountered_edge_shares(neighbor_pass_parallel_num, false);
        vector<uint32_t> current_thread_task_finished_by_prev_thread(
            neighbor_pass_parallel_num, 0);

        // set the first thread's neighbors to be ready (always zeros)
        // TODO: this logic needs to be replaced when there are three server
        // pairs.
        current_thread_task_finished_by_prev_thread[0] = 1;

        for (int i = 0; i < neighbor_pass_parallel_num; i++) {
            uint64_t start_index =
                elements_processed_per_thread * static_cast<uint64_t>(i);
            uint64_t end_index =
                elements_processed_per_thread * static_cast<uint64_t>(i + 1);
            if (i == neighbor_pass_parallel_num - 1) {
                end_index = element_num;
            }
            if (start_index == end_index) {
                MYASSERT(i != neighbor_pass_parallel_num - 1);
                cout << "[THREAD] " << i << " has no work, skip it\n";
                // set dependency to 1 so the next thread won't block
                current_thread_task_finished_by_prev_thread[(i + 1)] = 1;
                continue;
            }
            thread t(&NetServer::obliviousNeighborPassThread, this, i,
                     start_port + i, another_server_ip, party, start_index,
                     end_index, ref(ss), ref(found_neighbor_shares),
                     ref(current_thread_task_finished_by_prev_thread),
                     ref(encountered_edge_shares));
            t_vec.push_back(move(t));
        }

        for (auto& t : t_vec) {
            t.join();
        }
        // for (uint64_t i = 0; i < secret_shares.size(); i++) {
        //     cout << "after " << i << " " << secret_shares[i] << endl;
        // }
    }

    // // This version is a naive implementation
    // // the sort range is [begin_index, end_index), end_index is not included
    // void obliviousSortSlow(unique_ptr<ServerSecretShares>& ss,
    //                        uint64_t begin_index, uint64_t end_index,
    //                        string another_server_ip, int party) {
    //     auto start = system_clock::now();
    //     // Check if the current step can be stopped
    //     if (begin_index + 1 >= end_index) {
    //         return;
    //     }

    //     // if (party == ALICE) {
    //     //     cout << "init begin_index: " << begin_index << endl;
    //     //     cout << "init end_index: " << end_index << endl;
    //     // }

    //     auto extract_start = system_clock::now();
    //     // obtain secret shares
    //     vector<uint32_t> node_val_shares, end_node_shares;
    //     ss->extractNodeValFromServerSecretShare(
    //         node_val_shares, end_node_shares, begin_index, end_index);

    //     auto extract_end = system_clock::now();

    //     // cout << "[check first val] party: " << party << " "
    //     //      << node_val_shares[0] << " " << end_node_shares[0] << endl;

    //     auto thread_start = system_clock::now();
    //     // choose pivot
    //     uint32_t pivot_val_share = node_val_shares.back();
    //     uint32_t pivot_end_node_share = end_node_shares.back();

    //     // parallel partition

    //     // if there is not enough elements, do not spawn that many threads
    //     int required_thread_num =
    //         (end_index - begin_index) / this->sort_elem_threshold;
    //     if (required_thread_num == 0) {
    //         required_thread_num = 1;
    //     }
    //     required_thread_num = min(required_thread_num, sort_parallel_num);

    //     uint64_t task_per_thread =
    //         (end_index - begin_index) / required_thread_num;

    //     vector<vector<uint64_t>> smaller_index_vec(required_thread_num);
    //     vector<vector<uint64_t>> larger_index_vec(required_thread_num);

    //     vector<thread> t_vec;

    //     vector<vector<uint32_t>> node_val_shares_all_thread(
    //         required_thread_num);
    //     vector<vector<uint32_t>> end_node_shares_all_thread(
    //         required_thread_num);

    //     cout << "[Party] " << party << " using " << required_thread_num
    //          << " thread to run partition\n";
    //     for (int i = 0; i < required_thread_num; i++) {
    //         uint64_t begin = i * task_per_thread;
    //         uint64_t end = (i + 1) * task_per_thread;
    //         if (i == required_thread_num - 1) {
    //             // we also include the pivot as well for comparison
    //             end = end_index - begin_index;
    //         }
    //         node_val_shares_all_thread[i] = vector<uint32_t>(end - begin);
    //         copy(node_val_shares.begin() + begin, node_val_shares.begin() +
    //         end,
    //              node_val_shares_all_thread[i].begin());

    //         end_node_shares_all_thread[i] = vector<uint32_t>(end - begin);
    //         copy(end_node_shares.begin() + begin, end_node_shares.begin() +
    //         end,
    //              end_node_shares_all_thread[i].begin());

    //         cout << "[thread] " << i << " processing " << begin << " to " <<
    //         end
    //              << endl;

    //         thread t(&NetServer::partitionThread, this, start_port + i,
    //                  another_server_ip, party,
    //                  ref(node_val_shares_all_thread[i]),
    //                  ref(end_node_shares_all_thread[i]), pivot_val_share,
    //                  pivot_end_node_share, ref(smaller_index_vec[i]),
    //                  ref(larger_index_vec[i]), begin + begin_index);
    //         t_vec.push_back(move(t));
    //     }

    //     for (auto& t : t_vec) {
    //         t.join();
    //     }

    //     auto thread_end = system_clock::now();

    //     auto merge_start = system_clock::now();

    //     // merge smaller_index and larger_index
    //     uint64_t small_index_size = 0;
    //     uint64_t large_index_size = 0;
    //     for (int i = 0; i < required_thread_num; i++) {
    //         small_index_size += smaller_index_vec[i].size();
    //         large_index_size += larger_index_vec[i].size();
    //     }

    //     vector<uint64_t> all_small_indices(small_index_size);
    //     vector<uint64_t> all_large_indices(large_index_size);
    //     uint64_t small_current_index = 0;
    //     uint64_t large_current_index = 0;
    //     for (int i = 0; i < required_thread_num; i++) {
    //         copy(smaller_index_vec[i].begin(), smaller_index_vec[i].end(),
    //              all_small_indices.begin() + small_current_index);
    //         copy(larger_index_vec[i].begin(), larger_index_vec[i].end(),
    //              all_large_indices.begin() + large_current_index);
    //         small_current_index += smaller_index_vec[i].size();
    //         large_current_index += larger_index_vec[i].size();
    //     }

    //     // if (party == ALICE) {
    //     //     cout << "begin_index: " << begin_index << endl;
    //     //     cout << "end_index: " << end_index << endl;
    //     //     cout << "small_current_index: " << small_current_index <<
    //     endl;
    //     //     for (uint32_t i = 0; i < all_small_indices.size(); i++) {
    //     //         cout << i << " small " << all_small_indices[i] << endl;
    //     //     }
    //     //     for (uint32_t i = 0; i < all_large_indices.size(); i++) {
    //     //         cout << i << " large " << all_large_indices[i] << endl;
    //     //     }
    //     // }

    //     // resort ss again
    //     ss->resortByPartition(all_small_indices, all_large_indices,
    //     begin_index,
    //                           end_index,
    //                           begin_index + all_small_indices.size() - 1);

    //     // For now pivot is always in the middle of the partitioned data, at
    //     the
    //     // position of (small_current_index-1)

    //     // the current pivot is at least put into the small_current_index
    //     MYASSERT((begin_index + small_current_index) >= 1);
    //     MYASSERT(ss->svec[begin_index + small_current_index - 1]
    //                  ->get_secret_share_vec_by_index(
    //                      ss->svec[begin_index + small_current_index - 1]
    //                          ->get_detect_length_k() -
    //                      1) == pivot_end_node_share);

    //     MYASSERT(
    //         (ss->svec[begin_index + small_current_index - 1]
    //              ->get_secret_share_vec_by_index(
    //                  ss->svec[begin_index + small_current_index - 1]
    //                      ->get_detect_length_k() -
    //                  1) ^
    //          ss->svec[begin_index + small_current_index - 1]->get_node_id())
    //          ==
    //         pivot_val_share);

    //     auto merge_end = system_clock::now();

    //     auto end = system_clock::now();
    //     if (party == ALICE) {
    //         cout << "[TIME] processing " << begin_index << " to " <<
    //         end_index
    //              << " using "
    //              << duration_cast<std::chrono::duration<double>>(end - start)
    //                     .count()
    //              << " (s)" << endl;
    //         cout << "[TIME] on average "
    //              << duration_cast<std::chrono::duration<double>>(end - start)
    //                         .count() /
    //                     (end_index - begin_index) * 1000
    //              << " (ms/tuple)" << endl;
    //         cout << "[TIME] extract avg "
    //              << duration_cast<std::chrono::duration<double>>(extract_end
    //              -
    //                                                              extract_start)
    //                         .count() /
    //                     (end_index - begin_index) * 1000
    //              << endl;
    //         cout << "[TIME] thread avg "
    //              << duration_cast<std::chrono::duration<double>>(thread_end -
    //                                                              thread_start)
    //                         .count() /
    //                     (end_index - begin_index) * 1000
    //              << endl;
    //         cout << "[TIME] merge avg "
    //              << duration_cast<std::chrono::duration<double>>(merge_end -
    //                                                              merge_start)
    //                         .count() /
    //                     (end_index - begin_index) * 1000
    //              << endl;
    //     }

    //     // sort the left partition
    //     if ((begin_index + small_current_index) > 1) {
    //         obliviousSortSlow(ss, begin_index,
    //                           begin_index + small_current_index - 1,
    //                           another_server_ip, party);
    //     }

    //     // sort the right partition
    //     if ((begin_index + small_current_index) < end_index) {
    //         obliviousSortSlow(ss, begin_index + small_current_index,
    //         end_index,
    //                           another_server_ip, party);
    //     }
    // }

    struct PartitionTaskInfo {
        uint32_t pivot_val_share;
        uint32_t pivot_end_node_share;
        // indices of the current task of [begin_index, end_index)
        uint64_t begin_index;
        uint64_t end_index;
        // indicies of partition range
        uint64_t partition_begin;
        uint64_t partition_end;
        // indicate whether to run a partition or sort
        // if it's true, run partition; otherwise run sort
        bool run_partition;

        // index of the pivot
        uint64_t pivot_index;

        // pivot tie breaker share
        uint64_t pivot_tie_breaker_share;
    };

    struct PartitionResult {
        vector<uint64_t> smaller_index;
        vector<uint64_t> larger_index;
    };

    struct PivotInfo {
        uint32_t pivot_val_share;
        uint32_t pivot_end_node_share;
        uint64_t pivot_index;
        uint64_t pivot_tie_breaker_share;
    };

    struct PartitionThreadsInfo {
        vector<int> threads;
        uint64_t pivot_index;
    };

    // the thread for parallel partition
    void partitionWorker(unique_ptr<ServerSecretShares>& ss,
                         vector<PartitionTaskInfo>& tasks,
                         map<pair<uint64_t, uint64_t>, PartitionResult>& res,
                         int index, int port, string another_server_ip,
                         int party) {
        TwoPcEngine engine(party, another_server_ip, port, bit_len,
                           this->tie_breaker_bit);
        engine.initEngine();

        MYASSERT(!tasks.empty());
        bool run_partition = tasks[0].run_partition;

        for (auto const& task : tasks) {
            MYASSERT(task.run_partition == run_partition);
            pair<uint64_t, uint64_t> partition_key(task.partition_begin,
                                                   task.partition_end);
            struct PartitionResult partition_res {
                .smaller_index = vector<uint64_t>(),
                .larger_index = vector<uint64_t>(),
            };
            res[partition_key] = partition_res;
            vector<uint32_t> node_val_shares, end_node_shares;
            ss->extractNodeValFromServerSecretShare(
                node_val_shares, end_node_shares, task.begin_index,
                task.end_index);
            vector<uint64_t> tie_breaker_shares;
            ss->extractTieBreakerFromServerSecretShare(
                tie_breaker_shares, task.begin_index, task.end_index);
            if (run_partition) {
                // for (uint64_t i = 0; i < node_val_shares.size(); i++) {
                //     if (node_val_shares[i] == task.pivot_val_share) {
                //         cout << i + task.begin_index << " has same val as
                //         pivot"
                //              << endl;
                //     }
                //     if (end_node_shares[i] == task.pivot_end_node_share) {
                //         cout << i + task.begin_index << " has end val as
                //         pivot"
                //              << endl;
                //     }
                // }
                uint64_t total_tuple_num = task.end_index - task.begin_index;
                if (this->partition_batch_num >= total_tuple_num ||
                    this->partition_batch_num == 0) {
                    engine.obliviousPartition(
                        node_val_shares, end_node_shares, tie_breaker_shares,
                        task.pivot_val_share, task.pivot_end_node_share,
                        task.pivot_tie_breaker_share,
                        res[partition_key].smaller_index,
                        res[partition_key].larger_index, task.begin_index, 0,
                        total_tuple_num);
                } else {
                    uint64_t curr_vec_index = 0;
                    while (curr_vec_index < total_tuple_num) {
                        uint64_t remained_tuple =
                            total_tuple_num - curr_vec_index;
                        uint64_t process_num =
                            min(remained_tuple, this->partition_batch_num);
                        engine.obliviousPartition(
                            node_val_shares, end_node_shares,
                            tie_breaker_shares, task.pivot_val_share,
                            task.pivot_end_node_share,
                            task.pivot_tie_breaker_share,
                            res[partition_key].smaller_index,
                            res[partition_key].larger_index, task.begin_index,
                            curr_vec_index, process_num);
                        curr_vec_index += process_num;
                    }
                }
            } else {
                vector<uint64_t> sorted_index;
                engine.sortElemsDirectly(node_val_shares, end_node_shares,
                                         tie_breaker_shares, task.begin_index,
                                         sorted_index);
                ss->resortBySortResult(sorted_index, task.begin_index,
                                       task.end_index);
            }
        }
        engine.terminateEngine();
    }

    void medianWorker(unique_ptr<ServerSecretShares>& ss,
                      vector<pair<uint64_t, uint64_t>>& partition_vec,
                      uint64_t begin_index_in_vec, uint64_t end_index_in_vec,
                      vector<struct PivotInfo>& pivot_vec, int port,
                      string another_server_ip, int party, int tid) {
        MYASSERT(pivot_vec.size() == partition_vec.size());
        TwoPcEngine engine(party, another_server_ip, port, bit_len,
                           this->tie_breaker_bit);
        engine.initEngine();

        int rand_times = 0;
        for (uint64_t curr_index = begin_index_in_vec;
             curr_index < end_index_in_vec; curr_index++) {
            vector<uint32_t> node_val_shares;
            vector<uint32_t> end_node_val_shares;
            vector<uint64_t> tie_breaker_shares;
            vector<uint64_t> indices;
            auto partition_range = partition_vec[curr_index];
            uint64_t num_elems = partition_range.second - partition_range.first;
            uint64_t search_rand_num =
                min(static_cast<uint64_t>(this->num_elem_to_find_median),
                    num_elems);
            unsigned int rand_seed;
            for (uint64_t i = 0; i < search_rand_num; i++) {
                rand_seed = this->rand_median_seed + port + tid + rand_times;
                uint64_t rand_idx = rand_r(&rand_seed) % num_elems;
                rand_times++;
                while (find(indices.begin(), indices.end(), rand_idx) !=
                       indices.end()) {
                    unsigned int rand_seed =
                        this->rand_median_seed + port + tid + rand_times;
                    rand_idx = rand_r(&rand_seed) % num_elems;
                    rand_times++;
                }
                indices.push_back(rand_idx);
                // cout << "party " << party << " rand idx: " << rand_idx
                //      << " random partition id: "
                //      << rand_idx + partition_range.first
                //      << " partition start: " << partition_range.first
                //      << " partition end: " << partition_range.second << endl;
            }
            // to ensure stable sort
            sort(indices.begin(), indices.end());
            for (const auto& rand_idx : indices) {
                // cout << rand_idx << " ";
                node_val_shares.push_back(
                    ss->svec[rand_idx + partition_range.first]
                        ->getNodeValShare());
                end_node_val_shares.push_back(
                    ss->svec[rand_idx + partition_range.first]
                        ->getEndNodeShare());
                tie_breaker_shares.push_back(
                    ss->svec[rand_idx + partition_range.first]
                        ->get_tie_breaker_id());
            }
            // cout << " finish all indices\n";
            uint64_t median_id_in_indicies = engine.findMedian(
                node_val_shares, end_node_val_shares, tie_breaker_shares);
            uint64_t median_id = indices[median_id_in_indicies];
            // if (party == ALICE) {
            //     cout << "median_id: " << median_id << " plus start range: "
            //          << median_id + partition_range.first << endl;
            // }
            pivot_vec[curr_index].pivot_val_share =
                node_val_shares[median_id_in_indicies];
            pivot_vec[curr_index].pivot_end_node_share =
                end_node_val_shares[median_id_in_indicies];
            pivot_vec[curr_index].pivot_tie_breaker_share =
                tie_breaker_shares[median_id_in_indicies];
            pivot_vec[curr_index].pivot_index =
                median_id + partition_range.first;
        }
        engine.terminateEngine();
    }

    void obliviousSort(unique_ptr<ServerSecretShares>& ss,
                       string another_server_ip, int party) {
        // init the partition_vec, each pair is [begin, end) where end is not
        // included in the partition range
        vector<pair<uint64_t, uint64_t>> partition_vec;

        // store the smaller partitions where the range contains less than
        // `sort_elem_threshold` elements
        vector<pair<uint64_t, uint64_t>> smaller_partition_vec;

        if (ss->size() <= sort_elem_threshold) {
            smaller_partition_vec.push_back(make_pair(0, ss->size()));
        } else {
            partition_vec.push_back(make_pair(0, ss->size()));
        }

        int round = 0;

        while (!partition_vec.empty() || !smaller_partition_vec.empty()) {
            auto round_start = system_clock::now();
            round++;
            // if (party == ALICE) {
            //     cout << "round " << round << " output ss\n";
            //     ss->print();
            // }
            uint64_t total_elem_of_partition = 0;
            for (const auto& partition_range : partition_vec) {
                MYASSERT(partition_range.second > partition_range.first);
                total_elem_of_partition +=
                    partition_range.second - partition_range.first;
            }

            uint64_t total_elem_of_sort = 0;
            for (const auto& sort_range : smaller_partition_vec) {
                MYASSERT(sort_range.second > sort_range.first);
                MYASSERT(sort_range.second - sort_range.first <=
                         sort_elem_threshold);
                // treat it as the total number of sort
                total_elem_of_sort += (sort_range.second - sort_range.first) *
                                      (sort_range.second - sort_range.first);
            }

            double partition_ratio =
                static_cast<double>(total_elem_of_partition) /
                static_cast<double>(total_elem_of_partition +
                                    total_elem_of_sort);

            int partition_thread_num = static_cast<int>(
                static_cast<double>(sort_parallel_num) * partition_ratio);

            int small_sort_thread_num =
                sort_parallel_num - partition_thread_num;

            MYASSERT(partition_thread_num <= sort_parallel_num);
            MYASSERT(partition_thread_num >= 0);

            if (partition_thread_num == 0 && total_elem_of_partition > 0) {
                partition_thread_num = 1;
                small_sort_thread_num =
                    sort_parallel_num - partition_thread_num;
            } else if (small_sort_thread_num == 0 && total_elem_of_sort > 0) {
                small_sort_thread_num = 1;
                partition_thread_num = sort_parallel_num - partition_thread_num;
            }

            MYASSERT(partition_thread_num + small_sort_thread_num ==
                     sort_parallel_num);
            MYASSERT(partition_thread_num >= 0);
            MYASSERT(small_sort_thread_num >= 0);

            // to store which threads are processing a partition data
            // (from begin_index to end_index), end_index is not included
            map<pair<uint64_t, uint64_t>, struct PartitionThreadsInfo>
                partition_per_thread_map;
            vector<vector<struct PartitionTaskInfo>> partition_info_per_thread(
                sort_parallel_num);

            // find median of each partition range
            vector<struct PivotInfo> pivot_vec(partition_vec.size());
            vector<thread> pivot_finding_threads;

            if (!partition_vec.empty()) {
                uint64_t pivot_task_num = partition_vec.size();
                uint64_t pivot_task_per_thread =
                    pivot_task_num / sort_parallel_num;
                if (pivot_task_per_thread == 0) {
                    pivot_task_per_thread = 1;
                }
                for (int i = 0; i < sort_parallel_num; i++) {
                    uint64_t pivot_begin_index_in_vec =
                        static_cast<uint64_t>(i) * pivot_task_per_thread;
                    uint64_t pivot_end_index_in_vec =
                        static_cast<uint64_t>(i + 1) * pivot_task_per_thread;
                    if (pivot_begin_index_in_vec >= pivot_task_num) {
                        break;
                    }
                    if (pivot_end_index_in_vec >= pivot_task_num) {
                        pivot_end_index_in_vec = pivot_task_num;
                    }
                    if (i == sort_parallel_num - 1) {
                        pivot_end_index_in_vec = pivot_task_num;
                    }
                    if (pivot_begin_index_in_vec == pivot_end_index_in_vec) {
                        continue;
                    }
                    thread pivot_thread(
                        &NetServer::medianWorker, this, ref(ss),
                        ref(partition_vec), pivot_begin_index_in_vec,
                        pivot_end_index_in_vec, ref(pivot_vec), start_port + i,
                        another_server_ip, party, i);
                    pivot_finding_threads.push_back(move(pivot_thread));
                }
                for (auto& t : pivot_finding_threads) {
                    t.join();
                }
            }

            // generate partition range for large partition which still uses
            // quick sort.
            if (partition_thread_num > 0) {
                /*start partition*/
                MYASSERT(partition_vec.size() > 0);
                uint64_t current_partition_index = 0;
                uint64_t current_begin = partition_vec[0].first;
                uint64_t current_end = partition_vec[0].second;

                uint64_t elem_per_thread_of_partition =
                    total_elem_of_partition / partition_thread_num;
                cout << "processing " << total_elem_of_partition
                     << " for partition in total, using "
                     << partition_thread_num
                     << " threads, each thread processes "
                     << elem_per_thread_of_partition << endl;

                for (int i = 0; i < partition_thread_num; i++) {
                    uint64_t begin = i * elem_per_thread_of_partition;
                    uint64_t end = (i + 1) * elem_per_thread_of_partition;
                    if (i == partition_thread_num - 1) {
                        end = total_elem_of_partition;
                    }

                    // no task to do continue to next round
                    if (begin == end) {
                        // cout << "[THREAD] " << i
                        //      << " has no partition task, skip it" << endl;
                        continue;
                    }

                    uint64_t curr_ptr = 0;
                    while (curr_ptr < (end - begin)) {
                        // cout << "[THREAD] " << i << " curr_ptr: " << curr_ptr
                        //      << " num_of_elem: " << end - begin << endl;
                        MYASSERT(current_begin < current_end);
                        pair<uint64_t, uint64_t> partition_index_key =
                            partition_vec[current_partition_index];
                        if ((current_begin + (end - begin - curr_ptr)) <=
                            current_end) {
                            // if the current parition has enough data than what
                            // is required to fill [curr_ptr, end)
                            struct PartitionTaskInfo info {
                                .pivot_val_share =
                                    pivot_vec[current_partition_index]
                                        .pivot_val_share,
                                .pivot_end_node_share =
                                    pivot_vec[current_partition_index]
                                        .pivot_end_node_share,
                                .begin_index = current_begin,
                                .end_index =
                                    current_begin + (end - begin - curr_ptr),
                                .partition_begin = partition_index_key.first,
                                .partition_end = partition_index_key.second,
                                .run_partition = true,
                                .pivot_index =
                                    pivot_vec[current_partition_index]
                                        .pivot_index,
                                .pivot_tie_breaker_share =
                                    pivot_vec[current_partition_index]
                                        .pivot_tie_breaker_share,
                            };

                            // add the current thread to the task
                            if (partition_per_thread_map.find(
                                    partition_index_key) ==
                                partition_per_thread_map.end()) {
                                partition_per_thread_map[partition_index_key] =
                                    {
                                        .threads = vector<int>(),
                                        .pivot_index =
                                            pivot_vec[current_partition_index]
                                                .pivot_index,
                                    };
                            }
                            partition_per_thread_map[partition_index_key]
                                .threads.push_back(i);

                            partition_info_per_thread[i].push_back(info);

                            current_begin += (end - begin - curr_ptr);
                            curr_ptr += (end - begin - curr_ptr);

                            // if (party == ALICE) {
                            //     cout << "partition info 1: " << end << " "
                            //          << current_partition_index << " "
                            //          << partition_vec.size()
                            //          << " current_begin: " << current_begin
                            //          << " current_end: " << current_end
                            //          << ", end: " << end << ", begin: " <<
                            //          begin
                            //          << ", curr_ptr: " << curr_ptr
                            //          << ", thread: " << i << endl;
                            //     for (const auto& partition_range :
                            //     partition_vec)
                            //     {
                            //         cout << partition_range.first << " "
                            //              << partition_range.second << endl;
                            //     }
                            // }

                            // the current partition data has been consumed in
                            // full, proceed to next partition range
                            if ((current_begin == current_end) &&
                                (i != partition_thread_num - 1)) {
                                // increase the current_partition_index by 1
                                current_partition_index += 1;
                                MYASSERT(current_partition_index <
                                         partition_vec.size());
                                current_end =
                                    partition_vec[current_partition_index]
                                        .second;
                                current_begin =
                                    partition_vec[current_partition_index]
                                        .first;
                            }
                        } else {
                            // the current parition does not have enough data
                            // than what is required to fill [begin, end)
                            struct PartitionTaskInfo info {
                                .pivot_val_share =
                                    pivot_vec[current_partition_index]
                                        .pivot_val_share,
                                .pivot_end_node_share =
                                    pivot_vec[current_partition_index]
                                        .pivot_end_node_share,
                                .begin_index = current_begin,
                                .end_index = current_end,
                                .partition_begin = partition_index_key.first,
                                .partition_end = partition_index_key.second,
                                .run_partition = true,
                                .pivot_index =
                                    pivot_vec[current_partition_index]
                                        .pivot_index,
                                .pivot_tie_breaker_share =
                                    pivot_vec[current_partition_index]
                                        .pivot_tie_breaker_share,
                            };

                            partition_info_per_thread[i].push_back(info);

                            // add the current thread to the task
                            if (partition_per_thread_map.find(
                                    partition_index_key) ==
                                partition_per_thread_map.end()) {
                                partition_per_thread_map[partition_index_key] =
                                    {
                                        .threads = vector<int>(),
                                        .pivot_index =
                                            pivot_vec[current_partition_index]
                                                .pivot_index,
                                    };
                            }
                            partition_per_thread_map[partition_index_key]
                                .threads.push_back(i);
                            // if (party == ALICE) {
                            //     cout << "partition info 2: " << end << " "
                            //          << current_partition_index << " "
                            //          << partition_vec.size()
                            //          << " current_begin: " << current_begin
                            //          << " current_end: " << current_end
                            //          << ", end: " << end << ", begin: " <<
                            //          begin
                            //          << ", curr_ptr: " << curr_ptr
                            //          << ", thread: " << i << endl;
                            //     for (const auto& partition_range :
                            //     partition_vec)
                            //     {
                            //         cout << partition_range.first << " "
                            //              << partition_range.second << endl;
                            //     }
                            // }

                            curr_ptr += (current_end - current_begin);

                            // if this is the last thread, and it has finished
                            // assigning partition for all threads
                            if (curr_ptr == (end - begin) &&
                                i == partition_thread_num - 1) {
                                break;
                            }

                            // increase the current_partition_index by 1
                            current_partition_index += 1;
                            MYASSERT(current_partition_index <
                                     partition_vec.size());
                            current_end =
                                partition_vec[current_partition_index].second;
                            current_begin =
                                partition_vec[current_partition_index].first;
                        }
                    }
                }
            }

            // generate partition range for smaller partition which directly
            // sorts everything
            // the results do not need to be stored
            if (small_sort_thread_num > 0) {
                uint64_t total_sort_range_num = smaller_partition_vec.size();
                uint64_t partition_range_for_sort_per_thread =
                    total_sort_range_num / small_sort_thread_num;
                if (partition_range_for_sort_per_thread == 0) {
                    partition_range_for_sort_per_thread = 1;
                }
                cout << "processing " << total_elem_of_sort
                     << " for sort in total, using " << small_sort_thread_num
                     << " threads, each thread processes "
                     << partition_range_for_sort_per_thread
                     << " ranges (instead of tuples)" << endl;
                for (int i = 0; i < small_sort_thread_num; i++) {
                    uint64_t begin_index_in_vec =
                        static_cast<uint64_t>(i) *
                        partition_range_for_sort_per_thread;
                    uint64_t end_index_in_vec =
                        static_cast<uint64_t>(i + 1) *
                        partition_range_for_sort_per_thread;
                    if (begin_index_in_vec >= total_sort_range_num) {
                        break;
                    }
                    if (end_index_in_vec >= total_sort_range_num) {
                        end_index_in_vec = total_sort_range_num;
                    }

                    if (i == small_sort_thread_num - 1) {
                        end_index_in_vec = total_sort_range_num;
                    }
                    if (begin_index_in_vec == end_index_in_vec) {
                        continue;
                    }
                    for (uint64_t curr_index_in_vec = begin_index_in_vec;
                         curr_index_in_vec < end_index_in_vec;
                         curr_index_in_vec++) {
                        struct PartitionTaskInfo info {
                            .begin_index =
                                smaller_partition_vec[curr_index_in_vec].first,
                            .end_index =
                                smaller_partition_vec[curr_index_in_vec].second,
                            .partition_begin =
                                smaller_partition_vec[curr_index_in_vec].first,
                            .partition_end =
                                smaller_partition_vec[curr_index_in_vec].second,
                            .run_partition = false,
                        };
                        MYASSERT(i + partition_thread_num < sort_parallel_num);
                        partition_info_per_thread[i + partition_thread_num]
                            .push_back(info);
                    }
                }
            }

            vector<map<pair<uint64_t, uint64_t>, PartitionResult>>
                partition_res_per_thread(sort_parallel_num);

            vector<thread> t_vec;
            // spawn threads to launch partition tasks
            for (int i = 0; i < sort_parallel_num; i++) {
                if (partition_info_per_thread[i].empty()) {
                    cout << "[THREAD] " << i
                         << " no task, do not spawn a thread" << endl;
                    continue;
                }
                thread t(&NetServer::partitionWorker, this, ref(ss),
                         ref(partition_info_per_thread[i]),
                         ref(partition_res_per_thread[i]), i, start_port + i,
                         another_server_ip, party);
                t_vec.push_back(move(t));
            }

            for (auto& t : t_vec) {
                t.join();
            }

            // clear partition_vec so it could store data for next round
            partition_vec.clear();
            smaller_partition_vec.clear();

            uint64_t larger_elem_num_after_partition = 0;
            uint64_t smaller_elem_num_after_partition = 0;
            // re-sort the partitioned data using partition data
            for (const auto& kv : partition_per_thread_map) {
                pair<uint64_t, uint64_t> partition_range = kv.first;
                vector<int> target_threads = kv.second.threads;
                // merge smaller_index and larger_index
                uint64_t small_index_size = 0;
                uint64_t large_index_size = 0;
                for (const auto& tid : target_threads) {
                    MYASSERT(
                        partition_res_per_thread[tid].find(partition_range) !=
                        partition_res_per_thread[tid].end());
                    small_index_size +=
                        partition_res_per_thread[tid][partition_range]
                            .smaller_index.size();
                    large_index_size +=
                        partition_res_per_thread[tid][partition_range]
                            .larger_index.size();
                }

                vector<uint64_t> all_small_indices(small_index_size);
                vector<uint64_t> all_large_indices(large_index_size);
                uint64_t small_current_index = 0;
                uint64_t large_current_index = 0;
                for (const auto& tid : target_threads) {
                    copy(partition_res_per_thread[tid][partition_range]
                             .smaller_index.begin(),
                         partition_res_per_thread[tid][partition_range]
                             .smaller_index.end(),
                         all_small_indices.begin() + small_current_index);
                    copy(partition_res_per_thread[tid][partition_range]
                             .larger_index.begin(),
                         partition_res_per_thread[tid][partition_range]
                             .larger_index.end(),
                         all_large_indices.begin() + large_current_index);
                    small_current_index +=
                        partition_res_per_thread[tid][partition_range]
                            .smaller_index.size();
                    large_current_index +=
                        partition_res_per_thread[tid][partition_range]
                            .larger_index.size();
                }

                larger_elem_num_after_partition += all_large_indices.size();
                smaller_elem_num_after_partition += all_small_indices.size();

                MYASSERT(all_small_indices.size() + all_large_indices.size() ==
                         (partition_range.second - partition_range.first));

                // if (party == ALICE) {
                //     cout << "begin_index: " << partition_range.first << endl;
                //     cout << "end_index: " << partition_range.second << endl;
                //     cout << "small_current_index: " << small_current_index
                //          << endl;
                //     for (uint32_t i = 0; i < all_small_indices.size(); i++) {
                //         cout << i << " small " << all_small_indices[i] <<
                //         endl;
                //     }
                //     for (uint32_t i = 0; i < all_large_indices.size(); i++) {
                //         cout << i << " large " << all_large_indices[i] <<
                //         endl;
                //     }
                // }

                // resort the partial secret shares
                ss->resortByPartition(
                    all_small_indices, all_large_indices, partition_range.first,
                    partition_range.second, kv.second.pivot_index);

                // update partition_vec for next round computation

                // sort the left partition if there is at least two elements
                if (((partition_range.first + small_current_index) > 1) &&
                    (partition_range.first + 1 <
                     partition_range.first + small_current_index - 1)) {
                    // if (party == ALICE) {
                    //     cout << "add new left partition tasks from "
                    //          << partition_range.first << " to "
                    //          << partition_range.first +
                    //          small_current_index -
                    //          1
                    //          << endl;
                    // }
                    if (small_current_index - 1 > sort_elem_threshold) {
                        partition_vec.push_back(make_pair(
                            partition_range.first,
                            partition_range.first + small_current_index - 1));
                    } else {
                        smaller_partition_vec.push_back(make_pair(
                            partition_range.first,
                            partition_range.first + small_current_index - 1));
                    }
                }

                // sort the right partition if there is at least two
                // elements
                if ((partition_range.first + small_current_index + 1) <
                    partition_range.second) {
                    // if (party == ALICE) {
                    //     cout << "add new right partition tasks from "
                    //          << partition_range.first +
                    //          small_current_index
                    //          << " to " << partition_range.second << endl;
                    // }
                    if (partition_range.second -
                            (partition_range.first + small_current_index) >
                        sort_elem_threshold) {
                        partition_vec.push_back(make_pair(
                            partition_range.first + small_current_index,
                            partition_range.second));
                    } else {
                        smaller_partition_vec.push_back(make_pair(
                            partition_range.first + small_current_index,
                            partition_range.second));
                    }
                }
            }

            auto round_end = system_clock::now();
            cout << round << " round takes "
                 << duration_cast<std::chrono::duration<double>>(round_end -
                                                                 round_start)
                        .count()
                 << endl;
            cout << "larger/smaller ratio in round " << round << " is "
                 << static_cast<double>(larger_elem_num_after_partition) /
                        static_cast<double>(smaller_elem_num_after_partition +
                                            larger_elem_num_after_partition)
                 << endl;
        }
        cout << "sort runs " << round << " rounds in total\n";
    }

    void filterThread(int port, string another_server_ip, int party,
                      unique_ptr<ServerSecretShares>& ss, uint64_t begin_index,
                      uint64_t end_index, vector<bool>& is_valid,
                      vector<bool>& is_cycle) {
        // extract path shares from [begin_index, end_index)
        vector<uint32_t> path_shares;
        ss->extractSinglePathFromRange(path_shares, begin_index, end_index);

        // pass to engine
        TwoPcEngine engine(party, another_server_ip, port, bit_len,
                           this->tie_breaker_bit);
        engine.initEngine();
        is_valid = vector<bool>((end_index - begin_index), false);
        is_cycle = vector<bool>((end_index - begin_index), false);
        if (this->filter_batch_num == 0 ||
            ((end_index - begin_index) <= this->filter_batch_num)) {
            engine.obliviousFilter(path_shares, 0, end_index - begin_index,
                                   this->detect_length_k, is_valid, is_cycle);
        } else {
            cout << "run in batch mode\n";
            uint64_t begin_tuple_index = 0;
            uint64_t total_tuple_num = end_index - begin_index;
            while (begin_tuple_index < total_tuple_num) {
                uint64_t remained_tuple_num =
                    total_tuple_num - begin_tuple_index;
                uint64_t processed_tuple_num =
                    min(this->filter_batch_num, remained_tuple_num);
                engine.obliviousFilter(path_shares, begin_tuple_index,
                                       processed_tuple_num, detect_length_k,
                                       is_valid, is_cycle);
                begin_tuple_index += processed_tuple_num;
            }
            MYASSERT(begin_tuple_index == total_tuple_num);
        }
        engine.terminateEngine();
    }

    void oblivousFilter(unique_ptr<ServerSecretShares>& ss,
                        string another_server_ip, int party,
                        vector<bool>& is_valid, vector<bool>& is_cycle) {
        MYASSERT(is_valid.empty());
        MYASSERT(is_cycle.empty());
        uint64_t total_tuple_num = ss->size();
        uint64_t tuple_per_elem = total_tuple_num / filter_parallel_num;
        vector<vector<bool>> is_valid_per_thread(filter_parallel_num);
        vector<vector<bool>> is_cycle_per_thread(filter_parallel_num);
        vector<thread> t_vec;
        for (int i = 0; i < filter_parallel_num; i++) {
            uint64_t begin_index = static_cast<uint64_t>(i) * tuple_per_elem;
            uint64_t end_index = static_cast<uint64_t>(i + 1) * tuple_per_elem;
            if (begin_index >= total_tuple_num) {
                break;
            }
            if ((i == filter_parallel_num - 1) ||
                (end_index >= total_tuple_num)) {
                end_index = total_tuple_num;
            }
            thread t(&NetServer::filterThread, this, start_port + i,
                     another_server_ip, party, ref(ss), begin_index, end_index,
                     ref(is_valid_per_thread[i]), ref(is_cycle_per_thread[i]));
            t_vec.push_back(move(t));
        }
        for (auto& t : t_vec) {
            t.join();
        }

        // merge results of is_valid and is_cycle
        uint64_t cur_ptr = 0;
        is_valid = vector<bool>(total_tuple_num);
        is_cycle = vector<bool>(total_tuple_num);
        for (int i = 0; i < filter_parallel_num; i++) {
            MYASSERT(is_valid_per_thread[i].size() ==
                     is_cycle_per_thread[i].size());
            copy(is_valid_per_thread[i].begin(), is_valid_per_thread[i].end(),
                 is_valid.begin() + cur_ptr);
            copy(is_cycle_per_thread[i].begin(), is_cycle_per_thread[i].end(),
                 is_cycle.begin() + cur_ptr);
            cur_ptr += is_valid_per_thread[i].size();
        }
        MYASSERT(cur_ptr == total_tuple_num);
    }

    void extractPathThread(int port, string another_server_ip, int party,
                           unique_ptr<ServerSecretShares>& ss,
                           uint64_t begin_index, uint64_t end_index,
                           vector<uint64_t>& edge_tuple_indices) {
        // extract path shares from [begin_index, end_index)
        vector<uint32_t> node_shares;
        ss->extractNodeIdSharesOnly(node_shares, begin_index, end_index);

        uint64_t total_tuple_num = end_index - begin_index;

        // pass to engine
        TwoPcEngine engine(party, another_server_ip, port, bit_len,
                           this->tie_breaker_bit);
        engine.initEngine();
        if (this->extract_batch_num == 0 ||
            this->extract_batch_num >= total_tuple_num) {
            engine.obliviousExtractPath(node_shares, edge_tuple_indices,
                                        begin_index, end_index, 0,
                                        total_tuple_num);
        } else {
            cout << "run extract in batch mode\n";
            uint64_t curr_begin_index = 0;
            while (curr_begin_index < total_tuple_num) {
                uint64_t remained_processed_num =
                    total_tuple_num - curr_begin_index;
                uint64_t processed_num =
                    min(remained_processed_num, this->extract_batch_num);
                engine.obliviousExtractPath(node_shares, edge_tuple_indices,
                                            begin_index, end_index,
                                            curr_begin_index, processed_num);
                curr_begin_index += processed_num;
            }
        }
        engine.terminateEngine();
    }

    // extract edges and paths from ss respectively
    // ss is cleared as all pointers in ss will be moved away
    void obliviousExtractPathAndEdges(unique_ptr<ServerSecretShares>& ss,
                                      string another_server_ip, int party,
                                      unique_ptr<ServerSecretShares>& edges,
                                      unique_ptr<ServerSecretShares>& paths) {
        MYASSERT(edges->size() == 0);
        MYASSERT(paths->size() == 0);
        uint64_t total_tuple_num = ss->size();
        uint64_t tuple_per_elem =
            total_tuple_num / this->extract_path_parallel_num;
        vector<vector<uint64_t>> edge_indices_per_thread(
            this->extract_path_parallel_num);
        vector<thread> t_vec;
        // auto start = system_clock::now();
        for (int i = 0; i < this->extract_path_parallel_num; i++) {
            uint64_t begin_index = static_cast<uint64_t>(i) * tuple_per_elem;
            uint64_t end_index = static_cast<uint64_t>(i + 1) * tuple_per_elem;
            if (begin_index >= total_tuple_num) {
                break;
            }
            if ((i == this->extract_path_parallel_num - 1) ||
                (end_index >= total_tuple_num)) {
                end_index = total_tuple_num;
            }
            thread t(&NetServer::extractPathThread, this, start_port + i,
                     another_server_ip, party, ref(ss), begin_index, end_index,
                     ref(edge_indices_per_thread[i]));
            t_vec.push_back(move(t));
        }
        for (auto& t : t_vec) {
            t.join();
        }
        // auto end = system_clock::now();
        // cout
        //     << "[TIME] extract threads takes "
        //     << duration_cast<std::chrono::duration<double>>(end -
        //     start).count()
        //     << " seconds\n";

        // start = system_clock::now();

        // move unique_ptr from ss into edges or paths
        uint64_t cur_thread_id = 0;
        while (edge_indices_per_thread[cur_thread_id].empty()) {
            cur_thread_id++;
            // impossible that there are no edge tuples
            MYASSERT(cur_thread_id <
                     static_cast<uint64_t>(this->extract_path_parallel_num));
        }
        uint64_t cur_edge_index = 0;

        bool all_edges_filled = false;
        for (uint64_t i = 0; i < total_tuple_num; i++) {
            // the current tuple is an edge tuple and not all edges have been
            // filled
            if (!all_edges_filled &&
                (i == edge_indices_per_thread[cur_thread_id][cur_edge_index])) {
                edges->insert(ss->svec[i]);

                // move index by one more
                cur_edge_index++;
                // current edge vector of this thread
                if (cur_edge_index >=
                    edge_indices_per_thread[cur_thread_id].size()) {
                    cur_thread_id++;
                    if (cur_thread_id == static_cast<uint64_t>(
                                             this->extract_path_parallel_num)) {
                        all_edges_filled = true;
                        continue;
                    }
                    while (edge_indices_per_thread[cur_thread_id].empty()) {
                        cur_thread_id++;
                        // impossible that there are no edge tuples
                        if (cur_thread_id ==
                            static_cast<uint64_t>(
                                this->extract_path_parallel_num)) {
                            all_edges_filled = true;
                            continue;
                        }
                    }
                    cur_edge_index = 0;
                }
            } else {
                paths->insert(ss->svec[i]);
            }
        }

        // end = system_clock::now();
        // cout
        //     << "[TIME] extract move edges and paths takes "
        //     << duration_cast<std::chrono::duration<double>>(end -
        //     start).count()
        //     << " seconds\n";

        // empty ss
        ss->clear();
    }

    void listenThread(string ip_addr, int* client_fd) {
        int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
        int sock_opt = 1;
        MYASSERT(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &sock_opt,
                            sizeof(sock_opt)) >= 0);
        struct sockaddr_in servaddr = string_to_struct_addr(ip_addr);
        if (bind(listen_fd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            fprintf(stderr, "bind failed: %s\n", strerror(errno));
        }
        MYASSERT(listen(listen_fd, 100) >= 0);
        cout << "start listening on " << ip_addr << endl;
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        int fd =
            accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
        MYASSERT(fd > 0);
        *client_fd = fd;
        cout << "conn with client with fd " << *client_fd;
        string recv_msg;
        MYASSERT(recvMsg(*client_fd, recv_msg));
        cout << "received test msg: " << recv_msg << endl;
        close(listen_fd);
    }

    void connectThread(string ip, int* target_fd) {
        // manually delay connection to the server to ensure that the server is
        // actively listening on the port
        sleep(15);
        cout << "try to connect to " << ip << endl;
        *target_fd = connect_to_addr(ip);
        MYASSERT(target_fd > 0);
        cout << "connected to " << ip << " with fd " << *target_fd << endl;
        MYASSERT(sendMsg(*target_fd, ip));
    }

    // three server-client pairs
    // s: s1, c: s2
    // s: s2, c: s3
    // s: s3, c: s1
    void connectWithOtherServers() {
        if (server_id == SERVER_ID::S1) {
            thread t1(&NetServer::listenThread, this, ip_1, &fd_2);
            thread t2(&NetServer::connectThread, this, ip_3, &fd_3);
            t1.join();
            t2.join();
        } else if (server_id == SERVER_ID::S2) {
            thread t1(&NetServer::listenThread, this, ip_2, &fd_3);
            thread t2(&NetServer::connectThread, this, ip_1, &fd_1);
            t1.join();
            t2.join();
        } else if (server_id == SERVER_ID::S3) {
            thread t1(&NetServer::listenThread, this, ip_3, &fd_1);
            thread t2(&NetServer::connectThread, this, ip_2, &fd_2);
            t1.join();
            t2.join();
        } else {
            MYASSERT(0);
        }
    }

    void closeConnectionWithOtherServers() {
        if (server_id == SERVER_ID::S1) {
            close(fd_2);
            close(fd_3);
        } else if (server_id == SERVER_ID::S2) {
            close(fd_3);
            close(fd_1);
        } else if (server_id == SERVER_ID::S3) {
            close(fd_1);
            close(fd_2);
        } else {
            MYASSERT(0);
        }
    }

    void validateFds() {
        if (server_id == SERVER_ID::S1) {
            // cout << fd_2 << " " << fd_3;
            MYASSERT(fd_2 > 0);
            MYASSERT(fd_3 > 0);
        } else if (server_id == SERVER_ID::S2) {
            // cout << fd_1 << " " << fd_3;
            MYASSERT(fd_1 > 0);
            MYASSERT(fd_3 > 0);
        } else if (server_id == SERVER_ID::S3) {
            // cout << fd_1 << " " << fd_2;
            MYASSERT(fd_1 > 0);
            MYASSERT(fd_2 > 0);
        } else {
            MYASSERT(0);
        }
    }

    void incDetectLength() {
        this->detect_length_k++;
        compute_server->incDetectLength();
    }

    void sendServerSecretShareWorker(ServerSecretShares* ss, int fd) {
        string serialized_str =
            ss->serializeParallel(this->local_compute_parallel_num);
        cout << "send ss with size: " << serialized_str.size() << endl;
        bool success = sendMsg(fd, serialized_str);
        MYASSERT(success);
    }

    void recvServerSecretShareWorker(unique_ptr<ServerSecretShares>& ss, int fd,
                                     int curr_max_degree,
                                     int curr_detect_length_k) {
        MYASSERT(ss->size() == 0);
        string serialized_str;
        bool success = recvMsg(fd, serialized_str);
        MYASSERT(success);
        unique_ptr<ServerSecretShares> deserialized_ss =
            make_unique<ServerSecretShares>(serialized_str, curr_max_degree,
                                            curr_detect_length_k,
                                            this->local_compute_parallel_num);
        ss = move(deserialized_ss);
        MYASSERT(ss->size() != 0);
    }

    void recvServerSecretShareToPtrWorker(ServerSecretShares* ss, int fd,
                                          int curr_max_degree,
                                          int curr_detect_length_k) {
        MYASSERT(ss->size() == 0);
        string serialized_str;
        bool success = recvMsg(fd, serialized_str);
        MYASSERT(success);
        ss->fillServerSecretSharesFromStr(serialized_str, curr_max_degree,
                                          curr_detect_length_k,
                                          this->local_compute_parallel_num);
        MYASSERT(ss->size() != 0);
    }

    // [helper func for debugging]
    void reconCurrentSS(int curr_max_degree, int curr_detect_length_k) {
        if (server_id == SERVER_ID::S1) {
            // real ss2
            unique_ptr<ServerSecretShares> s2_1 =
                make_unique<ServerSecretShares>();
            // real ss3
            unique_ptr<ServerSecretShares> s2_2 =
                make_unique<ServerSecretShares>();
            // real ss3
            unique_ptr<ServerSecretShares> s3_1 =
                make_unique<ServerSecretShares>();
            // real ss1
            unique_ptr<ServerSecretShares> s3_2 =
                make_unique<ServerSecretShares>();

            thread recvS2_1(&NetServer::recvServerSecretShareWorker, this,
                            ref(s2_1), fd_2, curr_max_degree,
                            curr_detect_length_k);
            thread recvS3_1(&NetServer::recvServerSecretShareWorker, this,
                            ref(s3_1), fd_3, curr_max_degree,
                            curr_detect_length_k);
            recvS2_1.join();
            recvS3_1.join();

            thread recvS2_2(&NetServer::recvServerSecretShareWorker, this,
                            ref(s2_2), fd_2, curr_max_degree,
                            curr_detect_length_k);
            thread recvS3_2(&NetServer::recvServerSecretShareWorker, this,
                            ref(s3_2), fd_3, curr_max_degree,
                            curr_detect_length_k);
            recvS2_2.join();
            recvS3_2.join();

            // start checking each share is the same
            MYASSERT(*getFirstServerSecretShares() == *s3_2.get());
            cout << "ss1 equal\n";
            MYASSERT(*s2_1.get() == *getSecondServerSecretShares());
            cout << "ss2 equal\n";
            MYASSERT(*s2_2.get() == *s3_1.get());
            cout << "ss3 equal\n";

            vector<ServerSecretShares*> recon_vec = {
                getFirstServerSecretShares(), getSecondServerSecretShares(),
                s3_1.get()};

            SecretShareTupleHelper helper(this->id_range,
                                          this->tie_breaker_range);
            unique_ptr<ServerSecretShares> recon_ss = helper.reconServerShares(
                recon_vec, curr_max_degree, curr_detect_length_k);
            recon_ss->print();
        } else if (server_id == SERVER_ID::S2 || server_id == SERVER_ID::S3) {
            thread sendS1(&NetServer::sendServerSecretShareWorker, this,
                          getFirstServerSecretShares(), fd_1);
            sendS1.join();
            thread sendS2(&NetServer::sendServerSecretShareWorker, this,
                          getSecondServerSecretShares(), fd_1);
            sendS2.join();
        } else {
            MYASSERT(0);
        }
    }

    // [helper func for debugging]
    void reconWithTwoPCShare(unique_ptr<ServerSecretShares>& ss) {
        if (server_id == SERVER_ID::S1) {
            unique_ptr<ServerSecretShares> s2_ss =
                make_unique<ServerSecretShares>();
            thread recvSS(&NetServer::recvServerSecretShareWorker, this,
                          ref(s2_ss), fd_2, this->max_degree,
                          this->detect_length_k);
            recvSS.join();
            cout << "server1 finish recving local share from server2\n";
            vector<ServerSecretShares*> xor_vec = {ss.get(), s2_ss.get()};
            unique_ptr<ServerSecretShares> recon_ss =
                compute_server->xorSecretShareTupleAndCreateDst(xor_vec);
            recon_ss->print();
            return;
        } else if (server_id == SERVER_ID::S2) {
            thread sendSS(&NetServer::sendServerSecretShareWorker, this,
                          ss.get(), fd_1);
            sendSS.join();
            cout << "server2 finish sending local share to server1\n";
            return;
        } else if (server_id == SERVER_ID::S3) {
            return;
        } else {
            MYASSERT(0);
        }
    }

    // for shuffle only
    void waitForOtherServers() {
        string msg = "ack";
        string recv_msg1, recv_msg2;
        if (server_id == SERVER_ID::S1) {
            sendMsg(this->fd_2, msg);
            sendMsg(this->fd_3, msg);
            recvMsg(this->fd_2, recv_msg1);
            recvMsg(this->fd_3, recv_msg2);
        } else if (server_id == SERVER_ID::S2) {
            sendMsg(this->fd_1, msg);
            sendMsg(this->fd_3, msg);
            recvMsg(this->fd_1, recv_msg1);
            recvMsg(this->fd_3, recv_msg2);
        } else if (server_id == SERVER_ID::S3) {
            sendMsg(this->fd_1, msg);
            sendMsg(this->fd_2, msg);
            recvMsg(this->fd_1, recv_msg1);
            recvMsg(this->fd_2, recv_msg2);
        } else {
            MYASSERT(0);
        }
    }

    // [WARNING] to invoke this wrapper, make sure that first_s and second_s
    // are updated correctly
    void shuffleWrapper(int curr_max_degree, int curr_detect_length_k) {
        if (this->server_id == SERVER_ID::S1) {
            unique_ptr<ServerSecretShares> x2 =
                compute_server->shuffle_s1_step1();
            thread sendX2(&NetServer::sendServerSecretShareWorker, this,
                          x2.get(), fd_2);
            sendX2.join();
            cout << "finish sending x2 to server2\n";
            cout << "[INFO] shuffle S1 waiting for other servers\n";
            waitForOtherServers();
            cout << "[INFO] shuffle S1 waiting for other servers\n";

        } else if (this->server_id == SERVER_ID::S2) {
            unique_ptr<ServerSecretShares> y1 =
                compute_server->shuffle_s2_step1();
            thread sendY1(&NetServer::sendServerSecretShareWorker, this,
                          y1.get(), fd_3);
            unique_ptr<ServerSecretShares> x2 =
                make_unique<ServerSecretShares>();
            thread recvX2(&NetServer::recvServerSecretShareWorker, this,
                          ref(x2), fd_1, curr_max_degree, curr_detect_length_k);
            sendY1.join();
            cout << "finish sending y1 to server3\n";
            recvX2.join();
            cout << "finish recving x2 from server3\n";

            unique_ptr<ServerSecretShares> c1 =
                compute_server->shuffle_s2_step2(*x2.get());
            thread sendC1(&NetServer::sendServerSecretShareWorker, this,
                          c1.get(), fd_3);
            unique_ptr<ServerSecretShares> c2 =
                make_unique<ServerSecretShares>();
            thread recvC2(&NetServer::recvServerSecretShareWorker, this,
                          ref(c2), fd_3, curr_max_degree, curr_detect_length_k);
            sendC1.join();
            cout << "finish sending c1 to server3\n";
            recvC2.join();
            cout << "finish recving c2 from server3\n";

            compute_server->shuffle_s2_step3(*c1.get(), *c2.get());
            cout << "[INFO] shuffle S2 waiting for other servers\n";
            waitForOtherServers();
            cout << "[INFO] shuffle S2 waiting for other servers\n";
        } else if (this->server_id == SERVER_ID::S3) {
            unique_ptr<ServerSecretShares> y1 =
                make_unique<ServerSecretShares>();
            thread recvY1(&NetServer::recvServerSecretShareWorker, this,
                          ref(y1), fd_2, curr_max_degree, curr_detect_length_k);
            recvY1.join();
            cout << "finish recving y1 from server2\n";

            unique_ptr<ServerSecretShares> c2 =
                compute_server->shuffle_s3_step1(*y1.get());
            thread sendC2(&NetServer::sendServerSecretShareWorker, this,
                          c2.get(), fd_2);

            unique_ptr<ServerSecretShares> c1 =
                make_unique<ServerSecretShares>();
            thread recvC1(&NetServer::recvServerSecretShareWorker, this,
                          ref(c1), fd_2, curr_max_degree, curr_detect_length_k);

            sendC2.join();
            cout << "finish sending c2 from server2\n";
            recvC1.join();
            cout << "finish receiving c1 from server2\n";

            compute_server->shuffle_s3_step2(*c1.get(), *c2.get());
            cout << "[INFO] shuffle S3 waiting for other servers\n";
            waitForOtherServers();
            cout << "[INFO] shuffle S3 waiting for other servers\n";
        } else {
            MYASSERT(0);
        }
    }

    // note that after calling this, S2's second_s becomes all invalid ptr
    unique_ptr<ServerSecretShares> computeTwoPCShare() {
        if (server_id == SERVER_ID::S1) {
            // ss = ss1^ss2
            vector<ServerSecretShares*> xor_vec = {
                getFirstServerSecretShares(), getSecondServerSecretShares()};
            unique_ptr<ServerSecretShares> ss =
                compute_server->xorSecretShareTupleAndCreateDst(xor_vec);
            return ss;
        } else if (server_id == SERVER_ID::S2) {
            unique_ptr<ServerSecretShares> ss =
                make_unique<ServerSecretShares>(getSecondServerSecretShares());
            return ss;
        } else if (server_id == SERVER_ID::S3) {
            // create an empty ss as it's not used
            unique_ptr<ServerSecretShares> ss =
                make_unique<ServerSecretShares>();
            return ss;
        } else {
            MYASSERT(0);
        }
    }

    void sortWrapper(unique_ptr<ServerSecretShares>& ss) {
        if (server_id == SERVER_ID::S1) {
            // ss = ss1^ss2
            this->obliviousSort(ss, ip_no_port_1, ALICE);
            return;
        } else if (server_id == SERVER_ID::S2) {
            this->obliviousSort(ss, ip_no_port_1, BOB);
            return;
        } else if (server_id == SERVER_ID::S3) {
            return;
        } else {
            MYASSERT(0);
        }
    }

    void neighborPassWrapper(unique_ptr<ServerSecretShares>& ss) {
        if (server_id == SERVER_ID::S1) {
            // ss = ss1^ss2
            this->obliviousNeighborPass(ss, ip_no_port_1, ALICE);
            return;
        } else if (server_id == SERVER_ID::S2) {
            this->obliviousNeighborPass(ss, ip_no_port_1, BOB);
            return;
        } else if (server_id == SERVER_ID::S3) {
            return;
        } else {
            MYASSERT(0);
        }
    }

    void extractWrapper(unique_ptr<ServerSecretShares>& ss,
                        unique_ptr<ServerSecretShares>& edge_shares,
                        unique_ptr<ServerSecretShares>& path_shares) {
        if (server_id == SERVER_ID::S1) {
            this->obliviousExtractPathAndEdges(ss, ip_no_port_1, ALICE,
                                               edge_shares, path_shares);
            return;
        } else if (server_id == SERVER_ID::S2) {
            this->obliviousExtractPathAndEdges(ss, ip_no_port_1, BOB,
                                               edge_shares, path_shares);
            return;
        } else if (server_id == SERVER_ID::S3) {
            return;
        } else {
            MYASSERT(0);
        }
    }

    void parsePathsWrapper(unique_ptr<ServerSecretShares>& path_shares,
                           unique_ptr<ServerSecretShares>& parsed_path_shares) {
        if ((server_id == SERVER_ID::S1) || (server_id == SERVER_ID::S2)) {
            path_shares->extractPathTuples(parsed_path_shares);
            return;
        } else if (server_id == SERVER_ID::S3) {
            return;
        } else {
            MYASSERT(0);
        }
    }

    // filter out ss in
    void moveFilteredPathShares(unique_ptr<ServerSecretShares>& ss,
                                unique_ptr<ServerSecretShares>& cycle_shares,
                                vector<bool>& is_valid,
                                vector<bool>& is_cycle) {
        uint64_t tuple_num = ss->size();
        MYASSERT(is_valid.size() == tuple_num);
        MYASSERT(is_cycle.size() == tuple_num);
        unique_ptr<ServerSecretShares> filtered_ss =
            make_unique<ServerSecretShares>();
        MYASSERT(cycle_shares->size() == 0);
        for (uint64_t i = 0; i < tuple_num; i++) {
            if (is_valid[i] && is_cycle[i]) {
                cycle_shares->insert(ss->svec[i]);
            } else if (is_valid[i] && !is_cycle[i]) {
                filtered_ss->insert(ss->svec[i]);
            }
        }
        ss = move(filtered_ss);
    }

    void filterWrapper(unique_ptr<ServerSecretShares>& ss,
                       unique_ptr<ServerSecretShares>& cycle_shares) {
        vector<bool> is_valid, is_cycle;
        if (server_id == SERVER_ID::S1) {
            this->oblivousFilter(ss, ip_no_port_1, ALICE, is_valid, is_cycle);
            moveFilteredPathShares(ss, cycle_shares, is_valid, is_cycle);
            return;
        } else if (server_id == SERVER_ID::S2) {
            this->oblivousFilter(ss, ip_no_port_1, BOB, is_valid, is_cycle);
            moveFilteredPathShares(ss, cycle_shares, is_valid, is_cycle);
            return;
        } else if (server_id == SERVER_ID::S3) {
            return;
        } else {
            MYASSERT(0);
        }
    }

    // merge edge_shares into path_shares
    void padPathsAndEdges(unique_ptr<ServerSecretShares>& path_shares,
                          unique_ptr<ServerSecretShares>& edge_shares) {
        uint64_t total_tuple_num = path_shares->size() + edge_shares->size();
        for (uint64_t i = 0; i < path_shares->size(); i++) {
            path_shares->svec[i]->padPathTuple(this->max_degree,
                                               this->detect_length_k);
        }
        for (uint64_t i = 0; i < edge_shares->size(); i++) {
            edge_shares->svec[i]->padEdgeTuple(this->max_degree,
                                               this->detect_length_k);
            path_shares->insert(edge_shares->svec[i]);
        }
        MYASSERT(total_tuple_num == path_shares->size());
    }

    void reassignSecretShares(unique_ptr<ServerSecretShares>& ss) {
        if (server_id == SERVER_ID::S1) {
            ServerSecretShares* s2 = getSecondServerSecretShares();
            // s2 is cleared inside the func
            // cout << "start create random ness with s2\n";
            compute_server->createRandomnessToDstWithSize(
                s2,
                this->reassign_randomness_seed + this->detect_length_k +
                    this->max_degree,
                ss->size());
            // cout << "finish create random ness with s2\n";
            ServerSecretShares* s1 = getFirstServerSecretShares();
            // do not clear s1, xorSecretShareTuple requires the size to be
            // pre-set.
            vector<ServerSecretShares*> xor_vec = {ss.get(), s2};
            s1->clear();
            // cout << "start create random ness with s1\n";

            compute_server->xorSecretShareTupleWithEmptyDst(s1, xor_vec);
            // cout << "finish create random ness with s1\n";

            thread sendS1(&NetServer::sendServerSecretShareWorker, this, s1,
                          fd_3);
            // cout << "finish send1s1 thread\n";
            thread sendS2(&NetServer::sendServerSecretShareWorker, this, s2,
                          fd_2);
            // cout << "finish send1s2 thread\n";
            sendS1.join();
            // cout << "finish sends1 thread\n";
            sendS2.join();
            // cout << "finish sends2 thread\n";
            return;
        } else if (server_id == SERVER_ID::S2) {
            ServerSecretShares* s3 = getSecondServerSecretShares();
            // clear s3 and move ss into s3
            s3->moveServerShares(ss.get());
            thread sendS3(&NetServer::sendServerSecretShareWorker, this, s3,
                          fd_3);
            ServerSecretShares* s2 = getFirstServerSecretShares();
            s2->clear();
            thread recvS2(&NetServer::recvServerSecretShareToPtrWorker, this,
                          s2, fd_1, this->max_degree, this->detect_length_k);
            sendS3.join();
            recvS2.join();
            return;
        } else if (server_id == SERVER_ID::S3) {
            ServerSecretShares* s3 = getFirstServerSecretShares();
            ServerSecretShares* s1 = getSecondServerSecretShares();
            s3->clear();
            s1->clear();
            thread recvS1(&NetServer::recvServerSecretShareToPtrWorker, this,
                          s1, fd_1, this->max_degree, this->detect_length_k);
            thread recvS3(&NetServer::recvServerSecretShareToPtrWorker, this,
                          s3, fd_2, this->max_degree, this->detect_length_k);
            recvS1.join();
            recvS3.join();
            return;
        } else {
            MYASSERT(0);
        }
    }

    void run(string share_file1, string share_file2, bool debugging) {
        // load secret shares from file
        loadSecretShare(share_file1, share_file2);

        // establish connection with other servers
        connectWithOtherServers();
        cout << "finish connecting\n";
        validateFds();
        system_clock::time_point ts, te;

        /*in a for loop*/
        while (this->detect_length_k <= this->total_detect_length) {
            cout << "Round " << this->detect_length_k << " has "
                 << this->getFirstServerSecretShares()->size() << " tuples"
                 << endl;
            auto start = system_clock::now();
            if (debugging) {
                cout << "recon before shuffle\n";
                reconCurrentSS(this->max_degree, this->detect_length_k);
                cout << "finish recon\n";
            }

            ts = system_clock::now();
            // run oblivious sort wrapper: (1) shuffle
            shuffleWrapper(this->max_degree, this->detect_length_k);
            cout << "finish shuffle for detect_length_k " << detect_length_k
                 << endl;
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " shuffle before with sort takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "recon after shuffle\n";
                reconCurrentSS(this->max_degree, this->detect_length_k);
                cout << "finish recon\n";
            }

            ts = system_clock::now();

            // combine the local share to get compute share, S2' second share
            // becomes all invalid ptr
            unique_ptr<ServerSecretShares> local_compute_ss =
                computeTwoPCShare();

            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " combine shares takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            ts = system_clock::now();
            // run oblivious sort wrapper: (2) sort
            sortWrapper(local_compute_ss);
            cout << "sort finished\n";
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k << " sort takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "recon after sorting\n";
                reconWithTwoPCShare(local_compute_ss);
                cout << "finish recon after sorting\n";
            }

            ts = system_clock::now();
            // neighbor passing and extension
            neighborPassWrapper(local_compute_ss);
            cout << "neighbor pass finished\n";
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " neighbor passing takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "recon after neighbor pass\n";
                reconWithTwoPCShare(local_compute_ss);
                cout << "finish recon after neighbor pass\n";
            }

            ts = system_clock::now();
            // reassign first_s and second_s in all servers
            reassignSecretShares(local_compute_ss);
            cout << "finish reassign shares\n";
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " reassign shares after neighbor passing takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "recon after reassign\n";
                reconCurrentSS(this->max_degree, this->detect_length_k);
                cout << "finish recon after reassign\n";
            }

            ts = system_clock::now();
            // shuffle
            shuffleWrapper(this->max_degree, this->detect_length_k);
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " second shuffle passing takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            ts = system_clock::now();

            local_compute_ss->clear();
            local_compute_ss = computeTwoPCShare();

            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " combine local share after shuffle takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "before extract\n";
                reconWithTwoPCShare(local_compute_ss);
                cout << "finish before extract\n";
            }

            ts = system_clock::now();
            // extract paths
            unique_ptr<ServerSecretShares> edge_shares =
                make_unique<ServerSecretShares>();
            unique_ptr<ServerSecretShares> path_shares =
                make_unique<ServerSecretShares>();
            extractWrapper(local_compute_ss, edge_shares, path_shares);

            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " extract paths takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "edges after extract\n";
                reconWithTwoPCShare(edge_shares);
                cout << "finish recon edges after extract\n";

                cout << "paths after extract\n";
                reconWithTwoPCShare(path_shares);
                cout << "finish recon paths after extract\n";
            }

            // update the max degree to 1 temporily in both netserver and
            // compute_server objects
            this->setMaxDegreeToOne();

            ts = system_clock::now();

            // parse path tuples into multiple shares
            unique_ptr<ServerSecretShares> parsed_path_shares =
                make_unique<ServerSecretShares>();
            parsePathsWrapper(path_shares, parsed_path_shares);

            uint64_t total_num_of_parsed_tuples = parsed_path_shares->size();
            cout << "[INFO] finish parse paths, has "
                 << total_num_of_parsed_tuples << " tuples" << endl;

            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " parse paths takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            ts = system_clock::now();

            reassignSecretShares(parsed_path_shares);
            cout << "finish reassign parsed path shares\n";

            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " reassign shares takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            ts = system_clock::now();
            // shuffle
            shuffleWrapper(this->max_degree, this->detect_length_k);

            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k << " shuffle takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "recon after shuffle\n";
                reconCurrentSS(this->max_degree, this->detect_length_k);
                cout << "finish recon after shuffle\n";
            }

            ts = system_clock::now();

            local_compute_ss->clear();
            local_compute_ss = computeTwoPCShare();

            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k
                << " compute local shares takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "2pc recon after shuffle\n";
                reconWithTwoPCShare(local_compute_ss);
                cout << "finish 2pc recon after shuffle\n";
            }

            ts = system_clock::now();
            // filter, cycle detection
            unique_ptr<ServerSecretShares> cycle_shares =
                make_unique<ServerSecretShares>();
            filterWrapper(local_compute_ss, cycle_shares);
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k << " filter takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            uint64_t num_of_valid_paths = local_compute_ss->size();
            cout << "[INFO] valid paths: " << num_of_valid_paths
                 << ", and invalid paths: "
                 << total_num_of_parsed_tuples - num_of_valid_paths << endl;

            // the local_compute_ss is empty (for server1 and server2), could
            // stop without proceeding
            // Note: server 3 needs to be manually killed for now
            if (local_compute_ss->size() == 0 && server_id != SERVER_ID::S3) {
                cout << "no more exploration paths\n";
                cout << "detect length " << this->detect_length_k << " has "
                     << cycle_shares->size() << " cycles" << endl;
                // recon cycles
                if (debugging) {
                    if (cycle_shares->size() != 0) {
                        cout << "2pc recon cycles\n";
                        reconWithTwoPCShare(cycle_shares);
                        cout << "finish 2pc recon cycles\n";
                    }
                }
                break;
            }

            if (debugging) {
                cout << "2pc recon after filter\n";
                reconWithTwoPCShare(local_compute_ss);
                cout << "finish 2pc recon after filter\n";

                // recon cycles
                if (cycle_shares->size() != 0) {
                    cout << "2pc recon cycles\n";
                    reconWithTwoPCShare(cycle_shares);
                    cout << "finish 2pc recon cycles\n";
                }
            }

            cout << "detect length " << this->detect_length_k << " has "
                 << cycle_shares->size() << " cycles" << endl;

            // increment detect_length_k by 1
            this->incDetectLength();

            // reset max_degree back
            this->resetMaxDegree();

            ts = system_clock::now();
            // pad tuples to next round, merge padded edge shares into
            // local_compute_ss
            padPathsAndEdges(local_compute_ss, edge_shares);
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k - 1
                << " padding tuples takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            cout << "there are " << local_compute_ss->size()
                 << " tuples for next round" << endl;

            if (debugging) {
                cout << "2pc recon after padding\n";
                reconWithTwoPCShare(local_compute_ss);
                cout << "finish 2pc recon after padding\n";
            }

            ts = system_clock::now();
            // reassign secret shares
            reassignSecretShares(local_compute_ss);
            // reassign id
            compute_server->assignTieBreakerIds();
            te = system_clock::now();
            cout
                << "[TIME] round " << this->detect_length_k - 1
                << " reassign for next round takes "
                << duration_cast<std::chrono::duration<double>>(te - ts).count()
                << " seconds" << endl;

            if (debugging) {
                cout << "recon after reassign\n";
                reconCurrentSS(this->max_degree, this->detect_length_k);
                cout << "finish recon after reassign\n";
            }
            auto end = system_clock::now();
            cout << "[TIME] round " << this->detect_length_k - 1 << " takes "
                 << duration_cast<std::chrono::duration<double>>(end - start)
                        .count()
                 << " seconds" << endl;
        }
        cout << "finish run\n";
        closeConnectionWithOtherServers();
    }
};

#endif  // COMPUTE_SERVER_HPP