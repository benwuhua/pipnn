#include "candidates/pipnn_candidate_generator.h"
#include "core/pipnn_builder.h"

#include "common/timer.h"

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
  PipnnCandidateParams candidate_params;
  candidate_params.rbc = params.rbc;
  candidate_params.replicas = params.replicas;
  candidate_params.leaf_k = params.leaf_k;
  candidate_params.bidirected = params.bidirected;
  candidate_params.candidate_cap = std::max(1, params.hashprune.max_degree * 8);

  PipnnCandidateStats candidate_stats;
  auto candidates = BuildPipnnCandidates(points, candidate_params, stats != nullptr ? &candidate_stats : nullptr);
  if (stats != nullptr) {
    stats->partition_sec = candidate_stats.partition_sec;
    stats->leaf_knn_sec = candidate_stats.score_sec;
    stats->num_leaves = candidate_stats.num_leaves;
    stats->candidate_edges = candidate_stats.candidate_edges;
    stats->rbc_assignment_total = candidate_stats.rbc_assignment_total;
    stats->rbc_points_with_overlap = candidate_stats.rbc_points_with_overlap;
    stats->rbc_max_membership = candidate_stats.rbc_max_membership;
    stats->rbc_min_leaf_size = candidate_stats.rbc_min_leaf_size;
    stats->rbc_max_leaf_size = candidate_stats.rbc_max_leaf_size;
    stats->rbc_fallback_chunk_splits = candidate_stats.rbc_fallback_chunk_splits;
  }

  Timer t_prune;
  HashPruner pruner(params.hashprune);
  const int n = static_cast<int>(points.size());
#if defined(_OPENMP)
#pragma omp parallel for schedule(dynamic, 256)
#endif
  for (int i = 0; i < n; ++i) {
    auto& c = candidates[i];
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
