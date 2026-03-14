#pragma once

#include "common/types.h"
#include "core/distance.h"
#include "core/rbc.h"

#include <vector>

namespace pipnn {
using CandidateAdjacency = std::vector<std::vector<int>>;

struct PipnnCandidateStats {
  double partition_sec = 0.0;
  double score_sec = 0.0;
  std::size_t num_leaves = 0;
  std::size_t candidate_edges = 0;
  std::size_t rbc_assignment_total = 0;
  std::size_t rbc_points_with_overlap = 0;
  std::size_t rbc_max_membership = 0;
  std::size_t rbc_min_leaf_size = 0;
  std::size_t rbc_max_leaf_size = 0;
  std::size_t rbc_fallback_chunk_splits = 0;
};

struct PipnnCandidateParams {
  RbcParams rbc;
  int replicas = 1;
  int leaf_k = 24;
  bool bidirected = true;
  int candidate_cap = 0;
  MetricKind metric = MetricKind::L2;
};

CandidateAdjacency BuildPipnnCandidates(const Matrix& points, const PipnnCandidateParams& params,
                                        PipnnCandidateStats* stats = nullptr);
}  // namespace pipnn
