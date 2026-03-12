#include "search/greedy_beam.h"

#include "core/distance.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace pipnn {
std::vector<int> SearchGraph(const Matrix& points, const Graph& graph, const Vec& query,
                             const SearchParams& params) {
  if (points.empty()) return {};
  auto qdist = [&](int id) { return std::make_pair(L2Squared(points[id], query), id); };

  using Cand = std::pair<float, int>;
  std::priority_queue<Cand, std::vector<Cand>, std::greater<Cand>> pq;
  std::unordered_set<int> discovered;
  discovered.insert(params.entry);
  pq.push(qdist(params.entry));

  std::vector<Cand> expanded;
  expanded.reserve(params.beam);

  while (!pq.empty() && static_cast<int>(expanded.size()) < params.beam) {
    auto [cd, cur] = pq.top();
    pq.pop();
    expanded.push_back({cd, cur});

    for (int nxt : graph.Neighbors(cur)) {
      if (!discovered.insert(nxt).second) continue;
      pq.push(qdist(nxt));
    }
  }

  std::sort(expanded.begin(), expanded.end());
  std::vector<int> out;
  int k = std::min(params.topk, static_cast<int>(expanded.size()));
  out.reserve(k);
  for (int i = 0; i < k; ++i) out.push_back(expanded[i].second);
  return out;
}
}  // namespace pipnn
