#pragma once

#include <vector>

namespace pipnn {
class Graph {
 public:
  explicit Graph(int n = 0) : adj_(n) {}

  void Resize(int n) { adj_.assign(n, {}); }
  int NumNodes() const { return static_cast<int>(adj_.size()); }

  void SetNeighbors(int node, std::vector<int> nbrs) { adj_.at(node) = std::move(nbrs); }
  const std::vector<int>& Neighbors(int node) const { return adj_.at(node); }

  std::size_t EdgeCount() const {
    std::size_t sum = 0;
    for (const auto& n : adj_) sum += n.size();
    return sum;
  }

 private:
  std::vector<std::vector<int>> adj_;
};
}  // namespace pipnn
