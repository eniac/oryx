#ifndef SECRET_SHARE_LIB_HPP
#define SECRET_SHARE_LIB_HPP

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "common/common.h"

using namespace std;

bool is_power_of_2(uint64_t x) { return (x > 0 && !(x & (x - 1))); }

class SecretShareTuple {
   private:
    uint64_t tie_breaker_id;

   public:
    // if node_id is zero, it indicates a path tuple. Otherwise, it indicates an
    // edge tuple which includes the edges of the `node_id`.
    uint32_t node_id;
    // the vector to store secret shares of the tuples, it should have the size
    // as detect_length_k*max_degree
    vector<uint32_t> secret_share_vec;
    // the length of cycle to detect in this round
    int detect_length_k;
    // max degree of nodes
    int max_degree;

    SecretShareTuple(uint32_t node_id_, int max_degree_, int detect_length_k_)
        : node_id(node_id_),
          detect_length_k(detect_length_k_),
          max_degree(max_degree_) {
        // initialize to 0.
        tie_breaker_id = 0;
        secret_share_vec =
            vector<uint32_t>((detect_length_k + 1) * max_degree, 0);
    }

    // empty tuple
    SecretShareTuple() {}

    // Initialize to create SecretShareTuple from a serialized string
    SecretShareTuple(string serialize_str, int max_degree_,
                     int detect_length_k_)
        : detect_length_k(detect_length_k_), max_degree(max_degree_) {
        secret_share_vec =
            vector<uint32_t>((detect_length_k + 1) * max_degree, 0);
        uint64_t msg_size = serialize_str.size();
        MYASSERT(msg_size == (sizeof(uint64_t) +
                              (secret_share_vec.size() + 1) * sizeof(uint32_t) +
                              sizeof(uint64_t)));
        const char* ptr = serialize_str.c_str();
        MYASSERT(*((uint64_t*)ptr) == msg_size);
        ptr += sizeof(uint64_t);

        // assign node_id
        node_id = *((uint32_t*)ptr);
        ptr += sizeof(uint32_t);

        // assign tie_breaker_id
        tie_breaker_id = *((uint64_t*)ptr);
        ptr += sizeof(uint64_t);

        // assign secret_share_vec
        for (uint64_t i = 0; i < secret_share_vec.size(); i++) {
            secret_share_vec[i] = *((uint32_t*)ptr);
            ptr += sizeof(uint32_t);
        }
    }

    void set_tie_breaker_id(uint64_t val) { this->tie_breaker_id = val; }

    uint64_t get_tie_breaker_id() { return this->tie_breaker_id; }

    // copy constructor
    SecretShareTuple(const SecretShareTuple& other) {
        node_id = other.node_id;
        tie_breaker_id = other.tie_breaker_id;
        secret_share_vec = other.secret_share_vec;
        detect_length_k = other.detect_length_k;
        max_degree = other.max_degree;
    }

    SecretShareTuple& operator=(const SecretShareTuple& other) {
        node_id = other.node_id;
        tie_breaker_id = other.tie_breaker_id;
        secret_share_vec = other.secret_share_vec;
        detect_length_k = other.detect_length_k;
        max_degree = other.max_degree;
        return *this;
    }

    bool operator==(const SecretShareTuple& other) {
        if (node_id != other.node_id) {
            cout << "[DEBUG] node_id not the same" << endl;
            cout << "[DEBUG] original type: " << node_id << endl;
            cout << "[DEBUG] comparison type: " << other.node_id << endl;
            return false;
        }
        if (tie_breaker_id != other.tie_breaker_id) {
            cout << "[DEBUG] tie_breaker_id not the same" << endl;
            cout << "[DEBUG] original tie_breaker_id: " << tie_breaker_id
                 << endl;
            cout << "[DEBUG] comparison tie_breaker_id: "
                 << other.tie_breaker_id << endl;
            return false;
        }
        if (detect_length_k != other.detect_length_k) {
            cout << "[DEBUG] detect length not the same" << endl;
            cout << "[DEBUG] original length: " << detect_length_k << endl;
            cout << "[DEBUG] comparison length: " << other.detect_length_k
                 << endl;
            return false;
        }
        if (max_degree != other.max_degree) {
            cout << "[DEBUG] max degree not the same" << endl;
            cout << "[DEBUG] original max degree: " << max_degree << endl;
            cout << "[DEBUG] comparison max degree: " << other.max_degree
                 << endl;
            return false;
        }
        if (secret_share_vec.size() != other.secret_share_vec.size()) {
            cout << "[DEBUG] secret share vec size not the same" << endl;
            cout << "[DEBUG] original size: " << secret_share_vec.size()
                 << endl;
            cout << "[DEBUG] comparison size: " << other.secret_share_vec.size()
                 << endl;
            return false;
        }
        for (uint64_t i = 0; i < secret_share_vec.size(); i++) {
            if (secret_share_vec[i] != other.secret_share_vec[i]) {
                cout << "[DEBUG] secret_share_vec index " << i << " differs"
                     << endl;
                cout << "[DEBUG] first val: " << secret_share_vec[i] << endl;
                cout << "[DEBUG] second val: " << other.secret_share_vec[i]
                     << endl;
                return false;
            }
        }
        return true;
    }

    bool operator!=(const SecretShareTuple& other) { return !(*this == other); }

    // set the res to xor results of others
    void xorTuple(vector<SecretShareTuple*> others) {
        node_id = others[0]->node_id ^ others[1]->node_id;
        tie_breaker_id = others[0]->tie_breaker_id ^ others[1]->tie_breaker_id;
        uint64_t size = this->size();
        for (uint64_t i = 0; i < size; i++) {
            this->secret_share_vec[i] =
                others[0]->get_secret_share_vec_by_index(i) ^
                others[1]->get_secret_share_vec_by_index(i);
        }

        for (uint64_t i = 2; i < others.size(); i++) {
            node_id ^= others[i]->node_id;
            tie_breaker_id ^= others[i]->tie_breaker_id;
            // cout << "others: " << others[i]->node_id << endl;
            // cout << "node_id after " << i << " " << node_id << endl;
            for (uint64_t j = 0; j < size; j++) {
                this->secret_share_vec[j] ^=
                    others[i]->get_secret_share_vec_by_index(j);
            }
        }
    }

    int get_max_degree() { return max_degree; }

    int get_detect_length_k() { return detect_length_k; }

    int get_node_id() { return node_id; }

    void set_node_id(uint32_t node_id_) { node_id = node_id_; }

    uint32_t get_secret_share_vec_by_index(uint64_t index) {
        MYASSERT(index <= size());
        return secret_share_vec[index];
    }

    void set_secret_share_vec_by_index(uint64_t index, uint32_t v) {
        MYASSERT(index <= size());
        secret_share_vec[index] = v;
    }

