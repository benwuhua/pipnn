#pragma once

#include "core/leaf_knn.h"

namespace pipnn {
struct LeafBatchJob {
  std::vector<int> leaf;
};

struct LeafBatchConfig {
  int min_leaf_for_batch = 64;
  int point_block_rows = 128;
  int max_points_per_batch = 1024;
};

struct LeafBatchPlan {
  std::vector<int> job_indices;
  int total_points = 0;
};

enum class LeafKnnMode { NaiveFull, BlockedFull };

struct LeafKnnConfig {
  int min_leaf_for_blocked = 128;
  int point_block_rows = 128;
};

std::vector<Edge> BuildLeafKnnExactEdgesNaive(const Matrix& points, const std::vector<int>& leaf, int k,
                                              bool bidirected);

std::vector<LeafBatchPlan> PlanLeafKnnBatches(const std::vector<LeafBatchJob>& jobs,
                                              const LeafBatchConfig& cfg = {});

std::vector<Edge> BuildLeafKnnExactBatchedEdges(const Matrix& points, const std::vector<LeafBatchJob>& jobs,
                                                int k, bool bidirected,
                                                const LeafBatchConfig& cfg = {});

LeafKnnMode SelectLeafKnnMode(std::size_t leaf_size, int scan_cap, const LeafKnnConfig& cfg = {});

std::vector<Edge> BuildLeafKnnExactEdges(const Matrix& points, const std::vector<int>& leaf, int k,
                                         bool bidirected, LeafKnnMode mode,
                                         const LeafKnnConfig& cfg = {});
}  // namespace pipnn
