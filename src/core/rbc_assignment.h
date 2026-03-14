#pragma once

#include "common/types.h"
#include "core/distance.h"

#include <vector>

namespace pipnn {
enum class RbcAssignMode { Scalar, Blocked };

struct RbcAssignConfig {
  int min_points_for_blocked = 256;
  int min_leaders_for_blocked = 8;
  int point_block_rows = 256;
};

RbcAssignMode SelectRbcAssignMode(std::size_t point_count, std::size_t leader_count,
                                  const RbcAssignConfig& cfg = {});

std::vector<int> AssignPointsToLeaders(const Matrix& points, const std::vector<int>& ids,
                                       const std::vector<int>& leaders, RbcAssignMode mode,
                                       MetricKind metric = MetricKind::L2,
                                       const RbcAssignConfig& cfg = {});
}  // namespace pipnn
