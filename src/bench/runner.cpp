#include "bench/runner.h"

#include "baseline/hnsw_runner.h"
#include "candidates/pipnn_candidate_generator.h"
#include "common/timer.h"
#include "core/distance.h"
#include "refine/vamana_refiner.h"
#include "search/vamana_search_adapter.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace pipnn {
namespace {
CandidateAdjacency BuildNativeVamanaCandidates(const Matrix& base, int max_degree, MetricKind metric) {
  CandidateAdjacency candidates(base.size());
  const int width = std::max(1, max_degree * 4);
  for (int i = 0; i < static_cast<int>(base.size()); ++i) {
    std::vector<std::pair<float, int>> scored;
    scored.reserve(base.size() > 0 ? base.size() - 1 : 0);
    for (int j = 0; j < static_cast<int>(base.size()); ++j) {
      if (i == j) continue;
      scored.push_back({MetricScore(base[i], base[j], metric), j});
    }
    const int keep = std::min(width, static_cast<int>(scored.size()));
    std::partial_sort(scored.begin(), scored.begin() + keep, scored.end());
    candidates[i].reserve(keep);
    for (int j = 0; j < keep; ++j) candidates[i].push_back(scored[j].second);
  }
  return candidates;
}

Metrics EvaluateGraph(const char* mode, const Matrix& base, const Matrix& queries,
                      const std::vector<std::vector<int>>& truth, const Graph& graph,
                      const VamanaSearchParams& params, MetricKind metric, double build_sec) {
  Metrics m;
  m.mode = mode;
  m.build_sec = build_sec;
  m.edges = graph.EdgeCount();

  Timer tquery;
  std::vector<std::vector<int>> pred;
  pred.reserve(queries.size());
  for (const auto& q : queries) pred.push_back(SearchVamanaGraph(base, graph, q, params));
  m.qps = ComputeQps(queries.size(), tquery.Sec());

  if (!truth.empty()) {
    m.recall_at_10 = RecallAtK(truth, pred, params.topk);
  } else {
    std::vector<std::vector<int>> exact;
    exact.reserve(queries.size());
    for (const auto& q : queries) exact.push_back(ExactTopK(base, q, params.topk, metric));
    m.recall_at_10 = RecallAtK(exact, pred, params.topk);
  }
  return m;
}
}  // namespace

std::vector<int> ExactTopK(const Matrix& base, const Vec& query, int k, MetricKind metric) {
  std::vector<std::pair<float, int>> d;
  d.reserve(base.size());
  for (int i = 0; i < static_cast<int>(base.size()); ++i) {
    d.push_back({MetricScore(base[i], query, metric), i});
  }
  k = std::min(k, static_cast<int>(d.size()));
  std::partial_sort(d.begin(), d.begin() + k, d.end());
  std::vector<int> out;
  for (int i = 0; i < k; ++i) out.push_back(d[i].second);
  return out;
}

double RecallAtK(const std::vector<std::vector<int>>& truth, const std::vector<std::vector<int>>& pred,
                 int k) {
  if (truth.empty() || pred.empty()) return 0.0;
  double total = 0.0;
  for (std::size_t i = 0; i < truth.size(); ++i) {
    int hit = 0;
    for (int j = 0; j < k && j < static_cast<int>(pred[i].size()); ++j) {
      assert(j < static_cast<int>(pred[i].size()));
      for (int t = 0; t < k && t < static_cast<int>(truth[i].size()); ++t) {
        assert(t < static_cast<int>(truth[i].size()));
        if (pred[i][j] == truth[i][t]) {
          ++hit;
          break;
        }
      }
    }
    total += static_cast<double>(hit) / k;
  }
  return total / truth.size();
}

double ComputeQps(std::size_t query_count, double query_seconds) {
  return query_count == 0 ? 0.0 : query_count / std::max(1e-6, query_seconds);
}

