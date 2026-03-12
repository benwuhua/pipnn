#include "bench/runner.h"
#include "core/pipnn_builder.h"
#include "core/graph.h"
#include "search/greedy_beam.h"

#include <cassert>

int main() {
  pipnn::Matrix base(300, pipnn::Vec(8, 0.0f));
  for (int i = 0; i < 300; ++i) {
    base[i][0] = static_cast<float>(i);
    base[i][1] = static_cast<float>(i % 7);
  }
  pipnn::Matrix queries(20, pipnn::Vec(8, 0.0f));
  for (int i = 0; i < 20; ++i) queries[i][0] = static_cast<float>(i * 3);

  pipnn::PipnnBuildParams bp;
  bp.rbc.cmax = 64;
  bp.rbc.fanout = 2;
  bp.leaf_k = 8;
  bp.hashprune.max_degree = 16;

  auto g = pipnn::BuildPipnnGraph(base, bp);
  assert(g.NumNodes() == static_cast<int>(base.size()));
  assert(g.EdgeCount() > 0);
  for (int i = 0; i < g.NumNodes(); ++i) {
    assert(static_cast<int>(g.Neighbors(i).size()) <= bp.hashprune.max_degree);
  }

  pipnn::RunnerConfig cfg;
  cfg.mode = "pipnn";
  pipnn::SearchParams sp;
  auto m = pipnn::RunBenchmark(cfg, base, queries, {}, bp, sp);
  assert(m.edges > 0);
  assert(m.qps > 0.0);

  // Regression test: best-first search should not get trapped in a deep bad branch.
  pipnn::Matrix pts(60, pipnn::Vec(2, 0.0f));
  for (int i = 0; i < 60; ++i) {
    pts[i][0] = 1000.0f + i;
  }
  pts[50][0] = 0.01f;  // nearest to query
  pts[40][0] = 1000.5f;
  pipnn::Graph g2(60);
  g2.SetNeighbors(0, {1, 40});
  for (int i = 1; i < 30; ++i) g2.SetNeighbors(i, {i + 1});
  for (int i = 30; i < 60; ++i) g2.SetNeighbors(i, {});
  g2.SetNeighbors(40, {50});

  pipnn::SearchParams sp2;
  sp2.entry = 0;
  sp2.beam = 15;
  sp2.topk = 1;
  auto out = pipnn::SearchGraph(pts, g2, pipnn::Vec{0.0f, 0.0f}, sp2);
  assert(!out.empty());
  assert(out[0] == 50);

  return 0;
}
