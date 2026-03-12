#include "core/leaf_knn.h"

#include <cassert>

int main() {
  pipnn::Matrix points = {{0, 0}, {1, 0}, {5, 0}};
  std::vector<int> leaf = {0, 1, 2};

  auto edges = pipnn::BuildLeafKnnEdges(points, leaf, 1, true);
  bool has01 = false;
  bool has10 = false;
  bool self = false;
  for (auto [u, v] : edges) {
    if (u == 0 && v == 1) has01 = true;
    if (u == 1 && v == 0) has10 = true;
    if (u == v) self = true;
  }
  assert(has01);
  assert(has10);
  assert(!self);
  return 0;
}