Metrics RunBenchmark(const RunnerConfig& cfg, const Matrix& base, const Matrix& queries,
                     const std::vector<std::vector<int>>& truth, const PipnnBuildParams& build_params,
                     const SearchParams& search_params) {
  if (cfg.mode == "hnsw") {
    return RunHnswBaseline(base, queries, truth, search_params.topk, cfg.hnsw, cfg.metric);
  }

  VamanaSearchParams vamana_search;
  vamana_search.beam = search_params.beam;
  vamana_search.topk = search_params.topk;
  vamana_search.entry = search_params.entry;
  vamana_search.metric = cfg.metric;

  if (cfg.mode == "vamana") {
    Timer tbuild;
    auto candidates = BuildNativeVamanaCandidates(base, build_params.hashprune.max_degree, cfg.metric);
    VamanaRefineParams refine;
    refine.max_degree = build_params.hashprune.max_degree;
    refine.metric = cfg.metric;
    auto graph = RefineVamanaGraph(base, candidates, refine);
    return EvaluateGraph("vamana", base, queries, truth, graph, vamana_search, cfg.metric, tbuild.Sec());
  }

  if (cfg.mode == "pipnn_vamana") {
    Timer tbuild;
    PipnnCandidateParams candidate_params;
    candidate_params.rbc = build_params.rbc;
    candidate_params.replicas = build_params.replicas;
    candidate_params.leaf_k = build_params.leaf_k;
    candidate_params.bidirected = build_params.bidirected;
    candidate_params.candidate_cap = std::max(1, build_params.hashprune.max_degree * 8);
    candidate_params.metric = cfg.metric;
    auto candidates = BuildPipnnCandidates(base, candidate_params);
    VamanaRefineParams refine;
    refine.max_degree = build_params.hashprune.max_degree;
    refine.metric = cfg.metric;
    auto graph = RefineVamanaGraph(base, candidates, refine);
    return EvaluateGraph("pipnn_vamana", base, queries, truth, graph, vamana_search, cfg.metric,
                         tbuild.Sec());
  }

  Metrics m;
  m.mode = "pipnn";
  Timer tbuild;
  PipnnBuildStats build_stats;
  auto graph = BuildPipnnGraph(base, build_params, &build_stats);
  m.build_sec = tbuild.Sec();
  m.edges = graph.EdgeCount();
  if (std::getenv("PIPNN_PROFILE") != nullptr) {
    std::cout << "pipnn_profile partition_sec=" << build_stats.partition_sec
              << " leaf_knn_sec=" << build_stats.leaf_knn_sec
              << " prune_sec=" << build_stats.prune_sec << " leaves=" << build_stats.num_leaves
              << " candidate_edges=" << build_stats.candidate_edges
              << " rbc_assignment_total=" << build_stats.rbc_assignment_total
              << " rbc_points_with_overlap=" << build_stats.rbc_points_with_overlap
              << " rbc_max_membership=" << build_stats.rbc_max_membership
              << " rbc_min_leaf_size=" << build_stats.rbc_min_leaf_size
              << " rbc_max_leaf_size=" << build_stats.rbc_max_leaf_size
              << " rbc_fallback_chunk_splits=" << build_stats.rbc_fallback_chunk_splits
              << " prune_kept=" << build_stats.prune_kept
              << " prune_dropped=" << build_stats.prune_dropped
              << " prune_replaced=" << build_stats.prune_replaced
              << " prune_evicted=" << build_stats.prune_evicted
              << " prune_final_edges=" << build_stats.prune_final_edges << "\n";
  }

  Timer tquery;
  std::vector<std::vector<int>> pred;
  pred.reserve(queries.size());
  SearchParams metric_search = search_params;
  metric_search.metric = cfg.metric;
  for (const auto& q : queries) pred.push_back(SearchGraph(base, graph, q, metric_search));
  double qsec = tquery.Sec();
  m.qps = ComputeQps(queries.size(), qsec);

  if (!truth.empty()) {
    m.recall_at_10 = RecallAtK(truth, pred, 10);
  } else {
    std::vector<std::vector<int>> exact;
    exact.reserve(queries.size());
    for (const auto& q : queries) exact.push_back(ExactTopK(base, q, 10, cfg.metric));
    m.recall_at_10 = RecallAtK(exact, pred, 10);
  }
  return m;
}

std::string ToJson(const Metrics& m) {
  std::ostringstream oss;
  oss << "{\n"
      << "  \"mode\": \"" << m.mode << "\",\n"
      << "  \"build_sec\": " << m.build_sec << ",\n"
      << "  \"recall_at_10\": " << m.recall_at_10 << ",\n"
      << "  \"qps\": " << m.qps << ",\n"
      << "  \"edges\": " << m.edges << "\n"
      << "}\n";
  return oss.str();
}
}  // namespace pipnn
