#pragma once

#include "common/types.h"

#include <vector>

namespace pipnn {
Metrics RunHnswBaseline(const Matrix& base, const Matrix& queries,
                        const std::vector<std::vector<int>>& truth, int topk);
}  // namespace pipnn
