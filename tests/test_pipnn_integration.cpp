#include "bench/runner.h"
#include "core/pipnn_builder.h"
#include "core/graph.h"
#include "search/greedy_beam.h"

#include <cassert>

int main() {
  pipnn::PipnnBuildParams empty_bp;
  pipnn::PipnnBuildStats empty_stats;
  auto empty_graph = pipnn::BuildPipnnGraph({}, empty_bp, &empty_stats);
  assert(empty_graph.NumNodes() == 0);
  assert(empty_graph.EdgeCount() == 0);
  assert(empty_stats.candidate_edges == 0);
  assert(pipnn::SearchGraph({}, pipnn::Graph{}, pipnn::Vec{}, pipnn::SearchParams{}).empty());

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
  bp.replicas = 0;
  bp.hashprune.max_degree = 16;

  pipnn::PipnnBuildStats stats;
  auto g = pipnn::BuildPipnnGraph(base, bp, &stats);
  assert(g.NumNodes() == static_cast<int>(base.size()));
  assert(g.EdgeCount() > 0);
  assert(stats.num_leaves > 0);
  assert(stats.candidate_edges > 0);
  for (int i = 0; i < g.NumNodes(); ++i) {
    assert(static_cast<int>(g.Neighbors(i).size()) <= bp.hashprune.max_degree);
  }

  pipnn::RunnerConfig cfg;
  cfg.mode = "pipnn";
  pipnn::SearchParams sp;
  auto m = pipnn::RunBenchmark(cfg, base, queries, {}, bp, sp);
  assert(m.edges > 0);
  assert(m.qps > 0.0);
  assert(m.recall_at_10 >= 0.0);

  // Empty query input should keep qps/recall at zero while still building the graph.
  auto no_query = pipnn::RunBenchmark(cfg, base, {}, {}, bp, sp);
  assert(no_query.edges > 0);
  assert(no_query.qps == 0.0);
  assert(no_query.recall_at_10 == 0.0);

  pipnn::RunnerConfig hcfg;
  hcfg.mode = "hnsw";
  sp.topk = 2;
  std::vector<std::vector<int>> truth = {
      {0, 1},
      {3, 4},
  };
  pipnn::Matrix small_base = {
      {0.0f, 0.0f},
      {1.0f, 0.0f},
      {0.0f, 1.0f},
      {5.0f, 5.0f},
      {6.0f, 6.0f},
  };
  pipnn::Matrix small_queries = {
      {0.1f, 0.1f},
      {5.2f, 5.1f},
  };
  auto hnsw_metrics = pipnn::RunBenchmark(hcfg, small_base, small_queries, truth, bp, sp);
  assert(hnsw_metrics.edges == small_base.size() * 32);
  assert(hnsw_metrics.qps > 0.0);

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

  // Duplicated and cyclic edges should not loop forever, and beam=0 should yield no result.
  g2.SetNeighbors(50, {40, 40});
  pipnn::SearchParams sp3;
  sp3.entry = 0;
  sp3.beam = 0;
  sp3.topk = 3;
  assert(pipnn::SearchGraph(pts, g2, pipnn::Vec{0.0f, 0.0f}, sp3).empty());

  pipnn::SearchParams sp4;
  sp4.entry = 0;
  sp4.beam = 3;
  sp4.topk = 10;
  auto cycle_out = pipnn::SearchGraph(pts, g2, pipnn::Vec{0.0f, 0.0f}, sp4);
  assert(!cycle_out.empty());
  assert(static_cast<int>(cycle_out.size()) <= sp4.beam);

  auto json = pipnn::ToJson(hnsw_metrics);
  assert(json.find("\"mode\": \"hnsw\"") != std::string::npos);
  assert(json.find("\"edges\": ") != std::string::npos);

  return 0;
}
