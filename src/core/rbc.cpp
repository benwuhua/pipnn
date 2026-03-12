#include "core/rbc.h"

#include "core/distance.h"

#include <algorithm>
#include <numeric>
#include <random>

namespace pipnn {
namespace {
void ChunkSplit(const std::vector<int>& ids, int cmax, Leaves& out) {
  for (int i = 0; i < static_cast<int>(ids.size()); i += cmax) {
    int e = std::min(i + cmax, static_cast<int>(ids.size()));
    out.emplace_back(ids.begin() + i, ids.begin() + e);
  }
}

void PartitionRec(const Matrix& points, const RbcParams& params, std::mt19937& gen,
                  const std::vector<int>& ids, Leaves& out, int depth) {
  (void)depth;
  if (static_cast<int>(ids.size()) <= params.cmax) {
    out.push_back(ids);
    return;
  }

  int leader_count = std::max(2, static_cast<int>(ids.size() * params.leader_frac));
  leader_count = std::min(leader_count, params.max_leaders);
  leader_count = std::min(leader_count, static_cast<int>(ids.size()));

  std::vector<int> shuffled = ids;
  std::shuffle(shuffled.begin(), shuffled.end(), gen);
  std::vector<int> leaders(shuffled.begin(), shuffled.begin() + leader_count);

  std::vector<std::vector<int>> buckets(leader_count);
  int f = std::min(params.fanout, leader_count);

  for (int x : ids) {
    std::vector<std::pair<float, int>> near;
    near.reserve(leader_count);
    for (int i = 0; i < leader_count; ++i) {
      near.push_back({L2Squared(points[x], points[leaders[i]]), i});
    }
    std::partial_sort(near.begin(), near.begin() + f, near.end());
    for (int i = 0; i < f; ++i) buckets[near[i].second].push_back(x);
  }

  for (auto& b : buckets) {
    if (b.empty()) continue;
    if (static_cast<int>(b.size()) == static_cast<int>(ids.size())) {
      // prevent endless recursion while preserving leaf bound.
      ChunkSplit(b, params.cmax, out);
      continue;
    }
    if (static_cast<int>(b.size()) <= params.cmax) {
      out.push_back(b);
    } else {
      PartitionRec(points, params, gen, b, out, depth + 1);
    }
  }
}
}  // namespace

Leaves BuildRbcLeaves(const Matrix& points, const RbcParams& params) {
  std::vector<int> ids(points.size());
  std::iota(ids.begin(), ids.end(), 0);
  std::mt19937 gen(params.seed);
  Leaves out;
  PartitionRec(points, params, gen, ids, out, 0);
  return out;
}
}  // namespace pipnn
