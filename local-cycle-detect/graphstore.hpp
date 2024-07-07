#include <assert.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class GraphStore {
   private:
    std::unordered_map<int, std::vector<int>> g;
    std::string file_name;

    const std::vector<std::pair<int, int>> degree_bucket_ = {
        {0, 10}, {10, 20}, {20, 30}, {30, 40}, {40, 50}, {50, INT32_MAX}};

    int dfs(int node, int step, int start, std::vector<int> path,
            int64_t& total_path) {
        // If the node has no out-edge, just skip
        if (g.find(node) == g.end()) {
            return 0;
        }

        int edge_num = g[node].size();
        // int init_g_size = g.size();
        int count = 0;
        if (step == 0) {
            total_path += edge_num;
            for (int i = 0; i < edge_num; i++) {
                if (g[node][i] == start) return 1;
            }
            return 0;
        }
        for (int i = 0; i < edge_num; i++) {
            int next_hop = g[node][i];
            if (find(path.begin(), path.end(), next_hop) != path.end()) {
                continue;
            }
            std::vector<int> new_path = path;
            new_path.push_back(next_hop);
            assert(new_path.size() == (path.size() + 1));
            count += dfs(next_hop, step - 1, start, new_path, total_path);
        }
        // int current_g_size = g.size();
        // if (init_g_size != current_g_size) {
        //     for (int i = 0; i < path.size(); i++) {
        //         std::cout << "path " << i << " " << path[i] << std::endl;
        //     }
        //     assert(0);
        // }
        return count;
    }

   public:
    GraphStore(std::string file_name_) : file_name(file_name_) {}

    // Load graph data from a given file
    // Note that the graph **SHOULD NOT** contain duplicated edges.
    void load_graph() {
        set<uint32_t> unique_nodes;
        std::ifstream fs(file_name);
        int src, dst;
        int total_edges = 0;
        while (fs >> src >> dst) {
            total_edges++;
            // Make sure that there is no duplicated edges
            assert(std::find(g[src].begin(), g[src].end(), dst) ==
                   g[src].end());
            g[src].push_back(dst);
            unique_nodes.insert(src);
            unique_nodes.insert(dst);
        }
        fs.close();
        std::cout << "Finished loading, " << total_edges << " edges in total"
                  << std::endl;
        std::cout << "Total " << g.size() << " unique src nodes" << std::endl;
        cout << "total unique nodes: " << unique_nodes.size() << endl;
    }

    // Naive implementation of counting k-cycle in the graph, only used for
    // testing purposes.
    int count_cycle(int k) {
        int count = 0;
        int idx = 0;
        int64_t total_path = 0;
        for (auto kv : g) {
            idx++;
            if (idx % 200000 == 0) {
                std::cout << "searching for " << idx++ << " node, " << g.size()
                          << " nodes in total" << std::endl;
            }
            int src = kv.first;
            std::vector<int> path;
            path.push_back(src);
            count += dfs(src, k - 1, src, path, total_path);
        }
        // Because each path will be counted k times
        std::cout << "Total count: " << count << " with nodes of " << k
                  << " (note that this contains repeated cycles such as "
                     "1->2->3->1 and 2->3->1->2)"
                  << std::endl;
        std::cout << "Total path: " << total_path << std::endl;
        assert(count % k == 0);
        return count / k;
    }

    void countNodeDegrees() {
        std::vector<int> bucket_counts(degree_bucket_.size(), 0);

        for (const auto& entry : g) {
            int node = entry.first;
            const std::vector<int>& neighbors = entry.second;

            // Count neighbors
            int neighborCount = neighbors.size();

            // Check which bucket the neighbor count falls into
            for (size_t i = 0; i < degree_bucket_.size(); ++i) {
                if (neighborCount >= degree_bucket_[i].first &&
                    neighborCount < degree_bucket_[i].second) {
                    ++bucket_counts[i];
                    break;
                }
            }
        }

        for (size_t i = 0; i < degree_bucket_.size(); ++i) {
            std::cout << "Number of nodes with neighbors in the range ["
                      << degree_bucket_[i].first << ", "
                      << degree_bucket_[i].second << "): " << bucket_counts[i]
                      << std::endl;
        }
    }
};