    uint64_t size() {
        MYASSERT(secret_share_vec.size() ==
                 static_cast<uint64_t>(max_degree * (detect_length_k + 1)));
        return secret_share_vec.size();
    }

    void print() {
        cout << "[DEBUG] Begin of printing the secret share tuple..." << endl;
        cout << "Node id: " << node_id << endl;
        cout << "Tie breaker id: " << this->tie_breaker_id << endl;
        for (int i = 0; i < max_degree; i++) {
            for (int j = 0; j < detect_length_k + 1; j++) {
                int index = i * (detect_length_k + 1) + j;
                cout << get_secret_share_vec_by_index(index) << "\t";
            }
            cout << endl;
        }
        cout << "[DEBUG] End of printing the secret share tuple..." << endl;
    }

    string serialize() {
        // `node_id`, `tie_breaker_id` and `secret_share_vec`
        uint64_t msg_size =
            sizeof(uint32_t) * (1 + secret_share_vec.size()) + sizeof(uint64_t);
        // add the size of `msg_size`
        msg_size += sizeof(uint64_t);
        string res(msg_size, 0);
        char* ptr = (char*)(res.c_str());
        // write msg_size
        uint64_t* write_ptr = (uint64_t*)ptr;
        *write_ptr = msg_size;
        ptr += sizeof(uint64_t);
        // write node_id
        uint32_t* write_node_id_ptr = (uint32_t*)ptr;
        *write_node_id_ptr = node_id;
        ptr += sizeof(uint32_t);
        // write tie_breaker_id
        uint64_t* write_tie_breaker_id_ptr = (uint64_t*)ptr;
        *write_tie_breaker_id_ptr = tie_breaker_id;
        ptr += sizeof(uint64_t);
        for (uint64_t i = 0; i < secret_share_vec.size(); i++) {
            uint32_t* write_svec_ptr = (uint32_t*)ptr;
            *write_svec_ptr = secret_share_vec[i];
            ptr += sizeof(uint32_t);
        }
        return res;
    }

    uint32_t getEndNodeShare() {
        return get_secret_share_vec_by_index(this->detect_length_k - 1);
    }

    uint32_t getNodeValShare() { return (getEndNodeShare() ^ get_node_id()); }

    // Only works for a path with max_degree equals 1
    // append the nodes to the path_shares
    void extractSinglePath(vector<uint32_t>& path_shares) {
        MYASSERT(max_degree == 1);
        for (int i = 0; i < detect_length_k + 1; i++) {
            path_shares.push_back(secret_share_vec[i]);
        }
    }

    void extractAndAppendNodeAndNeighbos(vector<uint32_t>& secret_shares) {
        secret_shares.push_back(this->node_id);
        for (int i = 0; i < max_degree; i++) {
            secret_shares.push_back(
                secret_share_vec[i * (detect_length_k + 1) + detect_length_k]);
        }
    }

    void updateNeighbors(vector<uint32_t>& updated_neighbors) {
        MYASSERT(updated_neighbors.size() == static_cast<uint64_t>(max_degree));
        for (int i = 0; i < max_degree; i++) {
            secret_share_vec[i * (detect_length_k + 1) + detect_length_k] =
                updated_neighbors[i];
        }
    }

    void padPathTuple(int curr_max_degree, int curr_detect_length_k) {
        MYASSERT(this->max_degree == 1);
        // push an empty node for padding
        this->secret_share_vec.push_back(0);

        this->max_degree = curr_max_degree;
        this->detect_length_k = curr_detect_length_k;

        for (int i = 1; i < this->max_degree; i++) {
            for (int j = 0; j < this->detect_length_k + 1; j++) {
                this->secret_share_vec.push_back(this->secret_share_vec[j]);
            }
        }
        MYASSERT(this->size() ==
                 static_cast<uint64_t>(max_degree * (detect_length_k + 1)));
    }

    void padEdgeTuple(int curr_max_degree, int curr_detect_length_k) {
        MYASSERT(this->max_degree == curr_max_degree);
        MYASSERT((this->detect_length_k + 1) == curr_detect_length_k);
        this->detect_length_k = curr_detect_length_k;
        for (int i = 0; i < this->max_degree; i++) {
            secret_share_vec.insert(
                secret_share_vec.begin() + i * (this->detect_length_k + 1), 0);
        }
        MYASSERT(this->size() ==
                 static_cast<uint64_t>(max_degree * (detect_length_k + 1)));
    }
};

// When the client formats messages of secret shares to the servers, they are
// responsible for making sure that the sequence is correct. Server should only
// insert the secret share tuples without worrying about the sequence.
class ServerSecretShares {
   public:
    vector<unique_ptr<SecretShareTuple>> svec;

    void insert(unique_ptr<SecretShareTuple>& s) { svec.push_back(move(s)); }

    uint64_t size() const { return svec.size(); }

    void print() {
        for (uint64_t i = 0; i < size(); i++) {
            cout << "The " << i << " element" << endl;
            svec[i]->print();
        }
    }

    bool operator==(const ServerSecretShares& other) {
        if (this->size() != other.size()) {
            cout << "[DEBUG] size is different" << endl;
            return false;
        }
        uint64_t size = this->size();
        for (uint64_t i = 0; i < size; i++) {
            if (*(this->svec[i].get()) != *(other.svec[i].get())) {
                cout << "[DEBUG] " << i << " tuple is different" << endl;
                return false;
            }
        }
        return true;
    }

    void clear() { svec.clear(); }

    // Format (# of tuples, list of serialized tuples)
    string serialize() {
        // use the first one to record the $ of SecretShareTuple.
        string res(sizeof(uint64_t), 0);
        for (uint64_t i = 0; i < svec.size(); i++) {
            string serialized_tuple = svec[i]->serialize();
            res.append(serialized_tuple);
        }
        uint64_t* ptr = (uint64_t*)res.c_str();
        *ptr = svec.size();
        return res;
    }

    void serializeWorker(uint64_t start, uint64_t end, string& res) {
        MYASSERT(res.empty());
        for (uint64_t i = start; i < end; i++) {
            res.append(this->svec[i]->serialize());
        }
    }

