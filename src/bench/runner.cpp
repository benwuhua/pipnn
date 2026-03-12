#include "bench/runner.h"

#include "baseline/hnsw_runner.h"
#include "common/timer.h"
#include "core/distance.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace pipnn {
std::vector<int> ExactTopK(const Matrix& base, const Vec& query, int k) {
  std::vector<std::pair<float, int>> d;
  d.reserve(base.size());
  for (int i = 0; i < static_cast<int>(base.size()); ++i) d.push_back({L2Squared(base[i], query), i});
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
  if (cfg.mode == "hnsw") return RunHnswBaseline(base, queries, truth, search_params.topk);

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
              << " candidate_edges=" << build_stats.candidate_edges << "\n";
  }

  Timer tquery;
  std::vector<std::vector<int>> pred;
  pred.reserve(queries.size());
  for (const auto& q : queries) pred.push_back(SearchGraph(base, graph, q, search_params));
  double qsec = tquery.Sec();
  m.qps = ComputeQps(queries.size(), qsec);

  if (!truth.empty()) {
    m.recall_at_10 = RecallAtK(truth, pred, 10);
  } else {
    std::vector<std::vector<int>> exact;
    exact.reserve(queries.size());
    for (const auto& q : queries) exact.push_back(ExactTopK(base, q, 10));
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
