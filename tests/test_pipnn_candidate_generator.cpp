#include "candidates/pipnn_candidate_generator.h"

#include <cassert>

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
  return 0;
}
