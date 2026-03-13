#include "search/vamana_search_adapter.h"

#include <cassert>

int main() {
  pipnn::Matrix points = {{0.0f}, {1.0f}, {2.0f}, {3.0f}};
  pipnn::Graph graph(4);
  graph.SetNeighbors(0, {1, 2});
  graph.SetNeighbors(1, {0, 2});
  graph.SetNeighbors(2, {1, 3});
  graph.SetNeighbors(3, {2});

  pipnn::VamanaSearchParams params;
  params.entry = 0;
  params.beam = 8;
  params.topk = 2;

  auto out = pipnn::SearchVamanaGraph(points, graph, pipnn::Vec{2.1f}, params);
  assert(!out.empty());
  return 0;
}
