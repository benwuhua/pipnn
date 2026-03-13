#pragma once

#include "candidates/pipnn_candidate_generator.h"
#include "common/types.h"
#include "core/graph.h"

namespace pipnn {
struct VamanaRefineParams {
  int max_degree = 64;
};

Graph RefineVamanaGraph(const Matrix& points, const CandidateAdjacency& candidates,
                        const VamanaRefineParams& params);
}  // namespace pipnn
