#include "core/rbc.h"
#include "core/rbc_assignment.h"

#include <algorithm>
#include <cassert>

int main() {
  pipnn::Matrix points(200, pipnn::Vec(4, 0.0f));
  for (int i = 0; i < 200; ++i) points[i][0] = static_cast<float>(i);

  pipnn::RbcParams p;
  p.cmax = 32;
  p.fanout = 2;
  p.leader_frac = 0.1f;

  pipnn::RbcStats stats;
  auto leaves = pipnn::BuildRbcLeaves(points, p, &stats);
  assert(!leaves.empty());
  for (const auto& leaf : leaves) {
    assert(!leaf.empty());
  }
  assert(stats.leaf_count == leaves.size());
  assert(stats.assignment_total > points.size());
  assert(stats.assignment_total <= points.size() * static_cast<std::size_t>(p.fanout));
  assert(stats.points_with_overlap > 0);
  assert(stats.max_membership >= 2);
  assert(stats.max_membership <= static_cast<std::size_t>(p.fanout));
  assert(stats.min_leaf_size > 0);

  std::vector<int> appear(200, 0);
  for (const auto& leaf : leaves) {
    for (int id : leaf) appear[id]++;
  }
  for (int x : appear) assert(x >= 1);
  std::size_t overlap_points = 0;
  int max_membership = 0;
  for (int x : appear) {
    overlap_points += x > 1 ? 1 : 0;
    max_membership = std::max(max_membership, x);
  }
  assert(stats.points_with_overlap == overlap_points);
  assert(stats.max_membership == static_cast<std::size_t>(max_membership));

  pipnn::Matrix tiny(4, pipnn::Vec(2, 1.0f));
  pipnn::RbcParams tiny_params;
  tiny_params.cmax = 8;
  pipnn::RbcStats tiny_stats;
  auto tiny_leaves = pipnn::BuildRbcLeaves(tiny, tiny_params, &tiny_stats);
  assert(tiny_leaves.size() == 1);
  assert(tiny_leaves[0].size() == tiny.size());
  assert(tiny_stats.assignment_total == tiny.size());
  assert(tiny_stats.points_with_overlap == 0);
  assert(tiny_stats.max_membership == 1);

  pipnn::Matrix exact(8, pipnn::Vec(2, 0.0f));
  for (int i = 0; i < 8; ++i) exact[i][0] = static_cast<float>(i);
  pipnn::RbcParams exact_params;
  exact_params.cmax = 8;
  exact_params.fanout = 2;
  exact_params.leader_frac = 0.25f;
  pipnn::RbcStats exact_stats;
  auto exact_leaves = pipnn::BuildRbcLeaves(exact, exact_params, &exact_stats);
  assert(exact_leaves.size() == 1);
  assert(exact_leaves[0].size() == exact.size());
  assert(exact_stats.assignment_total == exact.size());
  assert(exact_stats.points_with_overlap == 0);

  // Identical points + full fanout trigger the chunk-split fallback path.
  pipnn::Matrix identical(20, pipnn::Vec(2, 0.0f));
  pipnn::RbcParams fallback;
  fallback.cmax = 4;
  fallback.fanout = 2;
  fallback.leader_frac = 0.05f;
  fallback.max_leaders = 2;
  pipnn::RbcStats fallback_stats;
  auto fallback_leaves = pipnn::BuildRbcLeaves(identical, fallback, &fallback_stats);
  assert(!fallback_leaves.empty());
  int total_ids = 0;
  for (const auto& leaf : fallback_leaves) {
    assert(static_cast<int>(leaf.size()) <= static_cast<int>(identical.size()));
    total_ids += static_cast<int>(leaf.size());
  }
  assert(total_ids > static_cast<int>(identical.size()));
  assert(total_ids <= static_cast<int>(identical.size()) * fallback.fanout);
  assert(fallback_stats.assignment_total == static_cast<std::size_t>(total_ids));
  assert(fallback_stats.fallback_chunk_splits > 0);
  assert(fallback_stats.max_membership <= static_cast<std::size_t>(fallback.fanout));

  // Exact-multiple chunk splits must not emit trailing empty leaves.
  pipnn::Matrix identical_exact(8, pipnn::Vec(2, 0.0f));
  pipnn::RbcStats exact_split_stats;
  auto exact_split_leaves = pipnn::BuildRbcLeaves(identical_exact, fallback, &exact_split_stats);
  assert(exact_split_leaves.size() == 2);
  for (const auto& leaf : exact_split_leaves) {
    assert(!leaf.empty());
    assert(static_cast<int>(leaf.size()) == 8);
  }
  assert(exact_split_stats.fallback_chunk_splits > 0);
  assert(exact_split_stats.min_leaf_size == 8);
  assert(exact_split_stats.max_leaf_size == 8);

  // Seeded 1D input should keep a stable leaf layout when leader count uses multiplication.
  pipnn::Matrix seeded(10, pipnn::Vec(1, 0.0f));
  for (int i = 0; i < 10; ++i) seeded[i][0] = static_cast<float>(i);
  pipnn::RbcParams seeded_params;
  seeded_params.cmax = 3;
  seeded_params.fanout = 1;
  seeded_params.leader_frac = 0.25f;
  seeded_params.max_leaders = 10;
  seeded_params.seed = 1;
  pipnn::RbcStats seeded_stats;
  auto seeded_leaves = pipnn::BuildRbcLeaves(seeded, seeded_params, &seeded_stats);
  assert(!seeded_leaves.empty());
  assert(seeded_leaves.size() < seeded.size());
  bool saw_non_singleton = false;
  for (const auto& leaf : seeded_leaves) {
    assert(!leaf.empty());
    assert(static_cast<int>(leaf.size()) <= seeded_params.cmax);
    saw_non_singleton = saw_non_singleton || leaf.size() > 1;
  }
  assert(saw_non_singleton);
  assert(seeded_stats.assignment_total == seeded.size());
  assert(seeded_stats.points_with_overlap == 0);
  assert(seeded_stats.max_membership == 1);

  {
    pipnn::Matrix points = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {2.0f, 0.0f},
        {3.0f, 0.0f},
    };
    std::vector<int> ids = {0, 1, 2, 3};
    std::vector<int> leaders = {0, 3};
    auto scalar = pipnn::AssignPointsToLeaders(points, ids, leaders, pipnn::RbcAssignMode::Scalar);
    auto blocked = pipnn::AssignPointsToLeaders(points, ids, leaders, pipnn::RbcAssignMode::Blocked);
    assert((scalar == std::vector<int>{0, 0, 1, 1}));
    assert(blocked == scalar);
  }

  {
    pipnn::Matrix points = {
        {1.0f, 0.0f},
        {0.0f, 0.0f},
        {2.0f, 0.0f},
    };
    std::vector<int> ids = {0};
    std::vector<int> leaders = {2, 1};
    auto scalar = pipnn::AssignPointsToLeaders(points, ids, leaders, pipnn::RbcAssignMode::Scalar);
    auto blocked = pipnn::AssignPointsToLeaders(points, ids, leaders, pipnn::RbcAssignMode::Blocked);
    assert(scalar.size() == 1);
    assert(scalar[0] == 1);
    assert(blocked == scalar);
  }

  {
    pipnn::Matrix points(33, pipnn::Vec(3, 0.0f));
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
      points[static_cast<std::size_t>(i)][0] = static_cast<float>(i);
      points[static_cast<std::size_t>(i)][1] = static_cast<float>(i % 5);
      points[static_cast<std::size_t>(i)][2] = static_cast<float>(i % 7);
    }
    std::vector<int> ids(points.size());
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) ids[static_cast<std::size_t>(i)] = i;
    std::vector<int> leaders = {0, 8, 16, 24, 32};
    auto scalar = pipnn::AssignPointsToLeaders(points, ids, leaders, pipnn::RbcAssignMode::Scalar);
    auto blocked = pipnn::AssignPointsToLeaders(points, ids, leaders, pipnn::RbcAssignMode::Blocked);
    assert(blocked == scalar);
  }

  {
    pipnn::RbcAssignConfig cfg;
    cfg.min_points_for_blocked = 16;
    cfg.min_leaders_for_blocked = 4;
    assert(pipnn::SelectRbcAssignMode(15, 4, cfg) == pipnn::RbcAssignMode::Scalar);
    assert(pipnn::SelectRbcAssignMode(16, 3, cfg) == pipnn::RbcAssignMode::Scalar);
    assert(pipnn::SelectRbcAssignMode(16, 4, cfg) == pipnn::RbcAssignMode::Blocked);
  }

  return 0;
}
