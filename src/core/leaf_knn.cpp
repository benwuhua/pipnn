#include "core/leaf_knn.h"

#include "core/leaf_knn_blocked.h"
#include "core/distance.h"

#include <algorithm>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace pipnn {
namespace {
std::vector<Edge> BuildLeafKnnEdgesCapped(const Matrix& points, const std::vector<int>& leaf, int k,
                                          bool bidirected, int scan_cap) {
  std::vector<Edge> edges;
  const std::size_t approx = static_cast<std::size_t>(leaf.size()) *
                             static_cast<std::size_t>(std::max(1, k)) *
                             (bidirected ? 2 : 1);
  edges.reserve(approx);

#if defined(_OPENMP)
  const int threads = std::max(1, omp_get_max_threads());
  std::vector<std::vector<Edge>> local(static_cast<std::size_t>(threads));
#pragma omp parallel
  {
    const int tid = omp_get_thread_num();
    auto& out = local[static_cast<std::size_t>(tid)];
    out.reserve(approx / static_cast<std::size_t>(threads) + 64);
#pragma omp for schedule(static)
    for (int idx = 0; idx < static_cast<int>(leaf.size()); ++idx) {
      const int u = leaf[static_cast<std::size_t>(idx)];
      std::vector<std::pair<float, int>> dists;
      const int m = static_cast<int>(leaf.size());
      const int start = (idx * 2654435761u) % static_cast<unsigned>(m);
      int picked = 0;
      dists.reserve(static_cast<std::size_t>(std::min(scan_cap, m > 0 ? m - 1 : 0)));
      for (int off = 0; off < m && picked < scan_cap; ++off) {
        const int v = leaf[static_cast<std::size_t>((start + off) % m)];
        if (u == v) continue;
        dists.push_back({L2Squared(points[static_cast<std::size_t>(u)],
                                   points[static_cast<std::size_t>(v)]),
                         v});
        ++picked;
      }
      const int kk = std::min(k, static_cast<int>(dists.size()));
      std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
      for (int i = 0; i < kk; ++i) {
        const int v = dists[static_cast<std::size_t>(i)].second;
        out.push_back({u, v});
        if (bidirected) out.push_back({v, u});
      }
    }
  }
  for (auto& v : local) edges.insert(edges.end(), v.begin(), v.end());
#else
  for (int idx = 0; idx < static_cast<int>(leaf.size()); ++idx) {
    const int u = leaf[static_cast<std::size_t>(idx)];
    std::vector<std::pair<float, int>> dists;
    const int m = static_cast<int>(leaf.size());
    const int start = (idx * 2654435761u) % static_cast<unsigned>(m);
    int picked = 0;
    dists.reserve(static_cast<std::size_t>(std::min(scan_cap, m > 0 ? m - 1 : 0)));
    for (int off = 0; off < m && picked < scan_cap; ++off) {
      const int v = leaf[static_cast<std::size_t>((start + off) % m)];
      if (u == v) continue;
      dists.push_back({L2Squared(points[static_cast<std::size_t>(u)],
                                 points[static_cast<std::size_t>(v)]),
                       v});
      ++picked;
    }
    const int kk = std::min(k, static_cast<int>(dists.size()));
    std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
    for (int i = 0; i < kk; ++i) {
      const int v = dists[static_cast<std::size_t>(i)].second;
      edges.push_back({u, v});
      if (bidirected) edges.push_back({v, u});
    }
  }
#endif
  return edges;
}
}  // namespace

std::vector<Edge> BuildLeafKnnEdges(const Matrix& points, const std::vector<int>& leaf, int k,
                                    bool bidirected, int scan_cap) {
  if (leaf.empty() || k <= 0) return {};
  const int full = static_cast<int>(leaf.size()) - 1;
  const bool capped = scan_cap > 0 && scan_cap < full;
  if (capped) return BuildLeafKnnEdgesCapped(points, leaf, k, bidirected, scan_cap);
  return BuildLeafKnnExactEdgesNaive(points, leaf, k, bidirected);
}
}  // namespace pipnn
