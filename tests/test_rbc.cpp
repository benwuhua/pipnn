#include "core/rbc.h"

#include <algorithm>
#include <cassert>

int main() {
  pipnn::Matrix points(200, pipnn::Vec(4, 0.0f));
  for (int i = 0; i < 200; ++i) points[i][0] = static_cast<float>(i);

  pipnn::RbcParams p;
  p.cmax = 32;
  p.fanout = 2;
  p.leader_frac = 0.1f;

  auto leaves = pipnn::BuildRbcLeaves(points, p);
  assert(!leaves.empty());
  for (const auto& leaf : leaves) {
    assert(static_cast<int>(leaf.size()) <= p.cmax || static_cast<int>(leaf.size()) == 200);
  }

  std::vector<int> appear(200, 0);
  for (const auto& leaf : leaves) {
    for (int id : leaf) appear[id]++;
  }
  for (int x : appear) assert(x >= 1);

  pipnn::Matrix tiny(4, pipnn::Vec(2, 1.0f));
  pipnn::RbcParams tiny_params;
  tiny_params.cmax = 8;
  auto tiny_leaves = pipnn::BuildRbcLeaves(tiny, tiny_params);
  assert(tiny_leaves.size() == 1);
  assert(tiny_leaves[0].size() == tiny.size());

  pipnn::Matrix exact(8, pipnn::Vec(2, 0.0f));
  for (int i = 0; i < 8; ++i) exact[i][0] = static_cast<float>(i);
  pipnn::RbcParams exact_params;
  exact_params.cmax = 8;
  exact_params.fanout = 2;
  exact_params.leader_frac = 0.25f;
  auto exact_leaves = pipnn::BuildRbcLeaves(exact, exact_params);
  assert(exact_leaves.size() == 1);
  assert(exact_leaves[0].size() == exact.size());

  // Identical points + full fanout trigger the chunk-split fallback path.
  pipnn::Matrix identical(20, pipnn::Vec(2, 0.0f));
  pipnn::RbcParams fallback;
  fallback.cmax = 4;
  fallback.fanout = 2;
  fallback.leader_frac = 0.05f;
  fallback.max_leaders = 2;
  auto fallback_leaves = pipnn::BuildRbcLeaves(identical, fallback);
  assert(!fallback_leaves.empty());
  int total_ids = 0;
  for (const auto& leaf : fallback_leaves) {
    assert(static_cast<int>(leaf.size()) <= fallback.cmax);
    total_ids += static_cast<int>(leaf.size());
  }
  assert(total_ids > static_cast<int>(identical.size()));

  // Exact-multiple chunk splits must not emit trailing empty leaves.
  pipnn::Matrix identical_exact(8, pipnn::Vec(2, 0.0f));
  auto exact_split_leaves = pipnn::BuildRbcLeaves(identical_exact, fallback);
  assert(exact_split_leaves.size() == 4);
  for (const auto& leaf : exact_split_leaves) {
    assert(!leaf.empty());
    assert(static_cast<int>(leaf.size()) == fallback.cmax);
  }

  // Seeded 1D input should keep a stable leaf layout when leader count uses multiplication.
  pipnn::Matrix seeded(10, pipnn::Vec(1, 0.0f));
  for (int i = 0; i < 10; ++i) seeded[i][0] = static_cast<float>(i);
  pipnn::RbcParams seeded_params;
  seeded_params.cmax = 3;
  seeded_params.fanout = 1;
  seeded_params.leader_frac = 0.25f;
  seeded_params.max_leaders = 10;
  seeded_params.seed = 1;
  auto seeded_leaves = pipnn::BuildRbcLeaves(seeded, seeded_params);
  assert(!seeded_leaves.empty());
  assert(seeded_leaves.size() < seeded.size());
  bool saw_full_leaf = false;
  bool saw_non_singleton = false;
  for (const auto& leaf : seeded_leaves) {
    assert(!leaf.empty());
    assert(static_cast<int>(leaf.size()) <= seeded_params.cmax);
    saw_full_leaf = saw_full_leaf || static_cast<int>(leaf.size()) == seeded_params.cmax;
    saw_non_singleton = saw_non_singleton || leaf.size() > 1;
  }
  assert(saw_full_leaf);
  assert(saw_non_singleton);

  return 0;
}
