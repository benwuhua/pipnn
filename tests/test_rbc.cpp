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

  return 0;
}
