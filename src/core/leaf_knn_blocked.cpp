#include "core/leaf_knn_blocked.h"

#include "core/distance.h"

#include <algorithm>

namespace pipnn {
namespace {
std::vector<Edge> BuildLeafKnnExactEdgesNaive(const Matrix& points, const std::vector<int>& leaf, int k,
                                              bool bidirected) {
  std::vector<Edge> edges;
  const std::size_t approx = static_cast<std::size_t>(leaf.size()) *
                             static_cast<std::size_t>(std::max(1, k)) *
                             (bidirected ? 2 : 1);
  edges.reserve(approx);

  for (int idx = 0; idx < static_cast<int>(leaf.size()); ++idx) {
    const int u = leaf[static_cast<std::size_t>(idx)];
    std::vector<std::pair<float, int>> dists;
    dists.reserve(leaf.size() > 0 ? leaf.size() - 1 : 0);
    for (int v : leaf) {
      if (u == v) continue;
      dists.push_back({L2Squared(points[static_cast<std::size_t>(u)],
                                 points[static_cast<std::size_t>(v)]),
                       v});
    }
    const int kk = std::min(k, static_cast<int>(dists.size()));
    std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
    for (int i = 0; i < kk; ++i) {
      const int v = dists[static_cast<std::size_t>(i)].second;
      edges.push_back({u, v});
      if (bidirected) edges.push_back({v, u});
    }
  }
  return edges;
}
}  // namespace

std::vector<Edge> BuildLeafKnnExactEdges(const Matrix& points, const std::vector<int>& leaf, int k,
                                         bool bidirected, LeafKnnMode mode,
                                         const LeafKnnConfig& cfg) {
  (void)cfg;
  if (leaf.empty() || k <= 0) return {};
  switch (mode) {
    case LeafKnnMode::NaiveFull:
      return BuildLeafKnnExactEdgesNaive(points, leaf, k, bidirected);
    case LeafKnnMode::BlockedFull:
      return BuildLeafKnnExactEdgesNaive(points, leaf, k, bidirected);
  }
  return {};
}
}  // namespace pipnn
