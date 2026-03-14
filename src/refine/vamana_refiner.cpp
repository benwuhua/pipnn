#include "refine/vamana_refiner.h"

#include "core/distance.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace pipnn {
Graph RefineVamanaGraph(const Matrix& points, const CandidateAdjacency& candidates,
                        const VamanaRefineParams& params) {
  Graph graph(static_cast<int>(points.size()));
  const int max_degree = std::max(0, params.max_degree);
  for (int i = 0; i < static_cast<int>(points.size()); ++i) {
    std::vector<std::pair<float, int>> scored;
    scored.reserve(candidates[i].size());
    for (int neighbor : candidates[i]) {
      if (neighbor < 0 || neighbor >= static_cast<int>(points.size()) || neighbor == i) continue;
      scored.push_back({MetricScore(points[i], points[neighbor], params.metric), neighbor});
    }
    std::sort(scored.begin(), scored.end());
    const int keep = std::min(max_degree, static_cast<int>(scored.size()));
    std::vector<int> neighbors;
    neighbors.reserve(keep);
    for (int j = 0; j < keep; ++j) neighbors.push_back(scored[j].second);
    graph.SetNeighbors(i, std::move(neighbors));
  }
  return graph;
}
}  // namespace pipnn
