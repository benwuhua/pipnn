#include "candidates/pipnn_candidate_generator.h"

#include "common/timer.h"
#include "core/leaf_candidates.h"

#include <algorithm>

namespace pipnn {
CandidateAdjacency BuildPipnnCandidates(const Matrix& points, const PipnnCandidateParams& params,
                                        PipnnCandidateStats* stats) {
  CandidateAdjacency candidates(points.size());
  const int reps = std::max(1, params.replicas);
  for (int rep = 0; rep < reps; ++rep) {
    Timer t_partition;
    auto rbc_params = params.rbc;
    rbc_params.seed = params.rbc.seed + rep;
    RbcStats rbc_stats;
    auto rbc = BuildRbc(points, rbc_params, stats != nullptr ? &rbc_stats : nullptr);
    if (stats != nullptr) {
      stats->partition_sec += t_partition.Sec();
      stats->num_leaves += rbc.leaves.size();
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

    Timer t_score;
    ShortlistConfig shortlist_cfg;
    shortlist_cfg.candidate_cap = params.candidate_cap;
    for (int point_id = 0; point_id < static_cast<int>(points.size()); ++point_id) {
      auto shortlist =
          BuildShortlistForPoint(point_id, rbc.leaves, rbc.point_memberships, shortlist_cfg);
      auto edges = ScoreShortlistExact(points, point_id, shortlist, params.leaf_k, params.bidirected);
      if (stats != nullptr) stats->candidate_edges += edges.size();
      for (const auto& [u, v] : edges) candidates[u].push_back(v);
    }
    if (stats != nullptr) stats->score_sec += t_score.Sec();
  }

  for (auto& row : candidates) {
    std::sort(row.begin(), row.end());
    row.erase(std::unique(row.begin(), row.end()), row.end());
  }
  return candidates;
}
}  // namespace pipnn
