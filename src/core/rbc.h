#pragma once

#include "common/types.h"

#include <vector>

namespace pipnn {
struct RbcParams {
  int cmax = 4096;
  int cmin = 32;
  float leader_frac = 0.02f;
  int fanout = 2;
  int max_leaders = 1000;
  int seed = 17;
};

using Leaves = std::vector<std::vector<int>>;

struct RbcResult {
  Leaves leaves;
  std::vector<std::vector<int>> point_memberships;
};

struct RbcStats {
  std::size_t leaf_count = 0;
  std::size_t assignment_total = 0;
  std::size_t points_with_overlap = 0;
  std::size_t max_membership = 0;
  std::size_t min_leaf_size = 0;
  std::size_t max_leaf_size = 0;
  std::size_t fallback_chunk_splits = 0;
};

RbcResult BuildRbc(const Matrix& points, const RbcParams& params, RbcStats* stats = nullptr);
Leaves BuildRbcLeaves(const Matrix& points, const RbcParams& params, RbcStats* stats = nullptr);
}  // namespace pipnn
