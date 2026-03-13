#include "bench/runner.h"
#include "core/pipnn_builder.h"
#include "core/graph.h"
#include "core/leaf_knn.h"
#include "search/greedy_beam.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
struct ScopedEnv {
  explicit ScopedEnv(const char* name, const char* value) : name(name) {
    const char* cur = std::getenv(name);
    if (cur != nullptr) {
      had_value = true;
      old_value = cur;
    }
    if (value != nullptr) {
      setenv(name, value, 1);
    } else {
      unsetenv(name);
    }
  }

  ~ScopedEnv() {
    if (had_value) {
      setenv(name, old_value.c_str(), 1);
    } else {
      unsetenv(name);
    }
  }

  const char* name;
  bool had_value = false;
  std::string old_value;
};

struct ScopedCoutCapture {
  ScopedCoutCapture() : old(std::cout.rdbuf(stream.rdbuf())) {}
  ~ScopedCoutCapture() { std::cout.rdbuf(old); }

  std::ostringstream stream;
  std::streambuf* old;
};
}  // namespace

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
  assert(stats.rbc_assignment_total > base.size());
  assert(stats.rbc_assignment_total <= base.size() * static_cast<std::size_t>(bp.rbc.fanout));
  assert(stats.rbc_points_with_overlap > 0);
  assert(stats.rbc_max_membership >= 2);
  assert(stats.rbc_max_membership <= static_cast<std::size_t>(bp.rbc.fanout));
  assert(stats.rbc_min_leaf_size > 0);
  assert(stats.prune_kept > 0);
  assert(stats.prune_final_edges == g.EdgeCount());
  assert(stats.prune_kept >= stats.prune_final_edges);
  assert(stats.prune_dropped + stats.prune_kept >= stats.prune_final_edges);
  for (int i = 0; i < g.NumNodes(); ++i) {
    assert(static_cast<int>(g.Neighbors(i).size()) <= bp.hashprune.max_degree);
  }

  {
    ScopedEnv clear_profile("PIPNN_PROFILE", nullptr);
    ScopedCoutCapture capture;
    pipnn::PipnnBuildStats local_stats;
    auto quiet_graph = pipnn::BuildPipnnGraph(base, bp, &local_stats);
    assert(quiet_graph.NumNodes() == static_cast<int>(base.size()));
    assert(capture.stream.str().empty());
  }

  {
    ScopedEnv enable_profile("PIPNN_PROFILE", "1");
    ScopedCoutCapture capture;
    pipnn::PipnnBuildStats profile_stats;
    auto profile_graph = pipnn::BuildPipnnGraph(base, bp, &profile_stats);
    assert(profile_graph.NumNodes() == static_cast<int>(base.size()));
    auto out = capture.stream.str();
    assert(out.find("pipnn_profile_build partition_sec=") != std::string::npos);
    assert(out.find("prune_kept=") != std::string::npos);
    assert(out.find("prune_dropped=") != std::string::npos);
    assert(profile_stats.prune_final_edges == profile_graph.EdgeCount());
  }

  pipnn::PipnnBuildParams replica_bp = bp;
  replica_bp.replicas = 2;
  replica_bp.rbc.seed = 11;
  pipnn::PipnnBuildStats replica_stats;
  auto replica_graph = pipnn::BuildPipnnGraph(base, replica_bp, &replica_stats);
  std::size_t expected_leaves = 0;
  std::size_t expected_rbc_assignment_total = 0;
  std::size_t expected_rbc_points_with_overlap = 0;
  std::size_t expected_rbc_fallback_chunk_splits = 0;
  std::size_t expected_rbc_max_membership = 0;
  std::size_t expected_rbc_max_leaf_size = 0;
  std::size_t expected_rbc_min_leaf_size = 0;
  for (int rep = 0; rep < replica_bp.replicas; ++rep) {
    auto rbc_params = replica_bp.rbc;
    rbc_params.seed = replica_bp.rbc.seed + rep;
    pipnn::RbcStats replica_rbc_stats;
    auto replica_leaves = pipnn::BuildRbcLeaves(base, rbc_params, &replica_rbc_stats);
    expected_leaves += replica_leaves.size();
    expected_rbc_assignment_total += replica_rbc_stats.assignment_total;
    expected_rbc_points_with_overlap += replica_rbc_stats.points_with_overlap;
    expected_rbc_fallback_chunk_splits += replica_rbc_stats.fallback_chunk_splits;
    expected_rbc_max_membership =
        std::max(expected_rbc_max_membership, replica_rbc_stats.max_membership);
    expected_rbc_max_leaf_size = std::max(expected_rbc_max_leaf_size, replica_rbc_stats.max_leaf_size);
    if (replica_rbc_stats.min_leaf_size > 0) {
      expected_rbc_min_leaf_size = expected_rbc_min_leaf_size == 0
                                       ? replica_rbc_stats.min_leaf_size
                                       : std::min(expected_rbc_min_leaf_size, replica_rbc_stats.min_leaf_size);
    }
  }
  assert(replica_graph.NumNodes() == static_cast<int>(base.size()));
  assert(replica_stats.num_leaves == expected_leaves);
  const std::size_t candidate_edge_upper =
      base.size() * static_cast<std::size_t>(std::max(1, replica_bp.leaf_k)) *
      (replica_bp.bidirected ? 2u : 1u) * static_cast<std::size_t>(replica_bp.replicas);
  assert(replica_stats.candidate_edges > 0);
  assert(replica_stats.candidate_edges <= candidate_edge_upper);
  assert(replica_stats.rbc_assignment_total == expected_rbc_assignment_total);
  assert(replica_stats.rbc_assignment_total <=
         base.size() * static_cast<std::size_t>(replica_bp.rbc.fanout) *
             static_cast<std::size_t>(replica_bp.replicas));
  assert(replica_stats.rbc_points_with_overlap == expected_rbc_points_with_overlap);
  assert(replica_stats.rbc_fallback_chunk_splits == expected_rbc_fallback_chunk_splits);
  assert(replica_stats.rbc_max_membership == expected_rbc_max_membership);
  assert(replica_stats.rbc_max_leaf_size == expected_rbc_max_leaf_size);
  assert(replica_stats.rbc_min_leaf_size == expected_rbc_min_leaf_size);
  assert(replica_stats.prune_kept > 0);
  assert(replica_stats.prune_final_edges == replica_graph.EdgeCount());

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
  hcfg.hnsw.m = 24;
  hcfg.hnsw.ef_construction = 320;
  hcfg.hnsw.ef_search = 96;
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
  assert(hnsw_metrics.edges == small_base.size() * 24);
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