    string serializeParallel(int parallel_num) {
        uint64_t total_elem = this->size();
        uint64_t elem_per_thread =
            total_elem / static_cast<uint64_t>(parallel_num);
        vector<string> serialized_strs(parallel_num);
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            uint64_t begin = static_cast<uint64_t>(i) * elem_per_thread;
            uint64_t end = static_cast<uint64_t>(i + 1) * elem_per_thread;
            if (i == parallel_num - 1) {
                end = total_elem;
            }
            thread t(&ServerSecretShares::serializeWorker, this, begin, end,
                     ref(serialized_strs[i]));
            t_vec.push_back(move(t));
        }
        for (auto& t : t_vec) {
            t.join();
        }
        string serialized_str(sizeof(uint64_t), 0);
        *(uint64_t*)serialized_str.c_str() = this->svec.size();
        for (int i = 0; i < parallel_num; i++) {
            serialized_str.append(serialized_strs[i]);
            // cout << i << " str: " << serialized_strs[i].size()
            //      << " total str size: " << serialized_str.size() << endl;
        }
        return serialized_str;
    }

    void appendFromServerSecretShares(unique_ptr<ServerSecretShares>& ss) {
        for (uint64_t i = 0; i < ss->size(); i++) {
            this->insert(ss->svec[i]);
        }
    }

    void mergeFromSSVec(vector<unique_ptr<ServerSecretShares>>& s_vec) {
        for (uint64_t i = 0; i < s_vec.size(); i++) {
            this->appendFromServerSecretShares(s_vec[i]);
        }
    }

    ServerSecretShares() {}

    // this remove the original data from ss, ss only has invalid ptr
    ServerSecretShares(ServerSecretShares* ss) {
        for (uint64_t i = 0; i < ss->size(); i++) {
            this->insert(ss->svec[i]);
        }
    }

    void copyFromServerSecretShares(unique_ptr<ServerSecretShares>& ss) {
        this->clear();
        for (uint64_t i = 0; i < ss->size(); i++) {
            // copy from the ptr, ss is still valid
            unique_ptr<SecretShareTuple> t =
                make_unique<SecretShareTuple>(*ss->svec[i].get());
            this->insert(t);
        }
    }

    // Construct an object using serialized string
    ServerSecretShares(string serialized_str, int max_degree,
                       int detect_length_k) {
        uint64_t svec_num = *((uint64_t*)serialized_str.c_str());
        char* ptr = (char*)serialized_str.c_str();
        // move ptr by sizeof(uint64_t)
        ptr += sizeof(uint64_t);

        for (uint64_t i = 0; i < svec_num; i++) {
            uint64_t tuple_msg_size = *(uint64_t*)ptr;
            string tuple_msg = string(ptr, tuple_msg_size);
            unique_ptr<SecretShareTuple> t = make_unique<SecretShareTuple>(
                tuple_msg, max_degree, detect_length_k);
            insert(t);
            ptr += tuple_msg_size;
        }
    }

    void deserializeWorker(int max_degree, int detect_length_k,
                           uint64_t start_index, uint64_t end_index, char* ptr,
                           unique_ptr<ServerSecretShares>& ss) {
        MYASSERT(ss->size() == 0);
        uint64_t ss_tuple_size =
            sizeof(uint64_t) +
            (static_cast<uint64_t>(max_degree) *
                 static_cast<uint64_t>(detect_length_k + 1) +
             1) *
                sizeof(uint32_t) +
            sizeof(uint64_t);
        char* curr_ptr = ptr;
        for (uint64_t i = start_index; i < end_index; i++) {
            string tuple_msg(curr_ptr, ss_tuple_size);
            unique_ptr<SecretShareTuple> t = make_unique<SecretShareTuple>(
                tuple_msg, max_degree, detect_length_k);
            ss->insert(t);
            curr_ptr += ss_tuple_size;
        }
    }

    // Construct an object using serialized string in parallel
    ServerSecretShares(string serialized_str, int max_degree,
                       int detect_length_k, int parallel_num) {
        uint64_t svec_num = *((uint64_t*)serialized_str.c_str());
        char* ptr = (char*)serialized_str.c_str();
        // move ptr by sizeof(uint64_t)
        ptr += sizeof(uint64_t);

        uint64_t ss_tuple_size =
            sizeof(uint64_t) +
            (static_cast<uint64_t>(max_degree) *
                 static_cast<uint64_t>(detect_length_k + 1) +
             1) *
                sizeof(uint32_t) +
            sizeof(uint64_t);

        // cout << ss_tuple_size * svec_num + sizeof(uint64_t) << " "
        //      << serialized_str.size() << endl;
        MYASSERT((ss_tuple_size * svec_num + sizeof(uint64_t)) ==
                 serialized_str.size());

        uint64_t elem_per_thread =
            svec_num / static_cast<uint64_t>(parallel_num);

        vector<unique_ptr<ServerSecretShares>> ss_tuples_vec;
        for (int i = 0; i < parallel_num; i++) {
            ss_tuples_vec.push_back(move(make_unique<ServerSecretShares>()));
        }

        vector<thread> t_vec;

        for (int i = 0; i < parallel_num; i++) {
            uint64_t begin = static_cast<uint64_t>(i) * elem_per_thread;
            uint64_t end = static_cast<uint64_t>(i + 1) * elem_per_thread;
            if (i == parallel_num - 1) {
                end = svec_num;
            }
            thread t(&ServerSecretShares::deserializeWorker, this, max_degree,
                     detect_length_k, begin, end, ptr + begin * ss_tuple_size,
                     ref(ss_tuples_vec[i]));
            t_vec.push_back(move(t));
        }

        for (auto& t : t_vec) {
            t.join();
        }

        for (int i = 0; i < parallel_num; i++) {
            this->appendFromServerSecretShares(ss_tuples_vec[i]);
        }
    }

    void fillServerSecretSharesFromStr(string serialized_str, int max_degree,
                                       int detect_length_k, int parallel_num) {
        MYASSERT(this->size() == 0);
        uint64_t svec_num = *((uint64_t*)serialized_str.c_str());
        char* ptr = (char*)serialized_str.c_str();
        // move ptr by sizeof(uint64_t)
        ptr += sizeof(uint64_t);

        uint64_t ss_tuple_size =
            sizeof(uint64_t) +
            (static_cast<uint64_t>(max_degree) *
                 static_cast<uint64_t>(detect_length_k + 1) +
             1) *
                sizeof(uint32_t) +
            sizeof(uint64_t);

        // cout << ss_tuple_size * svec_num + sizeof(uint64_t) << " "
        //      << serialized_str.size() << endl;
        MYASSERT((ss_tuple_size * svec_num + sizeof(uint64_t)) ==
                 serialized_str.size());

        uint64_t elem_per_thread =
            svec_num / static_cast<uint64_t>(parallel_num);

        vector<unique_ptr<ServerSecretShares>> ss_tuples_vec;
        for (int i = 0; i < parallel_num; i++) {
            ss_tuples_vec.push_back(move(make_unique<ServerSecretShares>()));
        }

        vector<thread> t_vec;

        for (int i = 0; i < parallel_num; i++) {
            uint64_t begin = static_cast<uint64_t>(i) * elem_per_thread;
            uint64_t end = static_cast<uint64_t>(i + 1) * elem_per_thread;
            if (i == parallel_num - 1) {
                end = svec_num;
            }
            thread t(&ServerSecretShares::deserializeWorker, this, max_degree,
                     detect_length_k, begin, end, ptr + begin * ss_tuple_size,
                     ref(ss_tuples_vec[i]));
            t_vec.push_back(move(t));
        }

        for (auto& t : t_vec) {
            t.join();
        }

        for (int i = 0; i < parallel_num; i++) {
            this->appendFromServerSecretShares(ss_tuples_vec[i]);
        }
    }

    // empty src and move
    void moveServerShares(ServerSecretShares* src) {
        this->clear();
        for (uint64_t i = 0; i < src->size(); i++) {
            this->svec.push_back(move(src->svec[i]));
        }
    }

    void resortByPartition(vector<uint64_t>& smaller_indices,
                           vector<uint64_t>& larger_indices,
                           uint64_t begin_index, uint64_t end_index,
                           uint64_t pivot_index) {
        MYASSERT(end_index > begin_index);
        vector<unique_ptr<SecretShareTuple>> tmp;
        bool pivot_found = false;
        for (uint64_t i = 0; i < smaller_indices.size(); i++) {
            MYASSERT(smaller_indices[i] >= begin_index);
            MYASSERT(smaller_indices[i] < end_index);
            if (pivot_index == smaller_indices[i]) {
                pivot_found = true;
                continue;
            }
            tmp.push_back(move(this->svec[smaller_indices[i]]));
        }
        // cout << "pivot in resort: " << begin_index << " " << end_index << " "
        //      << pivot_index << endl;
        MYASSERT(pivot_found);
        tmp.push_back(move(this->svec[pivot_index]));

        for (uint64_t i = 0; i < larger_indices.size(); i++) {
            MYASSERT(larger_indices[i] >= begin_index);
            MYASSERT(larger_indices[i] < end_index);
            tmp.push_back(move(this->svec[larger_indices[i]]));
        }
        // cout << tmp.size() << " " << end_index << " " << begin_index << endl;
        MYASSERT(tmp.size() == (end_index - begin_index));
        for (uint64_t i = begin_index; i < end_index; i++) {
            this->svec[i] = move(tmp[i - begin_index]);
        }
    }

    void resortBySortResult(vector<uint64_t>& sorted_index,
                            uint64_t begin_index, uint64_t end_index) {
        MYASSERT(end_index > begin_index);
        vector<unique_ptr<SecretShareTuple>> tmp;
        for (uint64_t i = 0; i < sorted_index.size(); i++) {
            // cout << sorted_index[i] << " " << begin_index << " " << end_index
            //      << endl;
            MYASSERT(sorted_index[i] >= begin_index);
            MYASSERT(sorted_index[i] < end_index);
            tmp.push_back(move(this->svec[sorted_index[i]]));
        }
        MYASSERT(tmp.size() == (end_index - begin_index));
        for (uint64_t i = begin_index; i < end_index; i++) {
            this->svec[i] = move(tmp[i - begin_index]);
        }
    }

    // Given ss, extract the node value and end node local shares from the ss
    // and return the created vector. node val of each tuple is defined as
    // `node_id ^ end_node`.
    // `end_node` is `detect_length_k-1`'s elements of each tuple.
    // For edge tuple, it's 0.
    // For path tuple, it's a non-zero node (the last node in the path)
    void extractNodeValFromServerSecretShare(vector<uint32_t>& node_val_shares,
                                             vector<uint32_t>& end_node_shares,
                                             uint64_t begin_index,
                                             uint64_t end_index) {
        MYASSERT(node_val_shares.empty());
        MYASSERT(end_node_shares.empty());
        MYASSERT(begin_index < end_index);
        MYASSERT(end_index <= this->svec.size());
        for (uint64_t i = begin_index; i < end_index; i++) {
            // uint32_t end_node_share = this->svec[i]->getEndNodeShare();
            // uint32_t node_val_share = this->svec[i]->getNodeValShare();
            // cout << "extract " << i << " " << this->svec.size() << endl;
            // cout << "val: " << this->svec[i]->getEndNodeShare() << endl;
            end_node_shares.push_back(this->svec[i]->getEndNodeShare());
            node_val_shares.push_back(this->svec[i]->getNodeValShare());
        }
    }

    void extractTieBreakerFromServerSecretShare(
        vector<uint64_t>& tie_breaker_shares, uint64_t begin_index,
        uint64_t end_index) {
        MYASSERT(tie_breaker_shares.empty());
        MYASSERT(begin_index < end_index);
        MYASSERT(end_index <= this->svec.size());
        for (uint64_t i = begin_index; i < end_index; i++) {
            tie_breaker_shares.push_back(this->svec[i]->get_tie_breaker_id());
        }
    }

    void extractSinglePathFromRange(vector<uint32_t>& path_shares,
                                    uint64_t begin_index, uint64_t end_index) {
        MYASSERT(path_shares.empty());
        for (uint64_t i = begin_index; i < end_index; i++) {
            this->svec[i]->extractSinglePath(path_shares);
        }
    }

    void extractNodeIdSharesOnly(vector<uint32_t>& node_shares,
                                 uint64_t begin_index, uint64_t end_index) {
        MYASSERT(node_shares.empty());
        MYASSERT(begin_index < end_index);
        MYASSERT(end_index <= this->svec.size());
        for (uint64_t i = begin_index; i < end_index; i++) {
            node_shares.push_back(this->svec[i]->get_node_id());
        }
    }

    void extractPathTuples(unique_ptr<ServerSecretShares>& ss) {
        for (uint64_t j = 0; j < this->size(); j++) {
            int curr_detect_length_k = this->svec[j]->get_detect_length_k();
            int curr_max_degree = this->svec[j]->get_max_degree();
            for (int i = 0; i < curr_max_degree; i++) {
                unique_ptr<SecretShareTuple> tuple =
                    make_unique<SecretShareTuple>(
                        /*node_id=*/0, /*max_degree=*/1, curr_detect_length_k);
                copy(this->svec[j]->secret_share_vec.begin() +
                         (i * (curr_detect_length_k + 1)),
                     this->svec[j]->secret_share_vec.begin() +
                         ((i + 1) * (curr_detect_length_k + 1)),
                     tuple->secret_share_vec.begin());
                ss->insert(tuple);
            }
        }
    }

    // [For testing purpose only] sort the svec using the plain text
    void sortTuple() {
        stable_sort(this->svec.begin(), this->svec.end(),
                    [this](const unique_ptr<SecretShareTuple>& t1,
                           const unique_ptr<SecretShareTuple>& t2) {
                        uint32_t e1 = t1->getEndNodeShare();
                        uint32_t e2 = t2->getEndNodeShare();
                        uint32_t v1 = t1->getNodeValShare();
                        uint32_t v2 = t2->getNodeValShare();
                        bool node_val_larger = (v1 > v2);
                        bool node_val_equal = (v1 == v2);
                        bool end_node_larger = (e1 > e2);
                        bool first_elem_larger =
                            (node_val_larger) ||
                            (node_val_equal && end_node_larger);
                        // t1->print();
                        // t2->print();
                        // cout << "e1 " << e1 << " e2 " << e2 << endl;
                        // cout << "v1 " << v1 << " v2 " << v2 << endl;
                        // cout << "first appears before second? "
                        //      << !first_elem_larger << endl;
                        // when two elements are equivelant, should always
                        // return false
                        if (node_val_equal && (e1 == e2)) {
                            return false;
                        }
                        return !first_elem_larger;
                    });
    }

    // for debugging purpose, it assumes each node has its own edges
    // TODO: possibly add check of sequence of tuples
    bool checkSorted() {
        MYASSERT(!svec.empty());
        MYASSERT(svec[0]->get_node_id() == 1);
        uint32_t current_node_id = svec[0]->get_node_id();
        bool previous_tuple_is_path = false;
        for (uint64_t i = 1; i < svec.size(); i++) {
            // if we encountered a path tuple, it must end with
            // `current_node_id`.
            if (svec[i]->get_node_id() == 0) {
                if (!previous_tuple_is_path) {
                    previous_tuple_is_path = true;
                } else {
                    if (!(svec[i - 1]->get_tie_breaker_id() <
                          svec[i]->get_tie_breaker_id())) {
                        cout << "[DEBUG] " << i << " tie_breaker_id "
                             << svec[i]->get_tie_breaker_id()
                             << " is smaller than the previous path "
                             << svec[i - 1]->get_tie_breaker_id() << endl;
                        return false;
                    }
                }
                if (!(svec[i]->getEndNodeShare() == current_node_id)) {
                    cout << "[DEBUG] " << i << " tuple end node "
                         << svec[i]->getEndNodeShare() << " is different from "
                         << current_node_id << endl;
                    return false;
                }
            } else {
                previous_tuple_is_path = false;
                uint32_t tuple_node_id = svec[i]->get_node_id();
                if (!((tuple_node_id > current_node_id) &&
                      (tuple_node_id == current_node_id + 1))) {
                    cout << "[DEBUG] " << i << " tuple node id "
                         << tuple_node_id << " is not larger than "
                         << current_node_id << endl;
                    return false;
                }
                current_node_id = tuple_node_id;
            }
        }
        cout << "check sorted passed\n";
        return true;
    }
};

