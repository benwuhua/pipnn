#include "core/leaf_knn_blocked.h"

#include "core/distance.h"

#include <Eigen/Dense>

#include <algorithm>
#include <array>
#include <cassert>

namespace pipnn {
namespace {
using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

float NormalizeDistance(float distance) {
  return (distance < 0.0f && distance > -1e-4f) ? 0.0f : distance;
}

int LeafSizeBucket(std::size_t leaf_size) {
  if (leaf_size < 128) return 0;
  if (leaf_size < 256) return 1;
  return 2;
}
}  // namespace

std::vector<Edge> BuildLeafKnnExactEdgesNaive(const Matrix& points, const std::vector<int>& leaf, int k,
                                              bool bidirected) {
  std::vector<Edge> edges;
  const std::size_t approx = static_cast<std::size_t>(leaf.size()) *
                             static_cast<std::size_t>(std::max(1, k)) *
                             (bidirected ? 2 : 1);
  edges.reserve(approx);

  for (int idx = 0; idx < static_cast<int>(leaf.size()); ++idx) {
    const int u = leaf[static_cast<std::size_t>(idx)];
    std::vector<std::pair<float, int>> dists;
    dists.reserve(leaf.size() > 0 ? leaf.size() - 1 : 0);
    for (int v : leaf) {
      if (u == v) continue;
      dists.push_back({L2Squared(points[static_cast<std::size_t>(u)],
                                 points[static_cast<std::size_t>(v)]),
                       v});
    }
    const int kk = std::min(k, static_cast<int>(dists.size()));
    std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
    for (int i = 0; i < kk; ++i) {
      const int v = dists[static_cast<std::size_t>(i)].second;
      edges.push_back({u, v});
      if (bidirected) edges.push_back({v, u});
    }
  }
  return edges;
}

std::vector<LeafBatchPlan> PlanLeafKnnBatches(const std::vector<LeafBatchJob>& jobs,
                                              const LeafBatchConfig& cfg) {
  std::array<std::vector<int>, 3> buckets;
  for (int idx = 0; idx < static_cast<int>(jobs.size()); ++idx) {
    const auto leaf_size = jobs[static_cast<std::size_t>(idx)].leaf.size();
    if (static_cast<int>(leaf_size) < cfg.min_leaf_for_batch) continue;
    buckets[static_cast<std::size_t>(LeafSizeBucket(leaf_size))].push_back(idx);
  }

  std::vector<LeafBatchPlan> plans;
  for (const auto& bucket : buckets) {
    LeafBatchPlan current;
    for (int idx : bucket) {
      const int leaf_points =
          static_cast<int>(jobs[static_cast<std::size_t>(idx)].leaf.size());
      const bool exceeds_budget =
          !current.job_indices.empty() &&
          current.total_points + leaf_points > cfg.max_points_per_batch;
      if (exceeds_budget) {
        plans.push_back(current);
        current = {};
      }
      current.job_indices.push_back(idx);
      current.total_points += leaf_points;
    }
    if (!current.job_indices.empty()) plans.push_back(current);
  }
  return plans;
}

std::vector<Edge> BuildLeafKnnExactBatchedEdges(const Matrix& points, const std::vector<LeafBatchJob>& jobs,
                                                int k, bool bidirected, const LeafBatchConfig& cfg) {
  if (k <= 0) return {};

  std::vector<Edge> edges;
  std::size_t approx = 0;
  for (const auto& job : jobs) {
    approx += static_cast<std::size_t>(job.leaf.size()) *
              static_cast<std::size_t>(std::max(1, k)) *
              (bidirected ? 2u : 1u);
  }
  edges.reserve(approx);

  for (const auto& job : jobs) {
    if (static_cast<int>(job.leaf.size()) >= cfg.min_leaf_for_batch) continue;
    auto leaf_edges = BuildLeafKnnExactEdgesNaive(points, job.leaf, k, bidirected);
    edges.insert(edges.end(), leaf_edges.begin(), leaf_edges.end());
  }

  const auto plans = PlanLeafKnnBatches(jobs, cfg);
  for (const auto& plan : plans) {
    if (plan.job_indices.empty() || plan.total_points <= 0) continue;

    struct LeafMeta {
      int offset;
      int size;
      const std::vector<int>* leaf;
    };

    const int rows = plan.total_points;
    const auto& first_leaf =
        jobs[static_cast<std::size_t>(plan.job_indices.front())].leaf;
    if (first_leaf.empty()) continue;
    const auto dim =
        static_cast<Eigen::Index>(points[static_cast<std::size_t>(first_leaf.front())].size());
    RowMajorMatrixXf matrix(static_cast<Eigen::Index>(rows), dim);
    Eigen::VectorXf norms(static_cast<Eigen::Index>(rows));
    std::vector<int> row_to_global(static_cast<std::size_t>(rows));
    std::vector<int> row_to_meta(static_cast<std::size_t>(rows), -1);
    std::vector<LeafMeta> metas;
    metas.reserve(plan.job_indices.size());

    int cursor = 0;
    for (int job_index : plan.job_indices) {
      const auto& leaf = jobs[static_cast<std::size_t>(job_index)].leaf;
      if (leaf.empty()) continue;
      metas.push_back({cursor, static_cast<int>(leaf.size()), &leaf});
      const int meta_index = static_cast<int>(metas.size()) - 1;
      for (int local = 0; local < static_cast<int>(leaf.size()); ++local) {
        const int row = cursor + local;
        const int point_id = leaf[static_cast<std::size_t>(local)];
        const auto& point = points[static_cast<std::size_t>(point_id)];
        row_to_global[static_cast<std::size_t>(row)] = point_id;
        row_to_meta[static_cast<std::size_t>(row)] = meta_index;
        for (Eigen::Index col = 0; col < dim; ++col) {
          matrix(static_cast<Eigen::Index>(row), col) = point[static_cast<std::size_t>(col)];
        }
        norms(static_cast<Eigen::Index>(row)) =
            matrix.row(static_cast<Eigen::Index>(row)).squaredNorm();
      }
      cursor += static_cast<int>(leaf.size());
    }

    const Eigen::Index block_rows =
        static_cast<Eigen::Index>(std::max(1, cfg.point_block_rows));
    const Eigen::Index total_rows = static_cast<Eigen::Index>(cursor);
    for (Eigen::Index start = 0; start < total_rows; start += block_rows) {
      const Eigen::Index block = std::min(block_rows, total_rows - start);
      const Eigen::MatrixXf dots = matrix.middleRows(start, block) * matrix.topRows(total_rows).transpose();
      for (Eigen::Index local_row = 0; local_row < block; ++local_row) {
        const int row = static_cast<int>(start + local_row);
        const int meta_index = row_to_meta[static_cast<std::size_t>(row)];
        assert(meta_index >= 0);
        const auto& meta = metas[static_cast<std::size_t>(meta_index)];
        std::vector<std::pair<float, int>> dists;
        dists.reserve(static_cast<std::size_t>(std::max(0, meta.size - 1)));
        for (int col = meta.offset; col < meta.offset + meta.size; ++col) {
          if (row == col) continue;
          const float distance =
              NormalizeDistance(norms(static_cast<Eigen::Index>(row)) +
                                norms(static_cast<Eigen::Index>(col)) -
                                2.0f * dots(local_row, static_cast<Eigen::Index>(col)));
          dists.push_back({distance, row_to_global[static_cast<std::size_t>(col)]});
        }
        const int kk = std::min(k, static_cast<int>(dists.size()));
        std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
        const int u = row_to_global[static_cast<std::size_t>(row)];
        for (int i = 0; i < kk; ++i) {
          const int v = dists[static_cast<std::size_t>(i)].second;
          edges.push_back({u, v});
          if (bidirected) edges.push_back({v, u});
        }
      }
    }
  }

  return edges;
}

namespace {
std::vector<Edge> BuildLeafKnnExactEdgesBlocked(const Matrix& points, const std::vector<int>& leaf, int k,
                                                bool bidirected, const LeafKnnConfig& cfg) {
  LeafBatchConfig batch_cfg;
  batch_cfg.min_leaf_for_batch = cfg.min_leaf_for_blocked;
  batch_cfg.point_block_rows = cfg.point_block_rows;
  batch_cfg.max_points_per_batch = std::max(cfg.min_leaf_for_blocked, static_cast<int>(leaf.size()));
  return BuildLeafKnnExactBatchedEdges(points, {LeafBatchJob{leaf}}, k, bidirected, batch_cfg);
}
}  // namespace

LeafKnnMode SelectLeafKnnMode(std::size_t leaf_size, int scan_cap, const LeafKnnConfig& cfg) {
  if (scan_cap > 0) return LeafKnnMode::NaiveFull;
  if (static_cast<int>(leaf_size) >= cfg.min_leaf_for_blocked) {
    return LeafKnnMode::BlockedFull;
  }
  return LeafKnnMode::NaiveFull;
}

std::vector<Edge> BuildLeafKnnExactEdges(const Matrix& points, const std::vector<int>& leaf, int k,
                                         bool bidirected, LeafKnnMode mode,
                                         const LeafKnnConfig& cfg) {
  if (leaf.empty() || k <= 0) return {};
  switch (mode) {
    case LeafKnnMode::NaiveFull:
      return BuildLeafKnnExactEdgesNaive(points, leaf, k, bidirected);
    case LeafKnnMode::BlockedFull:
      return BuildLeafKnnExactEdgesBlocked(points, leaf, k, bidirected, cfg);
  }
  return {};
}
}  // namespace pipnn
