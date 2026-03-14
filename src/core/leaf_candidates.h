#pragma once

#include "common/types.h"
#include "core/distance.h"
#include "core/leaf_knn.h"
#include "core/rbc.h"

#include <vector>

namespace pipnn {
struct ShortlistConfig {
  int candidate_cap = 0;
};

std::vector<int> BuildShortlistForPoint(int point_id, const Leaves& leaves,
                                        const std::vector<std::vector<int>>& point_memberships,
                                        const ShortlistConfig& cfg = {});

std::vector<Edge> ScoreShortlistExact(const Matrix& points, int point_id,
                                      const std::vector<int>& shortlist, int k,
                                      bool bidirected,
                                      MetricKind metric = MetricKind::L2);
}  // namespace pipnn