class Server {
   private:
    // For three servers, the first and second share vectors should be
    // stored in the sequence of (s1,s2), (s2,s3), and (s3,s1) respectively.
    unique_ptr<ServerSecretShares> first_s;
    unique_ptr<ServerSecretShares> second_s;

    SERVER_ID server_id;
    uint32_t id_range;
    int max_degree;
    int detect_length_k;
    int parallel_num;
    int seed12_permute;
    int seed23_permute;
    int seed31_permute;
    int seedA;
    int seedB;
    uint64_t tie_breaker_range;

    // WARNING: Should only be invoked inside `createRandomness` as the rand
    // seed must be set before calling this.
    unique_ptr<SecretShareTuple> createRandomSecretShare() {
        unique_ptr<SecretShareTuple> res =
            make_unique<SecretShareTuple>(0, max_degree, detect_length_k);
        res->set_node_id(rand() % id_range);
        res->set_tie_breaker_id(
            ((static_cast<uint64_t>(rand()) << 32) + rand()) %
            tie_breaker_range);
        uint64_t size = res->size();
        for (uint64_t i = 0; i < size; i++) {
            res->set_secret_share_vec_by_index(i, rand() % id_range);
        }
        return res;
    }

    unique_ptr<ServerSecretShares> createEmptyServerShares(uint64_t size) {
        unique_ptr<ServerSecretShares> res = make_unique<ServerSecretShares>();
        for (uint64_t i = 0; i < size; i++) {
            unique_ptr<SecretShareTuple> empty_tuple =
                make_unique<SecretShareTuple>(0, max_degree, detect_length_k);
            res->insert(empty_tuple);
        }
        return res;
    }

