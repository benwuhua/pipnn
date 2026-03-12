#include "core/pipnn_builder.h"

#include "common/timer.h"
#include "core/leaf_knn.h"

#include <cstdlib>
#include <iostream>
#if defined(_OPENMP)
#include <omp.h>
#endif

#include <algorithm>
#include <unordered_set>

namespace pipnn {
Graph BuildPipnnGraph(const Matrix& points, const PipnnBuildParams& params, PipnnBuildStats* stats) {
  Graph g(static_cast<int>(points.size()));
  std::vector<std::vector<int>> candidates(points.size());
  const int reps = std::max(1, params.replicas);
  for (int rep = 0; rep < reps; ++rep) {
    Timer t_partition;
    auto rbc_params = params.rbc;
    rbc_params.seed = params.rbc.seed + rep;
    RbcStats rbc_stats;
    auto leaves = BuildRbcLeaves(points, rbc_params, stats != nullptr ? &rbc_stats : nullptr);
    if (stats != nullptr) {
      stats->partition_sec += t_partition.Sec();
      stats->num_leaves += leaves.size();
      stats->rbc_assignment_total += rbc_stats.assignment_total;
      stats->rbc_points_with_overlap += rbc_stats.points_with_overlap;
      stats->rbc_max_membership = std::max(stats->rbc_max_membership, rbc_stats.max_membership);
      stats->rbc_max_leaf_size = std::max(stats->rbc_max_leaf_size, rbc_stats.max_leaf_size);
      if (rbc_stats.min_leaf_size > 0) {
        stats->rbc_min_leaf_size = stats->rbc_min_leaf_size == 0
                                       ? rbc_stats.min_leaf_size
                                       : std::min(stats->rbc_min_leaf_size, rbc_stats.min_leaf_size);
      }
      stats->rbc_fallback_chunk_splits += rbc_stats.fallback_chunk_splits;
    }

    Timer t_leaf;
    for (const auto& leaf : leaves) {
      auto edges = BuildLeafKnnEdges(points, leaf, params.leaf_k, params.bidirected, params.leaf_scan_cap);
      if (stats != nullptr) stats->candidate_edges += edges.size();
      for (const auto& [u, v] : edges) candidates[u].push_back(v);
    }
    if (stats != nullptr) stats->leaf_knn_sec += t_leaf.Sec();
  }

  Timer t_prune;
  HashPruner pruner(params.hashprune);
  const int n = static_cast<int>(points.size());
#if defined(_OPENMP)
#pragma omp parallel for schedule(dynamic, 256)
#endif
  for (int i = 0; i < n; ++i) {
    auto& c = candidates[i];
    std::sort(c.begin(), c.end());
    c.erase(std::unique(c.begin(), c.end()), c.end());
    HashPruneNodeStats node_stats;
    auto pruned = pruner.PruneNode(points, i, c, stats != nullptr ? &node_stats : nullptr);
    g.SetNeighbors(i, pruned);
    if (stats != nullptr) {
#if defined(_OPENMP)
#pragma omp critical
#endif
      {
        stats->prune_kept += node_stats.kept;
        stats->prune_dropped += node_stats.dropped;
        stats->prune_replaced += node_stats.replaced;
        stats->prune_evicted += node_stats.evicted;
        stats->prune_final_edges += pruned.size();
      }
    }
  }
  if (stats != nullptr) stats->prune_sec = t_prune.Sec();
  if (std::getenv("PIPNN_PROFILE") != nullptr && stats != nullptr) {
    std::cout << "pipnn_profile_build partition_sec=" << stats->partition_sec
              << " leaf_knn_sec=" << stats->leaf_knn_sec << " prune_sec=" << stats->prune_sec
              << " leaves=" << stats->num_leaves << " candidate_edges=" << stats->candidate_edges
              << " rbc_assignment_total=" << stats->rbc_assignment_total
              << " rbc_points_with_overlap=" << stats->rbc_points_with_overlap
              << " rbc_max_membership=" << stats->rbc_max_membership
              << " rbc_min_leaf_size=" << stats->rbc_min_leaf_size
              << " rbc_max_leaf_size=" << stats->rbc_max_leaf_size
              << " rbc_fallback_chunk_splits=" << stats->rbc_fallback_chunk_splits
              << " prune_kept=" << stats->prune_kept << " prune_dropped=" << stats->prune_dropped
              << " prune_replaced=" << stats->prune_replaced
              << " prune_evicted=" << stats->prune_evicted
              << " prune_final_edges=" << stats->prune_final_edges
              << "\n";
  }

  return g;
}
}  // namespace pipnn
