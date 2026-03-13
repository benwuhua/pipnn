#include "candidates/pipnn_candidate_generator.h"

#include <cassert>
#include <algorithm>

int main() {
  pipnn::Matrix points = {
      {0.0f, 0.0f},
      {1.0f, 0.0f},
      {0.0f, 1.0f},
      {1.0f, 1.0f},
  };

  pipnn::PipnnCandidateParams params;
  params.rbc.cmax = 2;
  params.rbc.fanout = 1;
  params.leaf_k = 2;

  pipnn::PipnnCandidateStats stats;
  auto candidates = pipnn::BuildPipnnCandidates(points, params, &stats);

  assert(candidates.size() == points.size());
  assert(stats.num_leaves > 0);
  assert(stats.candidate_edges > 0);

  for (int u = 0; u < static_cast<int>(candidates.size()); ++u) {
    for (int v : candidates[static_cast<std::size_t>(u)]) {
      const auto& reverse = candidates[static_cast<std::size_t>(v)];
      assert(std::find(reverse.begin(), reverse.end(), u) != reverse.end());
    }
  }

  params.bidirected = false;
  pipnn::Matrix directed_points = {
      {0.0f},
      {10.0f},
      {11.0f},
  };
  params.rbc.cmax = 3;
  params.rbc.fanout = 1;
  params.leaf_k = 1;
  pipnn::PipnnCandidateStats directed_stats;
  auto directed = pipnn::BuildPipnnCandidates(directed_points, params, &directed_stats);
  assert(directed_stats.candidate_edges > 0);
  bool has_one_way_edge = false;
  for (int u = 0; u < static_cast<int>(directed.size()); ++u) {
    for (int v : directed[static_cast<std::size_t>(u)]) {
      const auto& reverse = directed[static_cast<std::size_t>(v)];
      if (std::find(reverse.begin(), reverse.end(), u) == reverse.end()) {
        has_one_way_edge = true;
      }
    }
  }
  assert(has_one_way_edge);
  return 0;
}
