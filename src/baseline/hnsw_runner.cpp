#include "baseline/hnsw_runner.h"

#include "common/timer.h"
#include "core/distance.h"

#include "hnswlib/hnswlib.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <unordered_set>

namespace pipnn {
Metrics RunHnswBaseline(const Matrix& base, const Matrix& queries,
                        const std::vector<std::vector<int>>& truth, int topk,
                        const HnswParams& params, MetricKind metric) {
  Metrics m;
  m.mode = "hnsw";
  if (base.empty()) return m;

  const int dim = static_cast<int>(base.front().size());
  std::unique_ptr<hnswlib::SpaceInterface<float>> space;
  if (metric == MetricKind::InnerProduct) {
    space = std::make_unique<hnswlib::InnerProductSpace>(dim);
  } else {
    space = std::make_unique<hnswlib::L2Space>(dim);
  }

  const std::size_t max_elements = base.size();
  const std::size_t M = static_cast<std::size_t>(std::max(1, params.m));
  const std::size_t ef_construction =
      static_cast<std::size_t>(std::max(1, params.ef_construction));

  Timer tb;
  hnswlib::HierarchicalNSW<float> index(space.get(), max_elements, M, ef_construction);
  for (std::size_t i = 0; i < base.size(); ++i) {
    index.addPoint(base[i].data(), static_cast<hnswlib::labeltype>(i));
  }
  m.build_sec = tb.Sec();

  const int ef_search = params.ef_search > 0 ? params.ef_search : std::max(64, topk * 8);
  index.setEf(static_cast<std::size_t>(ef_search));

  Timer tq;
  std::vector<std::vector<int>> preds;
  preds.reserve(queries.size());
  for (std::size_t qi = 0; qi < queries.size(); ++qi) {
    auto pq = index.searchKnn(queries[qi].data(), static_cast<std::size_t>(topk));
    std::vector<int> pred;
    pred.reserve(topk);
    while (!pq.empty()) {
      pred.push_back(static_cast<int>(pq.top().second));
      pq.pop();
    }
    preds.push_back(std::move(pred));
  }
  m.qps = queries.empty() ? 0.0 : queries.size() / std::max(1e-6, tq.Sec());

  int total_hits = 0;
  int total_possible = 0;
  if (!truth.empty()) {
    for (std::size_t qi = 0; qi < preds.size() && qi < truth.size(); ++qi) {
      for (int p : preds[qi]) {
        for (int t : truth[qi]) {
          if (p == t) {
            ++total_hits;
            break;
          }
        }
      }
      total_possible += std::min(topk, static_cast<int>(truth[qi].size()));
    }
  } else {
    for (std::size_t qi = 0; qi < preds.size(); ++qi) {
      std::vector<std::pair<float, int>> exact;
      exact.reserve(base.size());
      for (int i = 0; i < static_cast<int>(base.size()); ++i) {
        exact.push_back({MetricScore(base[i], queries[qi], metric), i});
      }
      std::partial_sort(exact.begin(), exact.begin() + topk, exact.end());
      for (int p : preds[qi]) {
        for (int i = 0; i < topk; ++i) {
          if (p == exact[i].second) {
            ++total_hits;
            break;
          }
        }
      }
      total_possible += topk;
    }
  }
  m.recall_at_10 = total_possible == 0 ? 0.0 : static_cast<double>(total_hits) / total_possible;
  m.edges = base.size() * M;
  return m;
}
}  // namespace pipnn
