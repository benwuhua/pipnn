#pragma once

#include "core/leaf_knn.h"

namespace pipnn {
enum class LeafKnnMode { NaiveFull, BlockedFull };

struct LeafKnnConfig {
  int min_leaf_for_blocked = 128;
  int point_block_rows = 128;
};

LeafKnnMode SelectLeafKnnMode(std::size_t leaf_size, int scan_cap, const LeafKnnConfig& cfg = {});

std::vector<Edge> BuildLeafKnnExactEdges(const Matrix& points, const std::vector<int>& leaf, int k,
                                         bool bidirected, LeafKnnMode mode,
                                         const LeafKnnConfig& cfg = {});
}  // namespace pipnn
