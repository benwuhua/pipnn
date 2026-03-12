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
    auto leaves = BuildRbcLeaves(points, rbc_params);
    if (stats != nullptr) {
      stats->partition_sec += t_partition.Sec();
      stats->num_leaves += leaves.size();
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
    g.SetNeighbors(i, pruner.PruneNode(points, i, c));
  }
  if (stats != nullptr) stats->prune_sec = t_prune.Sec();
  if (std::getenv("PIPNN_PROFILE") != nullptr && stats != nullptr) {
    std::cout << "pipnn_profile_build partition_sec=" << stats->partition_sec
              << " leaf_knn_sec=" << stats->leaf_knn_sec << " prune_sec=" << stats->prune_sec
              << " leaves=" << stats->num_leaves << " candidate_edges=" << stats->candidate_edges
              << "\n";
  }

  return g;
}
}  // namespace pipnn
