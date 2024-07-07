#ifndef TWO_PC_COMPUTE_HPP
#define TWO_PC_COMPUTE_HPP

#include "common/common.h"
#include "emp-sh2pc/emp-sh2pc.h"
#include "emp-tool/emp-tool.h"
using namespace emp;

// Functions for computing 2pc tasks
class TwoPcEngine {
   private:
    int party;
    string ip;
    int port;
    int emp_bit_len;
    // Only used in `constructInyFromLocalShares` for now
    int bit_len;

    int tie_breaker_bit;
    NetIO* nio;
    // // for debugging output log purpose
    // ofstream out;

    // NOTE: remember to use `bit_len` instead of `emp_bit_len`
    uint32_t constructIntFromLocalShares(const Integer& input, int input_len) {
        uint32_t res = 0;
        for (int i = 0; i < input_len; i++) {
            // cout << "[DEBUG] round " << i
            //      << ", the bit is: " << getLSB(input.bits[i].bit) << endl;
            res |= (getLSB(input.bits[i].bit) << i);
            // cout << "[DEBUG] round " << i << ", res: " << res << endl;
        }
        return res;
    }

    void findNeighborsOfTheEnd(const vector<uint32_t>& secret_shares,
                               int max_degree,
                               vector<uint32_t>& end_neighbors_shares,
                               bool& if_encounter_edge,
                               uint64_t curr_begin_index,
                               uint64_t processed_tuple_num) {
        MYASSERT(secret_shares.size() % (max_degree + 1) == 0);
        MYASSERT(end_neighbors_shares.size() ==
                 static_cast<uint64_t>(max_degree));
        // the first element is `node_id` and the remaining `max_degree`
        // elements are neighbor nodes for edge tuple or all zeros for path
        // tuple.
        uint64_t num_elements = secret_shares.size() / (max_degree + 1);
        MYASSERT(num_elements >= curr_begin_index + processed_tuple_num);

        uint64_t ss_vec_size = processed_tuple_num * (max_degree + 1);
        uint64_t ss_vec_begin_index = curr_begin_index * (max_degree + 1);

        Integer* s1 = new Integer[ss_vec_size];
        Integer* s2 = new Integer[ss_vec_size];

        for (uint64_t i = 0; i < ss_vec_size; i++) {
            s1[i] = Integer(emp_bit_len, secret_shares[i + ss_vec_begin_index],
                            ALICE);
        }

        for (uint64_t i = 0; i < ss_vec_size; i++) {
            s2[i] = Integer(emp_bit_len, secret_shares[i + ss_vec_begin_index],
                            BOB);
        }

        Integer* neighbors_s1 = new Integer[max_degree];
        Integer* neighbors_s2 = new Integer[max_degree];

        Integer* neighbors = new Integer[max_degree];

        for (int i = 0; i < max_degree; i++) {
            // assign neighbors initially to the start_neighbors
            neighbors_s1[i] =
                Integer(emp_bit_len, end_neighbors_shares[i], ALICE);
        }

        for (int i = 0; i < max_degree; i++) {
            // assign neighbors initially to the start_neighbors
            neighbors_s2[i] =
                Integer(emp_bit_len, end_neighbors_shares[i], BOB);
        }

        for (int i = 0; i < max_degree; i++) {
            // assign neighbors initially to values from the last batch
            neighbors[i] = (neighbors_s1[i] ^ neighbors_s2[i]);
        }

        Integer* reconstructed_neighbors =
            new Integer[processed_tuple_num * max_degree];

        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            for (int j = 0; j < max_degree; j++) {
                // skip the first element which is the node_id
                reconstructed_neighbors[i * max_degree + j] =
                    s1[i * (max_degree + 1) + 1 + j] ^
                    s2[i * (max_degree + 1) + 1 + j];
            }
        }

