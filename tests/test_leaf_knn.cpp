#include "core/leaf_knn.h"
#include "core/leaf_knn_blocked.h"

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
    pipnn::Matrix exact_points = {{0.0f}, {1.0f}, {3.0f}, {10.0f}};
    std::vector<int> exact_leaf = {0, 1, 2, 3};
    auto naive = pipnn::BuildLeafKnnExactEdges(exact_points, exact_leaf, 2, false,
                                               pipnn::LeafKnnMode::NaiveFull);
    auto blocked = pipnn::BuildLeafKnnExactEdges(exact_points, exact_leaf, 2, false,
                                                 pipnn::LeafKnnMode::BlockedFull);
    assert(naive == blocked);
    for (auto [u, v] : naive) {
      assert(u != v);
    }
  }

  {
    pipnn::Matrix all_points = {{0.0f}, {2.0f}, {5.0f}};
    std::vector<int> all_leaf = {0, 1, 2};
    auto all_edges = pipnn::BuildLeafKnnExactEdges(all_points, all_leaf, 8, false,
                                                   pipnn::LeafKnnMode::NaiveFull);
    assert(all_edges.size() == 6);
    for (auto [u, v] : all_edges) {
      assert(u != v);
    }
  }

  {
    pipnn::LeafKnnConfig cfg;
    cfg.min_leaf_for_blocked = 4;
    assert(pipnn::SelectLeafKnnMode(3, 0, cfg) == pipnn::LeafKnnMode::NaiveFull);
    assert(pipnn::SelectLeafKnnMode(4, 1, cfg) == pipnn::LeafKnnMode::NaiveFull);
    assert(pipnn::SelectLeafKnnMode(4, 0, cfg) == pipnn::LeafKnnMode::BlockedFull);
  }

  assert(pipnn::BuildLeafKnnEdges(points, {}, 2, true).empty());
  assert(pipnn::BuildLeafKnnEdges(points, leaf, 0, true).empty());
  return 0;
}
