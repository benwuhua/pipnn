#include "bench/runner.h"

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

// [unit] exact metric helper coverage for runner.cpp
// [no integration test] pure computation helpers, no external I/O

int main() {
  pipnn::Matrix base = {{2.0f}, {0.0f}, {1.0f}};
  auto topk = pipnn::ExactTopK(base, pipnn::Vec{0.0f}, 5);
  assert(topk.size() == 3);
  assert(topk[0] == 1);
  assert(topk[1] == 2);
  assert(topk[2] == 0);

  assert(pipnn::RecallAtK({}, {{1}}, 10) == 0.0);
  assert(pipnn::RecallAtK({{1}}, {}, 10) == 0.0);

  std::vector<std::vector<int>> truth = {{1, 2, 3}, {4, 5, 6}};
  std::vector<std::vector<int>> pred = {{1, 4, 2}, {6, 9, 4}};
  double recall = pipnn::RecallAtK(truth, pred, 3);
  assert(recall > 0.66 && recall < 0.67);

  double sparse_recall = pipnn::RecallAtK({{7}}, {{7}}, 10);
  assert(sparse_recall == 0.1);

  assert(pipnn::ComputeQps(0, 0.25) == 0.0);
  assert(pipnn::ComputeQps(5, 2.0) == 2.5);
  assert(pipnn::ComputeQps(5, 0.0) == 5000000.0);

  pipnn::Matrix rb_base = {{0.0f}, {1.0f}, {2.0f}, {3.0f}};
  pipnn::Matrix rb_queries = {{0.1f}, {2.9f}};
  pipnn::PipnnBuildParams bp;
  bp.rbc.cmax = 2;
  bp.rbc.fanout = 1;
  bp.leaf_k = 1;
  bp.hashprune.max_degree = 2;
  pipnn::SearchParams sp;
  pipnn::RunnerConfig cfg;
  cfg.mode = "pipnn";

  {
    ScopedEnv clear_profile("PIPNN_PROFILE", nullptr);
    ScopedCoutCapture capture;
    auto metrics = pipnn::RunBenchmark(cfg, rb_base, rb_queries, {}, bp, sp);
    assert(metrics.edges > 0);
    assert(capture.stream.str().empty());
  }

  {
    ScopedEnv enable_profile("PIPNN_PROFILE", "1");
    ScopedCoutCapture capture;
    auto metrics = pipnn::RunBenchmark(cfg, rb_base, rb_queries, {}, bp, sp);
    assert(metrics.edges > 0);
    assert(capture.stream.str().find("pipnn_profile partition_sec=") != std::string::npos);
  }
  return 0;
}
