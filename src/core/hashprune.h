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

struct HashPruneNodeStats {
  std::size_t self_skipped = 0;
  std::size_t kept = 0;
  std::size_t replaced = 0;
  std::size_t evicted = 0;
  std::size_t dropped = 0;
  std::size_t final_degree = 0;
};

class HashPruner {
 public:
  explicit HashPruner(HashPruneParams params);
  HashPruner(HashPruneParams params, Matrix manual_hyperplanes);

  std::vector<int> PruneNode(const Matrix& points, int p, const std::vector<int>& candidates,
                             HashPruneNodeStats* stats = nullptr) const;
  std::uint64_t HashResidualForTest(const Vec& p, const Vec& c) const;

 private:
  void EnsureHyperplanes(std::size_t dim) const;
  std::uint64_t HashResidual(const Vec& p, const Vec& c) const;

  HashPruneParams params_;
  mutable std::once_flag init_once_;
  mutable Matrix hyperplanes_;
  bool has_manual_hyperplanes_ = false;
};
}  // namespace pipnn
