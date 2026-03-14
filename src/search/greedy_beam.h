#pragma once

#include "common/types.h"
#include "core/distance.h"
#include "core/graph.h"

#include <vector>

namespace pipnn {
struct SearchParams {
  int beam = 64;
  int topk = 10;
  int entry = 0;
  MetricKind metric = MetricKind::L2;
};

std::vector<int> SearchGraph(const Matrix& points, const Graph& graph, const Vec& query,
                             const SearchParams& params);
}  // namespace pipnn