        Integer* node_ids = new Integer[processed_tuple_num];
        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            uint64_t node_id_index = i * (max_degree + 1);
            node_ids[i] = (s1[node_id_index] ^ s2[node_id_index]);
        }

        Integer zero = Integer(emp_bit_len, 0, PUBLIC);

        // init from the last batch
        Bit if_encountered_edge_tuple_so_far_s1(if_encounter_edge, ALICE);
        Bit if_encountered_edge_tuple_so_far_s2(if_encounter_edge, BOB);

        Bit if_encountered_edge_tuple_so_far =
            if_encountered_edge_tuple_so_far_s1 ^
            if_encountered_edge_tuple_so_far_s2;

        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            Bit current_node_id_is_zero = node_ids[i].equal(zero);
            if (i == 0 && curr_begin_index == 0) {
                if_encountered_edge_tuple_so_far = !current_node_id_is_zero;
            } else {
                if_encountered_edge_tuple_so_far =
                    if_encountered_edge_tuple_so_far |
                    (!current_node_id_is_zero);
            }
            uint64_t start_index = i * max_degree;
            for (int j = 0; j < max_degree; j++) {
                // x.select(b, y) -> if b: y else: x
                // if current_node_id_is_zero is true, then this is a path tuple
                // neighbors are unchanged.
                // otherwise, it's an edge tuple, update neighbors to
                // reconstructed_neighbors of the current position
                neighbors[j] = reconstructed_neighbors[start_index + j].select(
                    current_node_id_is_zero, neighbors[j]);
            }
        }

        for (int i = 0; i < max_degree; i++) {
            uint32_t recon_local_share =
                constructIntFromLocalShares(neighbors[i], bit_len);
            end_neighbors_shares[i] = (recon_local_share);
        }

        // access local share of whether during the process there is any edge
        // tuple encountered.
        if_encounter_edge = getLSB(if_encountered_edge_tuple_so_far.bit);

        delete[] s1;
        delete[] s2;
        delete[] neighbors_s1;
        delete[] neighbors_s2;
        delete[] neighbors;
        delete[] reconstructed_neighbors;
        delete[] node_ids;
    }

    // secret share stores all the secret shares of the last column
    // secret_share.size() should equal (max_degree * # of total elements)
    void neighborPass(const vector<uint32_t>& secret_shares,
                      vector<uint32_t>& start_neighbors_shares, int max_degree,
                      uint64_t begin_tuple_index, uint64_t processed_tuple_num,
                      vector<uint32_t>& updated_neighbors_shares) {
        MYASSERT(secret_shares.size() % (max_degree + 1) == 0);
        MYASSERT(start_neighbors_shares.size() ==
                 static_cast<uint64_t>(max_degree));
        // the first element is `node_id` and the remaining `max_degree`
        // elements are neighbor nodes for edge tuple or all zeros for path
        // tuple.
        uint64_t num_elements = secret_shares.size() / (max_degree + 1);
        MYASSERT(num_elements >= (processed_tuple_num + begin_tuple_index));

        uint64_t ss_vec_size = processed_tuple_num * (max_degree + 1);
        uint64_t ss_vec_begin_index = begin_tuple_index * (max_degree + 1);

        Integer* s1 = new Integer[ss_vec_size];
        Integer* s2 = new Integer[ss_vec_size];

        for (uint64_t i = 0; i < ss_vec_size; i++) {
            s1[i] = Integer(emp_bit_len, secret_shares[i + ss_vec_begin_index],
                            ALICE);
        }

        for (uint64_t i = 0; i < ss_vec_size; i++) {
            s2[i] = Integer(emp_bit_len, secret_shares[i + ss_vec_begin_index],
                            BOB);
        }

        Integer* start_neighbors1 = new Integer[max_degree];
        Integer* start_neighbors2 = new Integer[max_degree];

        for (int i = 0; i < max_degree; i++) {
            start_neighbors1[i] =
                Integer(emp_bit_len, start_neighbors_shares[i], ALICE);
        }

        for (int i = 0; i < max_degree; i++) {
            start_neighbors2[i] =
                Integer(emp_bit_len, start_neighbors_shares[i], BOB);
        }

        Integer* neighbors = new Integer[max_degree];
        for (int i = 0; i < max_degree; i++) {
            // assign neighbors initially to the start_neighbors
            neighbors[i] = (start_neighbors1[i] ^ start_neighbors2[i]);
        }

        Integer* reconstructed_neighbors =
            new Integer[processed_tuple_num * max_degree];

        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            for (int j = 0; j < max_degree; j++) {
                // skip the first element which is the node_id
                reconstructed_neighbors[i * max_degree + j] =
                    s1[i * (max_degree + 1) + 1 + j] ^
                    s2[i * (max_degree + 1) + 1 + j];
            }
        }

        Integer* node_ids = new Integer[processed_tuple_num];
        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            uint64_t node_id_index = i * (max_degree + 1);
            node_ids[i] = (s1[node_id_index] ^ s2[node_id_index]);
        }

        Integer zero = Integer(emp_bit_len, 0, PUBLIC);
        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            Bit current_node_id_is_zero = node_ids[i].equal(zero);
            uint64_t start_index = i * max_degree;
            for (int j = 0; j < max_degree; j++) {
                // x.select(b, y) -> if b: y else: x
                // if current_node_id_is_zero is true, then this is a path tuple
                // pass neighbors
                // otherwise, do not change update the neighbors.
                reconstructed_neighbors[start_index + j] =
                    reconstructed_neighbors[start_index + j].select(
                        current_node_id_is_zero, neighbors[j]);
                // x.select(b, y) -> if b: y else: x
                // if current_node_id_is_zero is true, then this is a path tuple
                // neighbors are unchanged.
                // otherwise, it's an edge tuple, update neighbors to
                // reconstructed_neighbors of the current position
                neighbors[j] = reconstructed_neighbors[start_index + j].select(
                    current_node_id_is_zero, neighbors[j]);
            }
        }

        // write these updated neighbor shares from reconstructed_neighbors's
        // local shares.
        for (uint64_t i = 0; i < max_degree * processed_tuple_num; i++) {
            uint32_t recon_local_share = constructIntFromLocalShares(
                reconstructed_neighbors[i], bit_len);
            updated_neighbors_shares.push_back(recon_local_share);
        }

        for (int i = 0; i < max_degree; i++) {
            uint32_t recon_neighbor_val =
                constructIntFromLocalShares(neighbors[i], bit_len);
            start_neighbors_shares[i] = recon_neighbor_val;
        }

        delete[] s1;
        delete[] s2;
        delete[] start_neighbors1;
        delete[] start_neighbors2;
        delete[] neighbors;
        delete[] reconstructed_neighbors;
        delete[] node_ids;
    }

    // `previsous_thread_neighbor_shares` contains `thread_id+1` tuples (given
    // thread_id starts from 0). Each tuple has `max_degree` elements which
    // records the found tuple of all the previous threads and itself.
    // `previous_if_encountered_edge_shares` also has `thread_id+1` elements
    // similar to `previous_thread_neighbor_shares`.
    //
    // `real_edge_neighbor_shares` is the return value of local shares of real
    // neighbors the current thread.
    void findNeighborsOfPreviousEdgeTuple(
        int thread_id, int max_degree,
        vector<uint32_t>& previous_thread_neighbor_shares,
        vector<bool>& previous_if_encountered_edge_shares,
        vector<uint32_t>& real_edge_neighbor_shares) {
        if (thread_id <= 1) {
            return;
        }
        MYASSERT(previous_thread_neighbor_shares.size() ==
                 (uint64_t)(max_degree * (thread_id)));
        MYASSERT(previous_if_encountered_edge_shares.size() ==
                 (uint64_t)(thread_id));
        MYASSERT(real_edge_neighbor_shares.empty());

        Integer* s1 = new Integer[previous_thread_neighbor_shares.size()];
        Integer* s2 = new Integer[previous_thread_neighbor_shares.size()];

        for (uint64_t i = 0; i < previous_thread_neighbor_shares.size(); i++) {
            s1[i] =
                Integer(emp_bit_len, previous_thread_neighbor_shares[i], ALICE);
        }

        for (uint64_t i = 0; i < previous_thread_neighbor_shares.size(); i++) {
            s2[i] =
                Integer(emp_bit_len, previous_thread_neighbor_shares[i], BOB);
        }

        Integer* previous_neighbors =
            new Integer[previous_thread_neighbor_shares.size()];
        for (uint64_t i = 0; i < previous_thread_neighbor_shares.size(); i++) {
            previous_neighbors[i] = (s1[i] ^ s2[i]);
        }

        Bit* b1 = new Bit[previous_if_encountered_edge_shares.size()];
        Bit* b2 = new Bit[previous_if_encountered_edge_shares.size()];

        for (uint64_t i = 0; i < previous_if_encountered_edge_shares.size();
             i++) {
            b1[i] = Bit(previous_if_encountered_edge_shares[i], ALICE);
        }
        for (uint64_t i = 0; i < previous_if_encountered_edge_shares.size();
             i++) {
            b2[i] = Bit(previous_if_encountered_edge_shares[i], BOB);
        }
        Bit* if_encountered_edge =
            new Bit[previous_if_encountered_edge_shares.size()];
        for (uint64_t i = 0; i < previous_if_encountered_edge_shares.size();
             i++) {
            if_encountered_edge[i] = b1[i] ^ b2[i];
        }

        Integer* neighbors = new Integer[max_degree];
        for (int i = 0; i < max_degree; i++) {
            // assign neighbors initially to the the neighbors of the original
            neighbors[i] =
                (previous_neighbors[(thread_id - 1) * max_degree + i]);
        }
        // Iterate through the back of all neighbors to the beginning to find
        // real neighbors
        Bit edge_encountered_so_far = if_encountered_edge[(thread_id - 1)];
        for (int i = thread_id - 2; i >= 0; i--) {
            uint64_t elem_id = static_cast<uint64_t>(i);
            Bit if_update_neighbors =
                (!edge_encountered_so_far) & (if_encountered_edge[elem_id]);
            for (int j = 0; j < max_degree; j++) {
                // x.select(b, y) -> if b: y else: x
                neighbors[j] = neighbors[j].select(
                    if_update_neighbors,
                    previous_neighbors[elem_id * max_degree + j]);
            }
            edge_encountered_so_far =
                edge_encountered_so_far | if_encountered_edge[elem_id];
        }
        for (int i = 0; i < max_degree; i++) {
            uint32_t recon_local_share =
                constructIntFromLocalShares(neighbors[i], bit_len);
            real_edge_neighbor_shares.push_back(recon_local_share);
        }

        delete[] s1;
        delete[] s2;
        delete[] previous_neighbors;
        delete[] b1;
        delete[] b2;
        delete[] if_encountered_edge;
        delete[] neighbors;
    }

   public:
    TwoPcEngine(int party_, string ip_, int port_, int bit_len_,
                int tie_breaker_bit_)
        // emp_bit_len = bit_len_ + 1 with one more bit for sign
        // TODO: check whether we need one more bit for overflow if we have
        // additions, etc.
        : party(party_),
          ip(ip_),
          port(port_),
          emp_bit_len(bit_len_ + 1),
          bit_len(bit_len_),
          tie_breaker_bit(tie_breaker_bit_) {
        // // For debugging
        // stringstream ss;
        // ss << party << "." << port << ".txt";
        // out = ofstream(ss.str());
    }

    // NOTE: secret shares must be sorted (i.e., results after oblivious sort)
    void obliviousNeighborPass(const vector<uint32_t>& secret_shares,
                               vector<uint32_t>& start_neighbors_shares,
                               int max_degree, uint64_t begin_tuple_index,
                               uint64_t processed_tuple_num,
                               vector<uint32_t>& updated_neighbors_shares) {
        neighborPass(secret_shares, start_neighbors_shares, max_degree,
                     begin_tuple_index, processed_tuple_num,
                     updated_neighbors_shares);
    }

    void obliviousFindNeighbors(const vector<uint32_t>& secret_shares,
                                int max_degree,
                                vector<uint32_t>& updated_local_shares,
                                bool& if_encounter_edge,
                                uint64_t curr_begin_index,
                                uint64_t processed_tuple_num) {
        findNeighborsOfTheEnd(secret_shares, max_degree, updated_local_shares,
                              if_encounter_edge, curr_begin_index,
                              processed_tuple_num);
    }

    void obliviousFindNeighborsOfEdgeTuple(
        int thread_id, int max_degree,
        vector<uint32_t>& previous_thread_neighbor_shares,
        vector<bool>& previous_if_encountered_edge_shares,
        vector<uint32_t>& real_edge_neighbor_shares) {
        findNeighborsOfPreviousEdgeTuple(
            thread_id, max_degree, previous_thread_neighbor_shares,
            previous_if_encountered_edge_shares, real_edge_neighbor_shares);
    }

    void initEngine() {
        nio = new NetIO(party == ALICE ? nullptr : ip.c_str(), port);
        setup_semi_honest(nio, party);
    }

    void terminateEngine() {
        finalize_semi_honest();
        delete nio;
    }

    // There are two inputs, node_val, end_node
    // to compare two inputs, the first is larger if
    // node_val_larger_than_pivot | (!node_val_larger_than_pivot &
    // end_node_larger_than_pivot);
    //
    // smaller_index and larger_index
    void obliviousPartition(
        vector<uint32_t>& node_val_shares, vector<uint32_t>& end_node_shares,
        vector<uint64_t>& tie_breaker_shares, uint32_t pivot_val_share,
        uint32_t pivot_end_node_share, uint64_t pivot_tie_breaker_share,
        vector<uint64_t>& smaller_index, vector<uint64_t>& larger_index,
        uint64_t begin_index, uint64_t curr_vec_index, uint64_t process_num) {
        MYASSERT(node_val_shares.size() == end_node_shares.size());
        MYASSERT(tie_breaker_shares.size() == node_val_shares.size());
        MYASSERT(tie_breaker_shares.size() >= process_num + curr_vec_index);
        // MYASSERT(smaller_index.empty());
        // MYASSERT(larger_index.empty());
        Integer ps1(emp_bit_len, pivot_val_share, ALICE);
        Integer ps2(emp_bit_len, pivot_val_share, BOB);
        Integer pivot_node_val = ps1 ^ ps2;

        Integer pes1(emp_bit_len, pivot_end_node_share, ALICE);
        Integer pes2(emp_bit_len, pivot_end_node_share, BOB);
        Integer pivot_end_node = pes1 ^ pes2;

        Integer tbs1(this->tie_breaker_bit, pivot_tie_breaker_share, ALICE);
        Integer tbs2(this->tie_breaker_bit, pivot_tie_breaker_share, BOB);
        Integer pivot_tie_breaker = tbs1 ^ tbs2;

        Integer* node_s1 = new Integer[process_num];
        Integer* node_s2 = new Integer[process_num];

        for (uint64_t i = 0; i < process_num; i++) {
            node_s1[i] = Integer(emp_bit_len,
                                 node_val_shares[i + curr_vec_index], ALICE);
        }

        for (uint64_t i = 0; i < process_num; i++) {
            node_s2[i] =
                Integer(emp_bit_len, node_val_shares[i + curr_vec_index], BOB);
        }

        Integer* node_vals = new Integer[process_num];
        for (uint64_t i = 0; i < process_num; i++) {
            node_vals[i] = node_s1[i] ^ node_s2[i];
        }

        Integer* end_s1 = new Integer[process_num];
        Integer* end_s2 = new Integer[process_num];

        for (uint64_t i = 0; i < process_num; i++) {
            end_s1[i] = Integer(emp_bit_len,
                                end_node_shares[i + curr_vec_index], ALICE);
        }

        for (uint64_t i = 0; i < process_num; i++) {
            end_s2[i] =
                Integer(emp_bit_len, end_node_shares[i + curr_vec_index], BOB);
        }

        Integer* end_nodes = new Integer[process_num];
        for (uint64_t i = 0; i < process_num; i++) {
            end_nodes[i] = end_s1[i] ^ end_s2[i];
        }

        Integer* tie_breaker_s1 = new Integer[process_num];
        Integer* tie_breaker_s2 = new Integer[process_num];
        for (uint64_t i = 0; i < process_num; i++) {
            tie_breaker_s1[i] =
                Integer(this->tie_breaker_bit,
                        tie_breaker_shares[i + curr_vec_index], ALICE);
        }

        for (uint64_t i = 0; i < process_num; i++) {
            tie_breaker_s2[i] =
                Integer(this->tie_breaker_bit,
                        tie_breaker_shares[i + curr_vec_index], BOB);
        }

        Integer* tie_breakers = new Integer[process_num];
        for (uint64_t i = 0; i < process_num; i++) {
            tie_breakers[i] = tie_breaker_s1[i] ^ tie_breaker_s2[i];
        }

        string larger_than_shares(process_num, 0);
        string larger_than_another_shares(process_num, 0);

        for (uint64_t i = 0; i < process_num; i++) {
            Bit node_val_larger_than_pivot = (node_vals[i] > pivot_node_val);
            Bit node_val_equal = (node_vals[i] == pivot_node_val);
            Bit end_node_larger_than_pivot = (end_nodes[i] > pivot_end_node);
            Bit end_node_equal_pivot = (end_nodes[i] == pivot_end_node);
            Bit tie_breakers_larger_than_pivot =
                (tie_breakers[i] > pivot_tie_breaker);

            // The logic to check if the first tuple is larger
            Bit larger_than_pivot =
                node_val_larger_than_pivot |
                (node_val_equal & end_node_larger_than_pivot) |
                (node_val_equal & end_node_equal_pivot &
                 tie_breakers_larger_than_pivot);
            larger_than_shares[i] = getLSB(larger_than_pivot.bit) + '0';
            // // For debugging output only
            // bool b1 = node_val_larger_than_pivot.reveal();
            // bool b2 = end_node_larger_than_pivot.reveal();
            // uint32_t v1 = node_vals[i].reveal<uint32_t>();
            // uint32_t v2 = pivot_node_val.reveal<uint32_t>();
            // uint32_t v3 = end_nodes[i].reveal<uint32_t>();
            // uint32_t v4 = pivot_end_node.reveal<uint32_t>();
            // uint32_t v5 = node_s1[i].reveal<uint32_t>();
            // uint32_t v6 = node_s2[i].reveal<uint32_t>();
            // if (party == ALICE) {
            //     cout << i << " bool " << b1 << " " << b2 << " " <<
            //     larger_plain
            //          << endl;
            //     cout << i << " node " << v1 << " " << v2 << endl;
            //     cout << i << " end " << v3 << " " << v4 << endl;
            //     cout << "[local share val] party " << party << " " << v5 << "
            //     "
            //          << v6 << endl;
            // }
        }

        nio->flush();
        // exchange larger_than_shares
        if (party == ALICE) {
            this->nio->send_data_internal(larger_than_shares.c_str(),
                                          process_num);
            this->nio->recv_data_internal(
                (char*)larger_than_another_shares.c_str(), process_num);
        } else {
            this->nio->recv_data_internal(
                (char*)larger_than_another_shares.c_str(), process_num);
            this->nio->send_data_internal(larger_than_shares.c_str(),
                                          process_num);
        }
        nio->flush();

        for (uint64_t i = 0; i < process_num; i++) {
            bool larger_s1 = larger_than_shares[i] - '0';
            bool larger_s2 = larger_than_another_shares[i] - '0';
            bool larger_plain = larger_s1 ^ larger_s2;
            if (larger_plain) {
                larger_index.push_back(i + curr_vec_index + begin_index);
            } else {
                smaller_index.push_back(i + curr_vec_index + begin_index);
            }
        }

        // uint32_t pivot_node = pivot_node_val.reveal<uint32_t>();
        // uint32_t pivot_end = pivot_end_node.reveal<uint32_t>();
        // if (party == ALICE) {
        //     cout << "pivot node: " << pivot_node << " pivot end " <<
        //     pivot_end
        //          << endl;
        // }

        delete[] node_s1;
        delete[] node_s2;
        delete[] node_vals;
        delete[] end_s1;
        delete[] end_s2;
        delete[] end_nodes;
        delete[] tie_breaker_s1;
        delete[] tie_breaker_s2;
        delete[] tie_breakers;
    }

    // directly sort elements and return the sorted index
    void sortElemsDirectly(vector<uint32_t>& node_val_shares,
                           vector<uint32_t>& end_node_shares,
                           vector<uint64_t>& tie_breaker_shares,
                           uint64_t begin_index,
                           vector<uint64_t>& sorted_index) {
        MYASSERT(sorted_index.empty());
        MYASSERT(node_val_shares.size() == end_node_shares.size());
        MYASSERT(node_val_shares.size() == tie_breaker_shares.size());

        Integer* node_s1 = new Integer[node_val_shares.size()];
        Integer* node_s2 = new Integer[node_val_shares.size()];

        for (uint64_t i = 0; i < node_val_shares.size(); i++) {
            node_s1[i] = Integer(emp_bit_len, node_val_shares[i], ALICE);
        }

        for (uint64_t i = 0; i < node_val_shares.size(); i++) {
            node_s2[i] = Integer(emp_bit_len, node_val_shares[i], BOB);
        }

        Integer* node_vals = new Integer[node_val_shares.size()];
        for (uint64_t i = 0; i < node_val_shares.size(); i++) {
            node_vals[i] = node_s1[i] ^ node_s2[i];
        }

        Integer* end_s1 = new Integer[end_node_shares.size()];
        Integer* end_s2 = new Integer[end_node_shares.size()];

        for (uint64_t i = 0; i < end_node_shares.size(); i++) {
            end_s1[i] = Integer(emp_bit_len, end_node_shares[i], ALICE);
        }

        for (uint64_t i = 0; i < end_node_shares.size(); i++) {
            end_s2[i] = Integer(emp_bit_len, end_node_shares[i], BOB);
        }

        Integer* end_nodes = new Integer[end_node_shares.size()];
        for (uint64_t i = 0; i < end_node_shares.size(); i++) {
            end_nodes[i] = end_s1[i] ^ end_s2[i];
        }

        Integer* tie_breaker_s1 = new Integer[tie_breaker_shares.size()];
        Integer* tie_breaker_s2 = new Integer[tie_breaker_shares.size()];
        for (uint64_t i = 0; i < tie_breaker_shares.size(); i++) {
            tie_breaker_s1[i] =
                Integer(this->tie_breaker_bit, tie_breaker_shares[i], ALICE);
        }

        for (uint64_t i = 0; i < tie_breaker_shares.size(); i++) {
            tie_breaker_s2[i] =
                Integer(this->tie_breaker_bit, tie_breaker_shares[i], BOB);
        }

        Integer* tie_breakers = new Integer[tie_breaker_shares.size()];
        for (uint64_t i = 0; i < tie_breaker_shares.size(); i++) {
            tie_breakers[i] = tie_breaker_s1[i] ^ tie_breaker_s2[i];
        }

        uint64_t elem_num = end_node_shares.size();
        vector<uint64_t> indices;
        for (uint64_t i = 0; i < elem_num; i++) {
            indices.push_back(i);
        }

        while (sorted_index.size() < elem_num) {
            uint64_t smallest_index = indices[0];
            uint64_t smallest_id_in_indices = 0;
            for (uint64_t i = 1; i < indices.size(); i++) {
                uint64_t current_index = indices[i];
                Bit current_node_val_smaller =
                    (node_vals[current_index] < node_vals[smallest_index]);
                Bit node_val_equal =
                    (node_vals[current_index] == node_vals[smallest_index]);
                Bit current_end_node_smaller =
                    (end_nodes[current_index] < end_nodes[smallest_index]);
                Bit end_node_equal =
                    (end_nodes[current_index] == end_nodes[smallest_index]);
                Bit tie_breaker_smaller = (tie_breakers[current_index] <
                                           tie_breakers[smallest_index]);

                Bit current_is_smaller =
                    current_node_val_smaller |
                    (node_val_equal & current_end_node_smaller) |
                    (node_val_equal & end_node_equal & tie_breaker_smaller);

                bool smaller_plain = current_is_smaller.reveal();
                if (smaller_plain) {
                    smallest_index = current_index;
                    smallest_id_in_indices = i;
                }
            }
            sorted_index.push_back(smallest_index);
            indices.erase(indices.begin() + smallest_id_in_indices);
        }

        for (uint64_t i = 0; i < sorted_index.size(); i++) {
            sorted_index[i] += begin_index;
        }

        // uint32_t median_node_val =
        //     node_vals[sorted_index[sorted_index.size() /
        //     2]].reveal<uint32_t>();
        // uint32_t median_end_node =
        //     end_nodes[sorted_index[sorted_index.size() /
        //     2]].reveal<uint32_t>();

        // if (party == ALICE) {
        //     cout << "median_id: " << sorted_index[sorted_index.size() / 2]
        //          << endl;
        //     cout << "median node_val: " << median_node_val << endl;
        //     cout << "median end_val: " << median_end_node << endl;
        // }

        delete[] node_s1;
        delete[] node_s2;
        delete[] node_vals;
        delete[] end_s1;
        delete[] end_s2;
        delete[] end_nodes;
        delete[] tie_breaker_s1;
        delete[] tie_breaker_s2;
        delete[] tie_breakers;
    }

    // find the index (relative index in the passed vector) of median values in
    // the given vectors
    uint64_t findMedian(vector<uint32_t>& node_val_shares,
                        vector<uint32_t>& end_node_shares,
                        vector<uint64_t>& tie_breaker_shares) {
        vector<uint64_t> sorted_index;
        sortElemsDirectly(node_val_shares, end_node_shares, tie_breaker_shares,
                          0, sorted_index);
        return sorted_index[sorted_index.size() / 2];
    }

    // `path shares` is a vector of lists to path shares to check
    // (1) whether it's a valid path; (2) whether it's a cycle if it's a valid
    // path.
    // `path share` has `path_num*(detect_length_k+1)` elements
    // is_valid is set to true, if this is a valid path
    // only when is_valid is true, will is_cycle be set
    void obliviousFilter(vector<uint32_t>& path_shares,
                         uint64_t tuple_begin_index, uint64_t path_num,
                         int detect_length_k, vector<bool>& is_valid,
                         vector<bool>& is_cycle) {
        // cout << path_shares.size() << " "
        //      << (static_cast<uint64_t>(detect_length_k + 1) * path_num) << "
        //      "
        //      << detect_length_k << " " << path_num << endl;
        MYASSERT(path_shares.size() >=
                 (static_cast<uint64_t>(detect_length_k + 1) *
                  (tuple_begin_index + path_num)));

        MYASSERT(!is_valid.empty());
        MYASSERT(!is_cycle.empty());

        uint64_t path_share_begin_index =
            static_cast<uint64_t>(detect_length_k + 1) * (tuple_begin_index);

        uint64_t path_share_size =
            static_cast<uint64_t>(detect_length_k + 1) * (path_num);

        Integer* ps1 = new Integer[path_share_size];
        Integer* ps2 = new Integer[path_share_size];

        for (uint64_t i = 0; i < path_share_size; i++) {
            ps1[i] = Integer(emp_bit_len,
                             path_shares[i + path_share_begin_index], ALICE);
        }

        for (uint64_t i = 0; i < path_share_size; i++) {
            ps2[i] = Integer(emp_bit_len,
                             path_shares[i + path_share_begin_index], BOB);
        }

        Integer* path_nodes = new Integer[path_share_size];
        for (uint64_t i = 0; i < path_share_size; i++) {
            path_nodes[i] = ps1[i] ^ ps2[i];
        }

        Integer zero(emp_bit_len, 0, PUBLIC);

        string is_path_invalid_shares(path_num, 0);
        for (uint64_t i = 0; i < path_num; i++) {
            // check whether the last element is 0
            Bit is_path_invalid =
                (path_nodes[i * static_cast<uint64_t>(detect_length_k + 1) +
                            detect_length_k] == zero);

            // check whether the last element equals the previous ones
            for (int j = 1; j < detect_length_k; j++) {
                Bit current_node_equal_last_node =
                    (path_nodes[i * static_cast<uint64_t>(detect_length_k + 1) +
                                detect_length_k] ==
                     path_nodes[i * static_cast<uint64_t>(detect_length_k + 1) +
                                j]);
                is_path_invalid =
                    is_path_invalid | current_node_equal_last_node;
                // uint32_t current_node_val =
                //     path_nodes[i * static_cast<uint64_t>(detect_length_k + 1)
                //     +
                //                j]
                //         .reveal<uint32_t>();
                // uint32_t last_node_val =
                //     path_nodes[i * static_cast<uint64_t>(detect_length_k + 1)
                //     +
                //                detect_length_k]
                //         .reveal<uint32_t>();

                // bool cur_eq = current_node_equal_last_node.reveal();
                // if (party == ALICE) {
                //     cout << j << " " << current_node_val << " " <<
                //     last_node_val
                //          << " " << cur_eq << endl;
                // }
            }

            is_path_invalid_shares[i] = getLSB(is_path_invalid.bit) + '0';
        }
        string is_path_invalid_another_shares(path_num, 0);

        nio->flush();
        // exchange is_path_invalid_shares
        if (party == ALICE) {
            this->nio->send_data_internal(is_path_invalid_shares.c_str(),
                                          path_num);
            this->nio->recv_data_internal(
                (char*)is_path_invalid_another_shares.c_str(), path_num);
        } else {
            this->nio->recv_data_internal(
                (char*)is_path_invalid_another_shares.c_str(), path_num);
            this->nio->send_data_internal(is_path_invalid_shares.c_str(),
                                          path_num);
        }
        nio->flush();

        string is_cycle_shares(path_num, 0);
        string is_cycle_another_shares(path_num, 0);

        for (uint64_t i = 0; i < path_num; i++) {
            bool is_path_invalid_s1 = is_path_invalid_another_shares[i] - '0';
            bool is_path_invalid_s2 = is_path_invalid_shares[i] - '0';

            is_valid[i + tuple_begin_index] =
                !(is_path_invalid_s1 ^ is_path_invalid_s2);

            if (is_valid[i + tuple_begin_index]) {
                Bit first_node_equal_last_node =
                    (path_nodes[i * static_cast<uint64_t>(detect_length_k + 1) +
                                detect_length_k] ==
                     path_nodes[i *
                                static_cast<uint64_t>(detect_length_k + 1)]);
                is_cycle_shares[i] =
                    getLSB(first_node_equal_last_node.bit) + '0';
            }
        }

        nio->flush();
        // exchange is_cycle_shares
        if (party == ALICE) {
            this->nio->send_data_internal(is_cycle_shares.c_str(), path_num);
            this->nio->recv_data_internal(
                (char*)is_cycle_another_shares.c_str(), path_num);
        } else {
            this->nio->recv_data_internal(
                (char*)is_cycle_another_shares.c_str(), path_num);
            this->nio->send_data_internal(is_cycle_shares.c_str(), path_num);
        }
        nio->flush();

        for (uint64_t i = 0; i < path_num; i++) {
            if (is_valid[i + tuple_begin_index]) {
                bool is_cycle_invalid_s1 = is_cycle_another_shares[i] - '0';
                bool is_cycle_invalid_s2 = is_cycle_shares[i] - '0';
                is_cycle[i + tuple_begin_index] =
                    (is_cycle_invalid_s1 ^ is_cycle_invalid_s2);
            }
        }

        delete[] ps1;
        delete[] ps2;
        delete[] path_nodes;
    }

    void obliviousExtractPath(vector<uint32_t>& node_shares,
                              vector<uint64_t>& edge_tuple_indices,
                              uint64_t begin_index, uint64_t end_index,
                              uint64_t curr_vec_begin_index,
                              uint64_t processed_tuple_num) {
        MYASSERT(node_shares.size() == (end_index - begin_index));
        MYASSERT(node_shares.size() >=
                 (curr_vec_begin_index + processed_tuple_num));

        Integer* s1 = new Integer[processed_tuple_num];
        Integer* s2 = new Integer[processed_tuple_num];

        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            s1[i] = Integer(emp_bit_len, node_shares[i + curr_vec_begin_index],
                            ALICE);
        }

        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            s2[i] = Integer(emp_bit_len, node_shares[i + curr_vec_begin_index],
                            BOB);
        }

        string tuple_is_path_shares;
        Integer zero(emp_bit_len, 0, PUBLIC);
        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            Integer node = s1[i] ^ s2[i];
            Bit node_equal_zero = (node == zero);
            // a path tuple always have zero as node id
            bool current_tuple_is_path_share = getLSB(node_equal_zero.bit);
            string local_share = "0";
            if (current_tuple_is_path_share) {
                local_share = "1";
            }
            tuple_is_path_shares.append(local_share);
        }

        MYASSERT(tuple_is_path_shares.size() == processed_tuple_num);

        nio->flush();
        string tuple_is_path_another_shares(processed_tuple_num, 0);
        if (party == ALICE) {
            this->nio->send_data_internal(tuple_is_path_shares.c_str(),
                                          processed_tuple_num);
            this->nio->recv_data_internal(
                (char*)tuple_is_path_another_shares.c_str(),
                processed_tuple_num);
        } else {
            this->nio->recv_data_internal(
                (char*)tuple_is_path_another_shares.c_str(),
                processed_tuple_num);
            this->nio->send_data_internal(tuple_is_path_shares.c_str(),
                                          processed_tuple_num);
        }
        nio->flush();

        for (uint64_t i = 0; i < processed_tuple_num; i++) {
            bool current_tuple_is_path_s1 =
                tuple_is_path_another_shares[i] - '0';
            bool current_tuple_is_path_s2 = tuple_is_path_shares[i] - '0';
            bool current_tuple_is_path =
                current_tuple_is_path_s1 ^ current_tuple_is_path_s2;
            if (!current_tuple_is_path) {
                edge_tuple_indices.push_back(i + curr_vec_begin_index +
                                             begin_index);
            }
        }

        delete[] s1;
        delete[] s2;
    }
};

#endif  // TWO_PC_COMPUTE_HPP