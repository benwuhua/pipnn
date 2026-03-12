#pragma once

#include "common/types.h"

#include <vector>

namespace pipnn {
using Edge = std::pair<int, int>;

std::vector<Edge> BuildLeafKnnEdges(const Matrix& points, const std::vector<int>& leaf, int k,
                                    bool bidirected, int scan_cap = 0);
}  // namespace pipnn
