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

  return 0;
}