    void xorSecretShareTupleHandler(ServerSecretShares* dst,
                                    vector<ServerSecretShares*>& tuples, int id,
                                    int parallel_num) {
        uint64_t size = dst->size();
        for (const auto& t : tuples) {
            MYASSERT(t->size() == size);
        }

        uint64_t start = size / parallel_num * id;
        uint64_t end = size / parallel_num * (id + 1);
        if (id == parallel_num - 1) {
            end = size;
        }
        for (uint64_t i = start; i < end; i++) {
            vector<SecretShareTuple*> t_ptrs;
            for (uint64_t j = 0; j < tuples.size(); j++) {
                t_ptrs.push_back(tuples[j]->svec[i].get());
            }
            dst->svec[i]->xorTuple(t_ptrs);
        }
    }

    bool validateParam() {
        if (server_id == S1) {
            return ((seed12_permute != 0) && (seed23_permute == 0) &&
                    (seed31_permute != 0) && (seedA != 0) && (seedB != 0));
        } else if (server_id == S2) {
            return ((seed12_permute != 0) && (seed23_permute != 0) &&
                    (seed31_permute == 0) && (seedA == 0) && (seedB != 0));
        } else {
            return ((seed12_permute == 0) && (seed23_permute != 0) &&
                    (seed31_permute != 0) && (seedA != 0) && (seedB == 0));
        }
    }

    uint64_t getSecretShareSize() {
        if (first_s->size() != second_s->size()) {
            cout << "[ERROR], first size: " << first_s->size()
                 << ", second size: " << second_s->size() << endl;
        }
        MYASSERT(first_s->size() == second_s->size());
        return first_s->size();
    }

   public:
    // 0 seed value indicates it's not set.
    Server(SERVER_ID server_id_, uint32_t id_range_, int max_degree_,
           int detect_length_k_, int parallel_num_, int seed12_permute_,
           int seed23_permute_, int seed31_permute_, int seedA_, int seedB_,
           uint64_t tie_breaker_range_)
        : server_id(server_id_),
          id_range(id_range_),
          max_degree(max_degree_),
          detect_length_k(detect_length_k_),
          parallel_num(parallel_num_),
          seed12_permute(seed12_permute_),
          seed23_permute(seed23_permute_),
          seed31_permute(seed31_permute_),
          seedA(seedA_),
          seedB(seedB_),
          tie_breaker_range(tie_breaker_range_) {
        MYASSERT(is_power_of_2(tie_breaker_range));
        MYASSERT(is_power_of_2(id_range));
        MYASSERT(validateParam());
        first_s = make_unique<ServerSecretShares>();
        second_s = make_unique<ServerSecretShares>();
    }

