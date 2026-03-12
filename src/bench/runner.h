#pragma once

#include "common/types.h"
#include "core/pipnn_builder.h"
#include "search/greedy_beam.h"

#include <string>
#include <vector>

namespace pipnn {
struct RunnerConfig {
  std::string mode = "pipnn";
  std::string dataset = "synthetic";
  std::string output = "results/metrics.json";
};

Metrics RunBenchmark(const RunnerConfig& cfg, const Matrix& base, const Matrix& queries,
                     const std::vector<std::vector<int>>& truth, const PipnnBuildParams& build_params,
                     const SearchParams& search_params);

std::string ToJson(const Metrics& m);
}  // namespace pipnn
