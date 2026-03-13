#include "baseline/hnsw_runner.h"

#include <cassert>
#include <string_view>

int main() {
  pipnn::Metrics empty = pipnn::RunHnswBaseline({}, {}, {}, 10);
  assert(std::string_view(empty.mode) == "hnsw");
  assert(empty.build_sec == 0.0);
  assert(empty.edges == 0);

  pipnn::Matrix base = {
      {0.0f, 0.0f},
      {1.0f, 0.0f},
      {0.0f, 1.0f},
      {2.0f, 2.0f},
  };
  pipnn::Matrix queries = {
      {0.1f, 0.0f},
      {1.8f, 1.9f},
  };
  std::vector<std::vector<int>> truth = {
      {0, 1},
      {3, 1},
  };

  auto with_truth = pipnn::RunHnswBaseline(base, queries, truth, 2);
  assert(std::string_view(with_truth.mode) == "hnsw");
  assert(with_truth.build_sec >= 0.0);
  assert(with_truth.qps > 0.0);
  assert(with_truth.edges == static_cast<int>(base.size()) * 32);
  assert(with_truth.recall_at_10 >= 0.0);
  assert(with_truth.recall_at_10 <= 1.0);

  auto without_truth = pipnn::RunHnswBaseline(base, queries, {}, 2);
  assert(std::string_view(without_truth.mode) == "hnsw");
  assert(without_truth.qps > 0.0);
  assert(without_truth.edges == static_cast<int>(base.size()) * 32);
  assert(without_truth.recall_at_10 >= 0.0);
  assert(without_truth.recall_at_10 <= 1.0);

  pipnn::HnswParams tuned;
  tuned.m = 16;
  tuned.ef_construction = 128;
  tuned.ef_search = 32;
  auto tuned_metrics = pipnn::RunHnswBaseline(base, queries, truth, 2, tuned);
  assert(std::string_view(tuned_metrics.mode) == "hnsw");
  assert(tuned_metrics.edges == static_cast<int>(base.size()) * 16);
  assert(tuned_metrics.qps > 0.0);
  assert(tuned_metrics.recall_at_10 >= 0.0);
  assert(tuned_metrics.recall_at_10 <= 1.0);

  return 0;
}