    // a is set to i and b,c set to tie_breaker_range (11..11).
    void assignTieBreakerIds() {
        MYASSERT(first_s->size() == second_s->size());
        for (uint64_t i = 0; i < first_s->size(); i++) {
            if (server_id == SERVER_ID::S1) {
                first_s->svec[i]->set_tie_breaker_id(i);
                second_s->svec[i]->set_tie_breaker_id(this->tie_breaker_range);
            }
            if (server_id == SERVER_ID::S2) {
                first_s->svec[i]->set_tie_breaker_id(this->tie_breaker_range);
                second_s->svec[i]->set_tie_breaker_id(this->tie_breaker_range);
            }
            if (server_id == SERVER_ID::S3) {
                first_s->svec[i]->set_tie_breaker_id(this->tie_breaker_range);
                second_s->svec[i]->set_tie_breaker_id(i);
            }
        }
    }

    // shuffle step 1 for server 1
    // server 1 only needs one step
    unique_ptr<ServerSecretShares> shuffle_s1_step1() {
        MYASSERT(server_id == SERVER_ID::S1);
        ServerSecretShares z12 = createRandomness(seed12_permute);
        ServerSecretShares z31 = createRandomness(seed31_permute);
        vector<ServerSecretShares*> xor_vec1 = {this->first_s.get(),
                                                this->second_s.get(), &z12};
        unique_ptr<ServerSecretShares> x1 =
            xorSecretShareTupleAndCreateDst(xor_vec1);
        // x1 = permute_12(A^B^z12)
        shuffleSecretShares(*x1.get(), seed12_permute);
        vector<ServerSecretShares*> xor_vec2 = {x1.get(), &z31};
        unique_ptr<ServerSecretShares> x2 =
            xorSecretShareTupleAndCreateDst(xor_vec2);

        // x2 = permute_13(x1^z31)
        shuffleSecretShares(*x2.get(), seed31_permute);

        // A=A_hat
        createRandomnessToDst(first_s.get(), seedA);

        // B=B_hat
        createRandomnessToDst(second_s.get(), seedB);
        return x2;
    }

    // shuffle step 1 for server 2
    unique_ptr<ServerSecretShares> shuffle_s2_step1() {
        MYASSERT(server_id == SERVER_ID::S2);
        ServerSecretShares z12 = createRandomness(seed12_permute);
        // second_s is c and first_s is b
        vector<ServerSecretShares*> xor_vec = {this->second_s.get(), &z12};
        unique_ptr<ServerSecretShares> y1 =
            xorSecretShareTupleAndCreateDst(xor_vec);
        // y1=permute_12(c^z12)
        shuffleSecretShares(*y1.get(), seed12_permute);
        return y1;
    }

    unique_ptr<ServerSecretShares> shuffle_s2_step2(ServerSecretShares& x2) {
        MYASSERT(server_id == SERVER_ID::S2);
        ServerSecretShares z23 = createRandomness(seed23_permute);
        vector<ServerSecretShares*> xor_vec1 = {&x2, &z23};
        unique_ptr<ServerSecretShares> x3 =
            xorSecretShareTupleAndCreateDst(xor_vec1);
        // x3=permute_23(x2^z23)
        shuffleSecretShares(*x3.get(), seed23_permute);

        // b=b^hat
        createRandomnessToDst(first_s.get(), seedB);

        vector<ServerSecretShares*> xor_vec2 = {x3.get(), first_s.get()};
        // c1=x3^b_hat
        unique_ptr<ServerSecretShares> c1 =
            xorSecretShareTupleAndCreateDst(xor_vec2);

        return c1;
    }

    void shuffle_s2_step3(ServerSecretShares& c1, ServerSecretShares& c2) {
        MYASSERT(server_id == SERVER_ID::S2);
        vector<ServerSecretShares*> xor_vec = {&c1, &c2};
        // c=c1^c2
        xorSecretShareTuple(second_s.get(), xor_vec);
    }

    unique_ptr<ServerSecretShares> shuffle_s3_step1(ServerSecretShares& y1) {
        MYASSERT(server_id == SERVER_ID::S3);
        ServerSecretShares z31 = createRandomness(seed31_permute);
        vector<ServerSecretShares*> xor_vec1 = {&y1, &z31};
        unique_ptr<ServerSecretShares> y2 =
            xorSecretShareTupleAndCreateDst(xor_vec1);
        // y2=permute_31(y1^z31)
        shuffleSecretShares(*y2.get(), seed31_permute);

        ServerSecretShares z23 = createRandomness(seed23_permute);
        vector<ServerSecretShares*> xor_vec2 = {y2.get(), &z23};
        unique_ptr<ServerSecretShares> y3 =
            xorSecretShareTupleAndCreateDst(xor_vec2);
        // y3=permute_23(y2^z23)
        shuffleSecretShares(*y3.get(), seed23_permute);

        // second_s is a in server 3
        // second_s = a_hat
        createRandomnessToDst(second_s.get(), seedA);
        vector<ServerSecretShares*> xor_vec3 = {y3.get(), second_s.get()};

        // c2=y3^a_hat
        unique_ptr<ServerSecretShares> c2 =
            xorSecretShareTupleAndCreateDst(xor_vec3);
        return c2;
    }

    void shuffle_s3_step2(ServerSecretShares& c1, ServerSecretShares& c2) {
        MYASSERT(server_id == SERVER_ID::S3);
        vector<ServerSecretShares*> xor_vec = {&c1, &c2};
        xorSecretShareTuple(first_s.get(), xor_vec);
    }

    void shuffleSecretShares(ServerSecretShares& s, int seed) {
        shuffle(s.svec.begin(), s.svec.end(), default_random_engine(seed));
    }

    // Create randomness of the given length of the secret share vector.
    ServerSecretShares createRandomness(int seed) {
        ServerSecretShares randss;
        srand(seed);
        uint64_t size = getSecretShareSize();
        for (uint64_t i = 0; i < size; i++) {
            unique_ptr<SecretShareTuple> r = createRandomSecretShare();
            randss.insert(r);
        }
        return randss;
    }

    // this will clear the dst even if it's one of the shares
    void createRandomnessToDst(ServerSecretShares* dst, int seed) {
        srand(seed);
        uint64_t size = getSecretShareSize();
        // if dst is one of the shares, we can only clear it after we get the
        // size.
        dst->clear();
        for (uint64_t i = 0; i < size; i++) {
            unique_ptr<SecretShareTuple> r = createRandomSecretShare();
            dst->insert(r);
        }
    }

    void createRandomnessToDstWithSize(ServerSecretShares* dst, int seed,
                                       uint64_t tuple_size) {
        srand(seed);
        dst->clear();
        for (uint64_t i = 0; i < tuple_size; i++) {
            unique_ptr<SecretShareTuple> r = createRandomSecretShare();
            dst->insert(r);
        }
    }

