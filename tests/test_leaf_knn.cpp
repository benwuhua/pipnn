#include "core/leaf_knn.h"
#include "core/leaf_knn_blocked.h"

#include <cassert>
#include <algorithm>

namespace {
std::vector<pipnn::Edge> SortedEdges(std::vector<pipnn::Edge> edges) {
  std::sort(edges.begin(), edges.end());
  return edges;
}
}  // namespace

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

  auto directed = pipnn::BuildLeafKnnEdges(points, leaf, 1, false);
  assert(directed.size() == leaf.size());

  auto capped = pipnn::BuildLeafKnnEdges(points, leaf, 4, true, 1);
  assert(!capped.empty());
  for (auto [u, v] : capped) {
    assert(u != v);
  }

  pipnn::Matrix capped_points = {{0.0f}, {100.0f}, {1.0f}, {2.0f}};
  std::vector<int> capped_leaf = {0, 1, 2, 3};
  auto exact_capped = pipnn::BuildLeafKnnEdges(capped_points, capped_leaf, 1, false, 1);
  assert(exact_capped.size() == 4);
  assert(exact_capped[0].first == 0 && exact_capped[0].second == 1);
  assert(exact_capped[1].first == 1 && exact_capped[1].second == 2);
  assert(exact_capped[2].first == 2 && exact_capped[2].second == 3);
  assert(exact_capped[3].first == 3 && exact_capped[3].second == 0);

  auto full_scan = pipnn::BuildLeafKnnEdges(capped_points, capped_leaf, 1, false, 3);
  assert(full_scan.size() == 4);
  assert(full_scan[0].first == 0 && full_scan[0].second == 2);
  assert(full_scan[1].first == 1 && full_scan[1].second == 3);
  assert(full_scan[2].first == 2 && full_scan[2].second == 0);
  assert(full_scan[3].first == 3 && full_scan[3].second == 2);

  {
    pipnn::Matrix exact_points = {
        {0.0f}, {1.0f}, {3.0f}, {10.0f}, {20.0f}, {21.0f}, {22.5f}, {30.0f},
    };
    std::vector<int> first_leaf = {0, 1, 2, 3};
    std::vector<int> second_leaf = {4, 5, 6, 7};
    std::vector<pipnn::LeafBatchJob> jobs = {
        {.leaf = first_leaf},
        {.leaf = second_leaf},
    };
    pipnn::LeafBatchConfig cfg;
    cfg.min_leaf_for_batch = 4;
    cfg.point_block_rows = 2;
    cfg.max_points_per_batch = 8;

    auto expected = pipnn::BuildLeafKnnExactEdgesNaive(exact_points, first_leaf, 2, false);
    auto second_expected = pipnn::BuildLeafKnnExactEdgesNaive(exact_points, second_leaf, 2, false);
    expected.insert(expected.end(), second_expected.begin(), second_expected.end());

    auto batched = pipnn::BuildLeafKnnExactBatchedEdges(exact_points, jobs, 2, false, cfg);
    assert(SortedEdges(expected) == SortedEdges(batched));
    for (auto [u, v] : batched) {
      assert(u != v);
      const bool same_first = u < 4 && v < 4;
      const bool same_second = u >= 4 && v >= 4;
      assert(same_first || same_second);
    }
  }

  {
    std::vector<pipnn::LeafBatchJob> jobs = {
        {.leaf = {0, 1, 2, 3}},
        {.leaf = {4, 5, 6, 7}},
        {.leaf = {8, 9, 10, 11}},
    };
    pipnn::LeafBatchConfig cfg;
    cfg.min_leaf_for_batch = 4;
    cfg.max_points_per_batch = 8;
    auto plans = pipnn::PlanLeafKnnBatches(jobs, cfg);
    assert(plans.size() == 2);
    assert(plans[0].total_points == 8);
    assert(plans[1].total_points == 4);
    assert(plans[0].job_indices == std::vector<int>({0, 1}));
    assert(plans[1].job_indices == std::vector<int>({2}));

    cfg.max_points_per_batch = 4;
    plans = pipnn::PlanLeafKnnBatches(jobs, cfg);
    assert(plans.size() == 3);
    for (const auto& plan : plans) {
      assert(plan.total_points <= cfg.max_points_per_batch);
    }
  }

  {
    pipnn::Matrix all_points = {{0.0f}, {2.0f}, {5.0f}};
    std::vector<int> all_leaf = {0, 1, 2};
    auto all_edges = pipnn::BuildLeafKnnExactEdgesNaive(all_points, all_leaf, 8, false);
    assert(all_edges.size() == 6);
    for (auto [u, v] : all_edges) {
      assert(u != v);
    }
  }

  assert(pipnn::BuildLeafKnnEdges(points, {}, 2, true).empty());
  assert(pipnn::BuildLeafKnnEdges(points, leaf, 0, true).empty());
  return 0;
}
