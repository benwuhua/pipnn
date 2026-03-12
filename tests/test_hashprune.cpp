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
  pipnn::HashPruneNodeStats same_bucket_stats;
  auto same_bucket = same_bucket_pruner.PruneNode(points, 0, {0, 5, 1, 3}, &same_bucket_stats);
  assert(same_bucket.size() == 1);
  assert(same_bucket[0] == 1);
  assert(same_bucket_stats.self_skipped == 1);
  assert(same_bucket_stats.kept == 2);
  assert(same_bucket_stats.replaced == 1);
  assert(same_bucket_stats.evicted == 0);
  assert(same_bucket_stats.dropped == 1);
  assert(same_bucket_stats.final_degree == same_bucket.size());

  // Equal-distance candidates in the same bucket should tie-break on smaller id.
  pipnn::Matrix tie_points = {
      {0.0f}, {1.0f}, {-1.0f},
  };
  pipnn::HashPruneParams tie_params;
  tie_params.hash_bits = 0;
  tie_params.max_degree = 1;
  pipnn::HashPruner tie_pruner(tie_params);
  pipnn::HashPruneNodeStats tie_stats;
  auto tied = tie_pruner.PruneNode(tie_points, 0, {2, 1}, &tie_stats);
  assert(tied.size() == 1);
  assert(tied[0] == 1);
  assert(tie_stats.kept == 2);
  assert(tie_stats.replaced == 1);
  assert(tie_stats.dropped == 0);
  assert(tie_stats.final_degree == tied.size());

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
  pipnn::HashPruneNodeStats replace_stats;
  auto replaced = replace_pruner.PruneNode(line, 2, {4, 1}, &replace_stats);
  assert(replaced.size() == 1);
  assert(replaced[0] == 1);
  assert(replace_stats.kept == 2);
  assert(replace_stats.replaced == 0);
  assert(replace_stats.evicted == 1);
  assert(replace_stats.dropped == 0);
  assert(replace_stats.final_degree == replaced.size());

  // Once the best candidate is already present, a farther candidate from another bucket must not replace it.
  pipnn::HashPruneNodeStats not_replaced_stats;
  auto not_replaced = replace_pruner.PruneNode(line, 2, {1, 4}, &not_replaced_stats);
  assert(not_replaced.size() == 1);
  assert(not_replaced[0] == 1);
  assert(not_replaced_stats.kept == 1);
  assert(not_replaced_stats.replaced == 0);
  assert(not_replaced_stats.evicted == 0);
  assert(not_replaced_stats.dropped == 1);
  assert(not_replaced_stats.final_degree == not_replaced.size());

  // With two slots and manual hyperplanes, a better unseen bucket may evict the farthest bucket,
  // but a worse unseen bucket must be dropped.
  pipnn::Matrix quad = {
      {0.0f, 0.0f},
      {1.0f, 1.0f},
      {-2.0f, 1.0f},
      {1.0f, -1.0f},
      {-3.0f, -3.0f},
  };
  pipnn::HashPruneParams quad_params;
  quad_params.hash_bits = 2;
  quad_params.max_degree = 2;
  pipnn::HashPruner quad_pruner(quad_params, pipnn::Matrix{{1.0f, 0.0f}, {0.0f, 1.0f}});
  pipnn::HashPruneNodeStats quad_stats;
  auto quad_out = quad_pruner.PruneNode(quad, 0, {1, 2, 3, 4}, &quad_stats);
  assert(quad_out.size() == 2);
  assert(quad_out[0] == 1);
  assert(quad_out[1] == 3);
  assert(quad_stats.kept == 3);
  assert(quad_stats.replaced == 0);
  assert(quad_stats.evicted == 1);
  assert(quad_stats.dropped == 1);
  assert(quad_stats.final_degree == quad_out.size());
  return 0;
}