    // XOR over a list of ServerSecretShares. The XOR results will be
    // written to dst.
    // dst must already have the same size
    void xorSecretShareTuple(ServerSecretShares* dst,
                             vector<ServerSecretShares*>& tuples) {
        MYASSERT(tuples.size() >= 2);
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&Server::xorSecretShareTupleHandler, this, dst,
                     ref(tuples), i, parallel_num);
            t_vec.push_back(move(t));
        }
        for (uint64_t i = 0; i < t_vec.size(); i++) {
            t_vec[i].join();
        }
    }

    void xorSecretShareTupleWithEmptyDst(ServerSecretShares* dst,
                                         vector<ServerSecretShares*>& tuples) {
        MYASSERT(dst->size() == 0);
        MYASSERT(tuples.size() >= 2);
        uint64_t tuple_num = tuples[0]->size();
        for (uint64_t i = 0; i < tuple_num; i++) {
            unique_ptr<SecretShareTuple> tuple = make_unique<SecretShareTuple>(
                0, this->max_degree, this->detect_length_k);
            dst->insert(tuple);
        }

        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&Server::xorSecretShareTupleHandler, this, dst,
                     ref(tuples), i, parallel_num);
            t_vec.push_back(move(t));
        }
        for (uint64_t i = 0; i < t_vec.size(); i++) {
            t_vec[i].join();
        }
    }

    unique_ptr<ServerSecretShares> xorSecretShareTupleAndCreateDst(
        vector<ServerSecretShares*>& tuples) {
        MYASSERT(tuples.size() >= 2);
        unique_ptr<ServerSecretShares> dst =
            createEmptyServerShares(tuples[0]->size());
        vector<thread> t_vec;
        for (int i = 0; i < parallel_num; i++) {
            thread t(&Server::xorSecretShareTupleHandler, this, dst.get(),
                     ref(tuples), i, parallel_num);
            t_vec.push_back(move(t));
        }
        for (uint64_t i = 0; i < t_vec.size(); i++) {
            t_vec[i].join();
        }
        return dst;
    }

    void insertSecrtShareTuple(SecretShareTuple& s1, SecretShareTuple& s2) {
        unique_ptr<SecretShareTuple> ptr1 = make_unique<SecretShareTuple>(s1);
        unique_ptr<SecretShareTuple> ptr2 = make_unique<SecretShareTuple>(s2);
        first_s->insert(ptr1);
        second_s->insert(ptr2);
        MYASSERT(first_s->size() == second_s->size());
    }

    void printSecretShares() {
        cout << "[DEBUG] outputting first svec...\n";
        first_s->print();
        cout << "[DEBUG] finish outputting first svec1...\n";

        cout << "[DEBUG] outputting second svec...\n";
        second_s->print();
        cout << "[DEBUG] finish outputting second svec1...\n";
    }

    void loadFirstServerSecretShares(string& serialized_str) {
        first_s = make_unique<ServerSecretShares>(
            serialized_str, max_degree, detect_length_k, parallel_num);
    }

    void loadSecondServerSecretShares(string& serialized_str) {
        second_s = make_unique<ServerSecretShares>(
            serialized_str, max_degree, detect_length_k, parallel_num);
    }

    ServerSecretShares* getFirstServerSecretShares() { return first_s.get(); }
    ServerSecretShares* getSecondServerSecretShares() { return second_s.get(); }

    unique_ptr<ServerSecretShares>& getFirstSSUniquePtr() { return first_s; }

    unique_ptr<ServerSecretShares>& getSecondSSUniquePtr() { return second_s; }

    int get_max_degree() { return max_degree; }

    int get_detect_length_k() { return detect_length_k; }

    void incDetectLength() { this->detect_length_k++; }

    void set_max_degree(int v) { this->max_degree = v; }
};

// Helper class to format edge and path tuple
class SecretShareTupleHelper {
   private:
    uint32_t id_range;
    random_device rd;
    uint64_t tie_breaker_range;

   public:
    SecretShareTupleHelper(uint32_t id_range_, uint64_t tie_breaker_range_)
        : id_range(id_range_), tie_breaker_range(tie_breaker_range_) {
        MYASSERT(is_power_of_2(tie_breaker_range));
        MYASSERT(is_power_of_2(id_range));
    }

    // return three secret share tuples
    vector<unique_ptr<SecretShareTuple>> splitTuple(
        unique_ptr<SecretShareTuple>& t) {
        unique_ptr<SecretShareTuple> t1 = make_unique<SecretShareTuple>(
            0, t->get_max_degree(), t->get_detect_length_k());
        unique_ptr<SecretShareTuple> t2 = make_unique<SecretShareTuple>(
            0, t->get_max_degree(), t->get_detect_length_k());
        unique_ptr<SecretShareTuple> t3 = make_unique<SecretShareTuple>(
            0, t->get_max_degree(), t->get_detect_length_k());

        // Create secret share of node_id
        uint32_t s1 = rd() % (id_range);
        uint32_t s2 = rd() % (id_range);
        uint32_t s3 = t->get_node_id() ^ s1 ^ s2;
        t1->set_node_id(s1);
        t2->set_node_id(s2);
        t3->set_node_id(s3);

        // Create secret share of tie_breaker_id
        uint64_t id1 =
            ((static_cast<uint64_t>(rd()) << 32) + rd()) % (tie_breaker_range);
        uint64_t id2 =
            ((static_cast<uint64_t>(rd()) << 32) + rd()) % (tie_breaker_range);
        uint64_t id3 = t->get_tie_breaker_id() ^ id1 ^ id2;
        t1->set_tie_breaker_id(id1);
        t2->set_tie_breaker_id(id2);
        t3->set_tie_breaker_id(id3);

        // Create secret share of secret_share_vec
        for (uint64_t i = 0; i < t->size(); i++) {
            uint32_t s1 = rd() % (id_range);
            uint32_t s2 = rd() % (id_range);
            uint32_t s3 = t->get_secret_share_vec_by_index(i) ^ s1 ^ s2;
            t1->set_secret_share_vec_by_index(i, s1);
            t2->set_secret_share_vec_by_index(i, s2);
            t3->set_secret_share_vec_by_index(i, s3);
        }

        vector<unique_ptr<SecretShareTuple>> res;
        res.push_back(move(t1));
        res.push_back(move(t2));
        res.push_back(move(t3));
        return res;
    }

    // edges contain all *valid* neighbor nodes and it will be padded to
    // max_degree with zero nodes.
    //
    // Return a newly created SecretShareTuple.
    unique_ptr<SecretShareTuple> formatEdgeTuple(
        uint32_t node_id, const vector<uint32_t>& original_edges,
        int max_degree, int detect_length_k) {
        vector<uint32_t> edges = original_edges;
        unique_ptr<SecretShareTuple> t = make_unique<SecretShareTuple>(
            /*node_id=*/node_id, max_degree, detect_length_k);
        MYASSERT(edges.size() <= static_cast<uint64_t>(max_degree));
        // pad edges with zeros
        for (uint64_t i = edges.size(); i < static_cast<uint64_t>(max_degree);
             i++) {
            edges.push_back(0);
        }

        // Format the original tuple t.
        // the tuple contains d vectors where each vector has
        // (detect_length_k+1) elements.
        for (int i = 0; i < max_degree; i++) {
            for (int j = 0; j < detect_length_k + 1; j++) {
                int index = i * (detect_length_k + 1) + j;
                if (j == detect_length_k) {
                    t->set_secret_share_vec_by_index(index, edges[i]);
                } else {
                    t->set_secret_share_vec_by_index(index, 0);
                }
            }
        }
        return t;
    }

