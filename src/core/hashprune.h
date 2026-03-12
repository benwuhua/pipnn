#pragma once

#include "common/types.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace pipnn {
struct HashPruneParams {
  int hash_bits = 12;
  int max_degree = 64;
  int seed = 7;
};

class HashPruner {
 public:
  explicit HashPruner(HashPruneParams params);

  std::vector<int> PruneNode(const Matrix& points, int p, const std::vector<int>& candidates) const;

 private:
  void EnsureHyperplanes(std::size_t dim) const;
  std::uint64_t HashResidual(const Vec& p, const Vec& c) const;

  HashPruneParams params_;
  mutable std::once_flag init_once_;
  mutable Matrix hyperplanes_;
};
}  // namespace pipnn
