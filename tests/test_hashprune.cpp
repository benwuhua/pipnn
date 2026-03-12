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

  // Empty point-set is a no-op.
  assert(pruner.PruneNode({}, 0, candidates).empty());

  // hash_bits=0 forces all candidates into the same bucket so the closest one wins.
  pipnn::HashPruneParams same_bucket_params;
  same_bucket_params.hash_bits = 0;
  same_bucket_params.max_degree = 2;
  pipnn::HashPruner same_bucket_pruner(same_bucket_params);
  auto same_bucket = same_bucket_pruner.PruneNode(points, 0, {0, 5, 1, 3});
  assert(same_bucket.size() == 1);
  assert(same_bucket[0] == 1);

  // Equal-distance candidates in the same bucket should tie-break on smaller id.
  pipnn::Matrix tie_points = {
      {0.0f}, {1.0f}, {-1.0f},
  };
  pipnn::HashPruneParams tie_params;
  tie_params.hash_bits = 0;
  tie_params.max_degree = 1;
  pipnn::HashPruner tie_pruner(tie_params);
  auto tied = tie_pruner.PruneNode(tie_points, 0, {2, 1});
  assert(tied.size() == 1);
  assert(tied[0] == 1);

  // Identical residuals should hash to all-ones because dot==0 sets every bit.
  pipnn::HashPruneParams residual_params;
  residual_params.hash_bits = 3;
  pipnn::HashPruner residual_pruner(residual_params);
  auto self_code = residual_pruner.HashResidualForTest(pipnn::Vec{1.0f, -2.0f}, pipnn::Vec{1.0f, -2.0f});
  assert(self_code == 7);
  auto pos_code = residual_pruner.HashResidualForTest(pipnn::Vec{0.0f}, pipnn::Vec{1.0f});
  auto neg_code = residual_pruner.HashResidualForTest(pipnn::Vec{0.0f}, pipnn::Vec{-1.0f});
  assert(pos_code != neg_code);

  pipnn::HashPruneParams manual_params;
  manual_params.hash_bits = 2;
  manual_params.max_degree = 1;
  pipnn::HashPruner manual_pruner(manual_params, pipnn::Matrix{{1.0f}, {-1.0f}});
  assert(manual_pruner.HashResidualForTest(pipnn::Vec{0.0f}, pipnn::Vec{1.0f}) == 1);
  assert(manual_pruner.HashResidualForTest(pipnn::Vec{0.0f}, pipnn::Vec{-1.0f}) == 2);

  // With one bit and one slot, a closer candidate from a different bucket should replace a farther one.
  pipnn::Matrix line = {
      {-2.0f}, {-1.0f}, {0.0f}, {1.0f}, {2.0f},
  };
  pipnn::HashPruneParams replace_params;
  replace_params.hash_bits = 1;
  replace_params.max_degree = 1;
  replace_params.seed = 7;
  pipnn::HashPruner replace_pruner(replace_params);
  auto replaced = replace_pruner.PruneNode(line, 2, {4, 1});
  assert(replaced.size() == 1);
  assert(replaced[0] == 1);

  // Once the best candidate is already present, a farther candidate from another bucket must not replace it.
  auto not_replaced = replace_pruner.PruneNode(line, 2, {1, 4});
  assert(not_replaced.size() == 1);
  assert(not_replaced[0] == 1);
  return 0;
}
