#pragma once

#include "common/types.h"
#include "core/distance.h"

#include <vector>

namespace pipnn {
struct HnswParams {
  int m = 32;
  int ef_construction = 200;
  int ef_search = 0;  // 0 uses the runner default derived from topk.
};

Metrics RunHnswBaseline(const Matrix& base, const Matrix& queries,
                        const std::vector<std::vector<int>>& truth, int topk,
                        const HnswParams& params = {},
                        MetricKind metric = MetricKind::L2);
}  // namespace pipnn
