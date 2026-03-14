#include "core/leaf_candidates.h"

#include "core/distance.h"

#include <algorithm>

namespace pipnn {
std::vector<int> BuildShortlistForPoint(int point_id, const Leaves& leaves,
                                        const std::vector<std::vector<int>>& point_memberships,
                                        const ShortlistConfig& cfg) {
  if (point_id < 0 || static_cast<std::size_t>(point_id) >= point_memberships.size()) return {};
  std::vector<int> shortlist;
  std::vector<unsigned char> seen(point_memberships.size(), 0);
  const auto& memberships = point_memberships[static_cast<std::size_t>(point_id)];
  for (int leaf_index : memberships) {
    if (leaf_index < 0 || static_cast<std::size_t>(leaf_index) >= leaves.size()) continue;
    const auto& leaf = leaves[static_cast<std::size_t>(leaf_index)];
    for (int candidate : leaf) {
      if (candidate < 0 || static_cast<std::size_t>(candidate) >= seen.size()) continue;
      if (candidate == point_id || seen[static_cast<std::size_t>(candidate)] != 0) continue;
      seen[static_cast<std::size_t>(candidate)] = 1;
      shortlist.push_back(candidate);
    }
  }
  if (cfg.candidate_cap > 0 && static_cast<int>(shortlist.size()) > cfg.candidate_cap) {
    shortlist.resize(static_cast<std::size_t>(cfg.candidate_cap));
  }
  return shortlist;
}

std::vector<Edge> ScoreShortlistExact(const Matrix& points, int point_id,
                                      const std::vector<int>& shortlist, int k,
                                      bool bidirected, MetricKind metric) {
  if (k <= 0 || point_id < 0 || static_cast<std::size_t>(point_id) >= points.size()) return {};
  std::vector<std::pair<float, int>> dists;
  dists.reserve(shortlist.size());
  for (int candidate : shortlist) {
    if (candidate < 0 || static_cast<std::size_t>(candidate) >= points.size()) continue;
    if (candidate == point_id) continue;
    dists.push_back({MetricScore(points[static_cast<std::size_t>(point_id)],
                                 points[static_cast<std::size_t>(candidate)], metric),
                     candidate});
  }
  const int kk = std::min(k, static_cast<int>(dists.size()));
  std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
  std::vector<Edge> edges;
  edges.reserve(static_cast<std::size_t>(kk) * (bidirected ? 2u : 1u));
  for (int i = 0; i < kk; ++i) {
    const int v = dists[static_cast<std::size_t>(i)].second;
    edges.push_back({point_id, v});
    if (bidirected) edges.push_back({v, point_id});
  }
  return edges;
}
}  // namespace pipnn
