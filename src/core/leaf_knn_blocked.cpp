#include "core/leaf_knn_blocked.h"

#include "core/distance.h"

#include <Eigen/Dense>

#include <algorithm>

namespace pipnn {
namespace {
using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

float NormalizeDistance(float distance) {
  return (distance < 0.0f && distance > -1e-4f) ? 0.0f : distance;
}

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

std::vector<Edge> BuildLeafKnnExactEdgesBlocked(const Matrix& points, const std::vector<int>& leaf, int k,
                                                bool bidirected, const LeafKnnConfig& cfg) {
  if (leaf.empty() || k <= 0) return {};

  const auto dim = static_cast<Eigen::Index>(points[static_cast<std::size_t>(leaf.front())].size());
  const Eigen::Index rows = static_cast<Eigen::Index>(leaf.size());
  RowMajorMatrixXf matrix(rows, dim);
  Eigen::VectorXf norms(rows);
  for (Eigen::Index row = 0; row < rows; ++row) {
    const auto& point = points[static_cast<std::size_t>(leaf[static_cast<std::size_t>(row)])];
    for (Eigen::Index col = 0; col < dim; ++col) {
      matrix(row, col) = point[static_cast<std::size_t>(col)];
    }
    norms(row) = matrix.row(row).squaredNorm();
  }

  std::vector<Edge> edges;
  const std::size_t approx = static_cast<std::size_t>(leaf.size()) *
                             static_cast<std::size_t>(std::max(1, k)) *
                             (bidirected ? 2 : 1);
  edges.reserve(approx);

  const Eigen::Index block_rows =
      static_cast<Eigen::Index>(std::max(1, cfg.point_block_rows));
  for (Eigen::Index start = 0; start < rows; start += block_rows) {
    const Eigen::Index block = std::min(block_rows, rows - start);
    const Eigen::MatrixXf dots = matrix.middleRows(start, block) * matrix.transpose();
    for (Eigen::Index local_row = 0; local_row < block; ++local_row) {
      const Eigen::Index row = start + local_row;
      std::vector<std::pair<float, int>> dists;
      dists.reserve(static_cast<std::size_t>(rows > 0 ? rows - 1 : 0));
      for (Eigen::Index col = 0; col < rows; ++col) {
        if (row == col) continue;
        const float distance =
            NormalizeDistance(norms(row) + norms(col) - 2.0f * dots(local_row, col));
        dists.push_back({distance, leaf[static_cast<std::size_t>(col)]});
      }
      const int kk = std::min(k, static_cast<int>(dists.size()));
      std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
      const int u = leaf[static_cast<std::size_t>(row)];
      for (int i = 0; i < kk; ++i) {
        const int v = dists[static_cast<std::size_t>(i)].second;
        edges.push_back({u, v});
        if (bidirected) edges.push_back({v, u});
      }
    }
  }
  return edges;
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
