#pragma once

#include "baseline/hnsw_runner.h"
#include "candidates/pipnn_candidate_generator.h"
#include "common/types.h"
#include "core/pipnn_builder.h"
#include "refine/vamana_refiner.h"
#include "search/greedy_beam.h"
#include "search/vamana_search_adapter.h"

#include <string>
#include <vector>

namespace pipnn {
struct RunnerConfig {
  std::string mode = "pipnn";
  std::string dataset = "synthetic";
  std::string output = "results/metrics.json";
  HnswParams hnsw;
};

std::vector<int> ExactTopK(const Matrix& base, const Vec& query, int k);
double RecallAtK(const std::vector<std::vector<int>>& truth, const std::vector<std::vector<int>>& pred, int k);
double ComputeQps(std::size_t query_count, double query_seconds);

Metrics RunBenchmark(const RunnerConfig& cfg, const Matrix& base, const Matrix& queries,
                     const std::vector<std::vector<int>>& truth, const PipnnBuildParams& build_params,
                     const SearchParams& search_params);

std::string ToJson(const Metrics& m);
}  // namespace pipnn
