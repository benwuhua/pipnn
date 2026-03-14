#include "search/vamana_search_adapter.h"

#include "search/greedy_beam.h"

namespace pipnn {
std::vector<int> SearchVamanaGraph(const Matrix& points, const Graph& graph, const Vec& query,
                                   const VamanaSearchParams& params) {
  SearchParams legacy;
  legacy.beam = params.beam;
  legacy.topk = params.topk;
  legacy.entry = params.entry;
  legacy.metric = params.metric;
  return SearchGraph(points, graph, query, legacy);
}
}  // namespace pipnn
