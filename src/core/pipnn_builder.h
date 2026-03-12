#pragma once

#include "common/types.h"
#include "core/graph.h"
#include "core/hashprune.h"
#include "core/rbc.h"

namespace pipnn {
struct PipnnBuildStats {
  double partition_sec = 0.0;
  double leaf_knn_sec = 0.0;
  double prune_sec = 0.0;
  std::size_t num_leaves = 0;
  std::size_t candidate_edges = 0;
};

struct PipnnBuildParams {
  RbcParams rbc;
  HashPruneParams hashprune;
  int replicas = 1;
  int leaf_k = 24;
  int leaf_scan_cap = 0;
  bool bidirected = true;
};

Graph BuildPipnnGraph(const Matrix& points, const PipnnBuildParams& params,
                      PipnnBuildStats* stats = nullptr);
}  // namespace pipnn
