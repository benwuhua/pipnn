#pragma once

#include "common/types.h"
#include "core/graph.h"

#include <vector>

namespace pipnn {
struct VamanaSearchParams {
  int beam = 64;
  int topk = 10;
  int entry = 0;
};

std::vector<int> SearchVamanaGraph(const Matrix& points, const Graph& graph, const Vec& query,
                                   const VamanaSearchParams& params);
}  // namespace pipnn