    // path is a vector of length `detect_length_k`, it corresponds to
    // `detect_length_k-1` edges.
    // Returns a newly created SecretShareTuple.
    unique_ptr<SecretShareTuple> formatPathTuple(vector<uint32_t> path,
                                                 int max_degree,
                                                 int detect_length_k) {
        MYASSERT(path.size() == static_cast<uint64_t>(detect_length_k));
        unique_ptr<SecretShareTuple> t = make_unique<SecretShareTuple>(
            /*node_id=*/0, max_degree, detect_length_k);

        // Format the original tuple t.
        // the tuple contains d vectors where each vector has
        // (detect_length_k+1) elements.
        for (int i = 0; i < max_degree; i++) {
            for (int j = 0; j < detect_length_k + 1; j++) {
                int index = i * (detect_length_k + 1) + j;
                if (j < detect_length_k) {
                    t->set_secret_share_vec_by_index(index, path[j]);
                } else {
                    t->set_secret_share_vec_by_index(index, 0);
                }
            }
        }
        return t;
    }

    // path is a vector of length `detect_length_k+1`, it corresponds to
    // `detect_length_k` edges. This path is already filled by extending an
    // edge. Returns a newly created SecretShareTuple.
    unique_ptr<SecretShareTuple> formatFilledPathTuple(vector<uint32_t> path,
                                                       int max_degree,
                                                       int detect_length_k) {
        MYASSERT(path.size() == static_cast<uint64_t>(detect_length_k + 1));
        unique_ptr<SecretShareTuple> t = make_unique<SecretShareTuple>(
            /*node_id=*/0, max_degree, detect_length_k);

        // Format the original tuple t.
        // the tuple contains d vectors where each vector has
        // (detect_length_k+1) elements.
        for (int i = 0; i < max_degree; i++) {
            for (int j = 0; j < detect_length_k + 1; j++) {
                int index = i * (detect_length_k + 1) + j;
                t->set_secret_share_vec_by_index(index, path[j]);
            }
        }
        return t;
    }

    unique_ptr<SecretShareTuple> reconTuple(vector<SecretShareTuple*>& ss,
                                            int max_degree,
                                            int detect_length_k) {
        MYASSERT(ss.size() == 3);
        unique_ptr<SecretShareTuple> t =
            make_unique<SecretShareTuple>(0, max_degree, detect_length_k);
        t->set_node_id(ss[0]->get_node_id() ^ ss[1]->get_node_id() ^
                       ss[2]->get_node_id());

        t->set_tie_breaker_id(ss[0]->get_tie_breaker_id() ^
                              ss[1]->get_tie_breaker_id() ^
                              ss[2]->get_tie_breaker_id());

        for (uint64_t i = 0; i < t->size(); i++) {
            t->set_secret_share_vec_by_index(
                i, ss[0]->get_secret_share_vec_by_index(i) ^
                       ss[1]->get_secret_share_vec_by_index(i) ^
                       ss[2]->get_secret_share_vec_by_index(i));
        }
        return t;
    }

    unique_ptr<ServerSecretShares> reconServerShares(
        vector<ServerSecretShares*>& ss, int max_degree, int detect_length_k) {
        // We only accept 3 secret shares.
        MYASSERT(ss.size() == 3);
        uint64_t share_size = ss[0]->size();
        for (uint64_t i = 0; i < ss.size(); i++) {
            MYASSERT(share_size == ss[i]->size());
        }
        unique_ptr<ServerSecretShares> res = make_unique<ServerSecretShares>();
        for (uint64_t i = 0; i < share_size; i++) {
            vector<SecretShareTuple*> t_vec = {ss[0]->svec[i].get(),
                                               ss[1]->svec[i].get(),
                                               ss[2]->svec[i].get()};
            unique_ptr<SecretShareTuple> recon_t =
                reconTuple(t_vec, max_degree, detect_length_k);
            res->insert(recon_t);
        }
        return res;
    }

    // [for testing purpose only] helper function to create two secret shares
    vector<unique_ptr<SecretShareTuple>> splitTupleIntoTwo(
        unique_ptr<SecretShareTuple>& t) {
        unique_ptr<SecretShareTuple> t1 = make_unique<SecretShareTuple>(
            0, t->get_max_degree(), t->get_detect_length_k());
        unique_ptr<SecretShareTuple> t2 = make_unique<SecretShareTuple>(
            0, t->get_max_degree(), t->get_detect_length_k());

        // Create secret share of node_id
        uint32_t s1 = rd() % (id_range);
        uint32_t s2 = t->get_node_id() ^ s1;
        t1->set_node_id(s1);
        t2->set_node_id(s2);

        // create secret share of tie_breaker_id
        uint64_t id1 =
            ((static_cast<uint64_t>(rd()) << 32) + rd()) % (tie_breaker_range);
        uint64_t id2 = t->get_tie_breaker_id() ^ id1;
        t1->set_tie_breaker_id(id1);
        t2->set_tie_breaker_id(id2);

        // Create secret share of secret_share_vec
        for (uint64_t i = 0; i < t->size(); i++) {
            uint32_t s1 = rd() % (id_range);
            uint32_t s2 = t->get_secret_share_vec_by_index(i) ^ s1;
            t1->set_secret_share_vec_by_index(i, s1);
            t2->set_secret_share_vec_by_index(i, s2);
        }

        vector<unique_ptr<SecretShareTuple>> res;
        res.push_back(move(t1));
        res.push_back(move(t2));
        return res;
    }

    // [for testing purpose only] helper function to reconstruct using two
    // secret shares.
    unique_ptr<SecretShareTuple> reconTupleFromTwo(
        vector<SecretShareTuple*>& ss, int max_degree, int detect_length_k) {
        MYASSERT(ss.size() == 2);
        unique_ptr<SecretShareTuple> t =
            make_unique<SecretShareTuple>(0, max_degree, detect_length_k);
        t->set_node_id(ss[0]->get_node_id() ^ ss[1]->get_node_id());
        t->set_tie_breaker_id(ss[0]->get_tie_breaker_id() ^
                              ss[1]->get_tie_breaker_id());

        for (uint64_t i = 0; i < t->size(); i++) {
            t->set_secret_share_vec_by_index(
                i, ss[0]->get_secret_share_vec_by_index(i) ^
                       ss[1]->get_secret_share_vec_by_index(i));
        }
        return t;
    }
};

#endif  // SECRET_SHARE_LIB_HPP