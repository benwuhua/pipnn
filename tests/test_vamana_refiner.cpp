#include "candidates/pipnn_candidate_generator.h"
#include "refine/vamana_refiner.h"

#include <cassert>

int main() {
  pipnn::Matrix points = {{0.0f}, {1.0f}, {2.0f}, {3.0f}};
  pipnn::CandidateAdjacency candidates = {
      {1, 2, 3},
      {0, 2, 3},
      {1, 3, 0},
      {2, 1, 0},
  };

  pipnn::VamanaRefineParams params;
  params.max_degree = 2;

  auto graph = pipnn::RefineVamanaGraph(points, candidates, params);
  assert(graph.NumNodes() == 4);
  for (int i = 0; i < graph.NumNodes(); ++i) {
    assert(graph.Neighbors(i).size() <= 2);
  }
  return 0;
}
