#include "core/rbc.h"

#include "core/rbc_assignment.h"
#include "core/distance.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <random>

namespace pipnn {
namespace {
struct LeafBucket {
  int representative = -1;
  std::vector<int> ids;
};

using LeafBuckets = std::vector<LeafBucket>;

struct RbcNode {
  int representative = -1;
  int leaf_index = -1;
  std::vector<int> ids;
  std::vector<RbcNode> children;

  bool IsLeaf() const { return children.empty(); }
};

RbcNode MakeLeaf(const std::vector<int>& ids, int representative) {
  RbcNode node;
  node.representative = representative >= 0 ? representative : ids.front();
  node.ids = ids;
  return node;
}

void ChunkSplit(const std::vector<int>& ids, int cmax, std::vector<RbcNode>& out,
                std::size_t* fallback_chunk_splits) {
  for (int i = 0; i < static_cast<int>(ids.size()); i += cmax) {
    int e = std::min(i + cmax, static_cast<int>(ids.size()));
    out.push_back(MakeLeaf(std::vector<int>(ids.begin() + i, ids.begin() + e), ids[i]));
    if (fallback_chunk_splits != nullptr) ++(*fallback_chunk_splits);
  }
}

RbcNode BuildNode(const Matrix& points, const RbcParams& params, std::mt19937& gen,
                  const std::vector<int>& ids, int representative,
                  std::size_t* fallback_chunk_splits) {
  if (static_cast<int>(ids.size()) <= params.cmax) {
    return MakeLeaf(ids, representative);
  }

  RbcNode node;
  node.representative = representative >= 0 ? representative : ids.front();
  int leader_count = std::max(2, static_cast<int>(ids.size() * params.leader_frac));
  leader_count = std::min(leader_count, params.max_leaders);
  leader_count = std::min(leader_count, static_cast<int>(ids.size()));

  std::vector<int> shuffled = ids;
  std::shuffle(shuffled.begin(), shuffled.end(), gen);
  std::vector<int> leaders(shuffled.begin(), shuffled.begin() + leader_count);

  std::vector<std::vector<int>> buckets(leader_count);
  const RbcAssignConfig assign_cfg;
  const auto assign_mode = SelectRbcAssignMode(ids.size(), leaders.size(), assign_cfg);
  const auto assignments =
      AssignPointsToLeaders(points, ids, leaders, assign_mode, params.metric, assign_cfg);

  for (std::size_t i = 0; i < ids.size(); ++i) {
    const int best_index = assignments[i];
    buckets[static_cast<std::size_t>(best_index)].push_back(ids[i]);
  }

  for (int i = 0; i < leader_count; ++i) {
    auto& b = buckets[i];
    if (b.empty()) continue;
    if (static_cast<int>(b.size()) == static_cast<int>(ids.size())) {
      ChunkSplit(b, params.cmax, node.children, fallback_chunk_splits);
      continue;
    }
    if (static_cast<int>(b.size()) <= params.cmax) {
      node.children.push_back(MakeLeaf(b, leaders[i]));
    } else {
      node.children.push_back(BuildNode(points, params, gen, b, leaders[i], fallback_chunk_splits));
    }
  }
  if (node.children.empty()) return MakeLeaf(ids, representative);
  return node;
}

void CollectLeaves(RbcNode& node, LeafBuckets& out) {
  if (node.IsLeaf()) {
    node.leaf_index = static_cast<int>(out.size());
    out.push_back({node.representative, node.ids});
    return;
  }
  for (auto& child : node.children) CollectLeaves(child, out);
}

std::vector<int> ChooseTopChildren(const Matrix& points, std::size_t point_id,
                                   const std::vector<RbcNode>& children, int topk, MetricKind metric) {
  std::vector<std::pair<float, int>> near;
  near.reserve(children.size());
  for (int i = 0; i < static_cast<int>(children.size()); ++i) {
    near.push_back({MetricScore(points[point_id], points[children[i].representative], metric), i});
  }
  topk = std::min(topk, static_cast<int>(near.size()));
  std::partial_sort(near.begin(), near.begin() + topk, near.end(), [&](const auto& lhs, const auto& rhs) {
    if (lhs.first != rhs.first) return lhs.first < rhs.first;
    return children[lhs.second].representative < children[rhs.second].representative;
  });

  std::vector<int> out;
  out.reserve(topk);
  for (int i = 0; i < topk; ++i) out.push_back(near[i].second);
  return out;
}

int RoutePointToLeaf(const Matrix& points, std::size_t point_id, const RbcNode& node, MetricKind metric) {
  if (node.IsLeaf()) return node.leaf_index;
  const auto next = ChooseTopChildren(points, point_id, node.children, 1, metric);
  return RoutePointToLeaf(points, point_id, node.children[next.front()], metric);
}

Leaves ApplyLeafOverlap(const Matrix& points, const RbcParams& params, const RbcNode& root,
                        const LeafBuckets& base_leaves) {
  Leaves leaves;
  leaves.reserve(base_leaves.size());
  for (const auto& leaf : base_leaves) leaves.push_back(leaf.ids);
  if (params.fanout <= 1 || root.IsLeaf()) return leaves;

  const int fanout = std::min<int>(params.fanout, root.children.size());
  std::vector<int> home_leaf(points.size(), -1);
  for (int leaf_idx = 0; leaf_idx < static_cast<int>(base_leaves.size()); ++leaf_idx) {
    for (int id : base_leaves[leaf_idx].ids) home_leaf[static_cast<std::size_t>(id)] = leaf_idx;
  }

  for (std::size_t point_id = 0; point_id < points.size(); ++point_id) {
    const auto child_indices = ChooseTopChildren(points, point_id, root.children, fanout, params.metric);
    const int home = home_leaf[point_id];
    int selected = 0;
    for (int child_index : child_indices) {
      const int leaf_index = RoutePointToLeaf(points, point_id, root.children[child_index], params.metric);
      if (leaf_index < 0 || leaf_index == home) continue;
      leaves[static_cast<std::size_t>(leaf_index)].push_back(static_cast<int>(point_id));
      ++selected;
      if (selected >= fanout - 1) break;
    }
  }

  for (auto& leaf : leaves) {
    std::sort(leaf.begin(), leaf.end());
    leaf.erase(std::unique(leaf.begin(), leaf.end()), leaf.end());
  }
  return leaves;
}

void FillStats(const Matrix& points, const Leaves& leaves, std::size_t fallback_chunk_splits, RbcStats* stats) {
  if (stats == nullptr) return;
  stats->leaf_count = leaves.size();
  stats->fallback_chunk_splits = fallback_chunk_splits;
  if (!leaves.empty()) stats->min_leaf_size = leaves.front().size();

  std::vector<std::size_t> membership(points.size(), 0);
  for (const auto& leaf : leaves) {
    stats->assignment_total += leaf.size();
    stats->min_leaf_size = stats->min_leaf_size == 0 ? leaf.size() : std::min(stats->min_leaf_size, leaf.size());
    stats->max_leaf_size = std::max(stats->max_leaf_size, leaf.size());
    for (int id : leaf) {
      if (id >= 0 && static_cast<std::size_t>(id) < membership.size()) {
        ++membership[static_cast<std::size_t>(id)];
      }
    }
  }
  for (std::size_t count : membership) {
    stats->points_with_overlap += count > 1 ? 1 : 0;
    stats->max_membership = std::max(stats->max_membership, count);
  }
}

std::vector<std::vector<int>> BuildPointMemberships(const Leaves& leaves, std::size_t point_count) {
  std::vector<std::vector<int>> point_memberships(point_count);
  for (int leaf_index = 0; leaf_index < static_cast<int>(leaves.size()); ++leaf_index) {
    const auto& leaf = leaves[static_cast<std::size_t>(leaf_index)];
    for (int id : leaf) {
      if (id < 0 || static_cast<std::size_t>(id) >= point_memberships.size()) continue;
      point_memberships[static_cast<std::size_t>(id)].push_back(leaf_index);
    }
  }
  return point_memberships;
}
}  // namespace

RbcResult BuildRbc(const Matrix& points, const RbcParams& params, RbcStats* stats) {
  if (stats != nullptr) *stats = {};
  if (points.empty()) return {};
  std::vector<int> ids(points.size());
  std::iota(ids.begin(), ids.end(), 0);
  std::mt19937 gen(params.seed);
  LeafBuckets base_leaves;
  std::size_t fallback_chunk_splits = 0;
  auto root = BuildNode(points, params, gen, ids, -1, &fallback_chunk_splits);
  CollectLeaves(root, base_leaves);
  auto result_leaves = ApplyLeafOverlap(points, params, root, base_leaves);
  FillStats(points, result_leaves, fallback_chunk_splits, stats);
  RbcResult result;
  result.point_memberships = BuildPointMemberships(result_leaves, points.size());
  result.leaves = std::move(result_leaves);
  return result;
}

Leaves BuildRbcLeaves(const Matrix& points, const RbcParams& params, RbcStats* stats) {
  return BuildRbc(points, params, stats).leaves;
}
}  // namespace pipnn
