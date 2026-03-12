#include "core/hashprune.h"

#include <algorithm>
#include <cassert>
#include <random>

int main() {
  pipnn::Matrix points = {
      {0, 0}, {1, 0}, {0, 1}, {2, 0}, {0, 2}, {3, 3},
  };

  pipnn::HashPruneParams hp;
  hp.hash_bits = 6;
  hp.max_degree = 3;
  hp.seed = 123;
  pipnn::HashPruner pruner(hp);

  std::vector<int> candidates = {1, 2, 3, 4, 5};
  auto a = pruner.PruneNode(points, 0, candidates);
  assert(a.size() <= 3);

  std::mt19937 gen(42);
  auto shuffled = candidates;
  std::shuffle(shuffled.begin(), shuffled.end(), gen);
  auto b = pruner.PruneNode(points, 0, shuffled);
  assert(a == b);
  return 0;
}
