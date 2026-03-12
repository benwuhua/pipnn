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

Leaves BuildRbcLeaves(const Matrix& points, const RbcParams& params);
}  // namespace pipnn
