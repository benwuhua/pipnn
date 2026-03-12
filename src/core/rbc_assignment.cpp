#include "core/rbc_assignment.h"

#include "core/distance.h"

#include <Eigen/Dense>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace pipnn {
namespace {
using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

float NormalizeDistance(float distance) {
  return (distance < 0.0f && distance > -1e-4f) ? 0.0f : distance;
}

std::vector<int> AssignPointsToLeadersScalar(const Matrix& points, const std::vector<int>& ids,
                                             const std::vector<int>& leaders) {
  if (!ids.empty() && leaders.empty()) {
    throw std::invalid_argument("leaders must not be empty");
  }
  std::vector<int> assignments;
  assignments.reserve(ids.size());
  for (int point_id : ids) {
    float best_distance = std::numeric_limits<float>::max();
    int best_index = 0;
    for (int i = 0; i < static_cast<int>(leaders.size()); ++i) {
      const float distance = L2Squared(points[static_cast<std::size_t>(point_id)],
                                       points[static_cast<std::size_t>(leaders[static_cast<std::size_t>(i)])]);
      if (distance < best_distance ||
          (distance == best_distance &&
           leaders[static_cast<std::size_t>(i)] < leaders[static_cast<std::size_t>(best_index)])) {
        best_distance = distance;
        best_index = i;
      }
    }
    assignments.push_back(best_index);
  }
  return assignments;
}

std::vector<int> AssignPointsToLeadersBlocked(const Matrix& points, const std::vector<int>& ids,
                                              const std::vector<int>& leaders,
                                              const RbcAssignConfig& cfg) {
  if (!ids.empty() && leaders.empty()) {
    throw std::invalid_argument("leaders must not be empty");
  }
  if (ids.empty()) return {};

  const auto dim = static_cast<Eigen::Index>(points[static_cast<std::size_t>(ids.front())].size());
  RowMajorMatrixXf leader_matrix(static_cast<Eigen::Index>(leaders.size()), dim);
  Eigen::VectorXf leader_norms(static_cast<Eigen::Index>(leaders.size()));

  for (Eigen::Index row = 0; row < static_cast<Eigen::Index>(leaders.size()); ++row) {
    const auto& leader = points[static_cast<std::size_t>(leaders[static_cast<std::size_t>(row)])];
    if (static_cast<Eigen::Index>(leader.size()) != dim) {
      throw std::invalid_argument("dimension mismatch");
    }
    for (Eigen::Index col = 0; col < dim; ++col) {
      leader_matrix(row, col) = leader[static_cast<std::size_t>(col)];
    }
    leader_norms(row) = leader_matrix.row(row).squaredNorm();
  }

  std::vector<int> assignments(ids.size(), 0);
  const Eigen::Index block_rows =
      static_cast<Eigen::Index>(std::max(1, cfg.point_block_rows));

  for (Eigen::Index start = 0; start < static_cast<Eigen::Index>(ids.size()); start += block_rows) {
    const Eigen::Index rows =
        std::min(block_rows, static_cast<Eigen::Index>(ids.size()) - start);
    RowMajorMatrixXf point_matrix(rows, dim);
    Eigen::VectorXf point_norms(rows);
    for (Eigen::Index row = 0; row < rows; ++row) {
      const auto& point = points[static_cast<std::size_t>(
          ids[static_cast<std::size_t>(start + row)])];
      if (static_cast<Eigen::Index>(point.size()) != dim) {
        throw std::invalid_argument("dimension mismatch");
      }
      for (Eigen::Index col = 0; col < dim; ++col) {
        point_matrix(row, col) = point[static_cast<std::size_t>(col)];
      }
      point_norms(row) = point_matrix.row(row).squaredNorm();
    }

    const Eigen::MatrixXf dots = point_matrix * leader_matrix.transpose();
    for (Eigen::Index row = 0; row < rows; ++row) {
      float best_distance = std::numeric_limits<float>::max();
      int best_index = 0;
      for (Eigen::Index leader_index = 0;
           leader_index < static_cast<Eigen::Index>(leaders.size()); ++leader_index) {
        const float distance = NormalizeDistance(
            point_norms(row) + leader_norms(leader_index) - 2.0f * dots(row, leader_index));
        if (distance < best_distance ||
            (distance == best_distance &&
             leaders[static_cast<std::size_t>(leader_index)] <
                 leaders[static_cast<std::size_t>(best_index)])) {
          best_distance = distance;
          best_index = static_cast<int>(leader_index);
        }
      }
      assignments[static_cast<std::size_t>(start + row)] = best_index;
    }
  }

  return assignments;
}
}  // namespace

RbcAssignMode SelectRbcAssignMode(std::size_t point_count, std::size_t leader_count,
                                  const RbcAssignConfig& cfg) {
  if (static_cast<int>(point_count) >= cfg.min_points_for_blocked &&
      static_cast<int>(leader_count) >= cfg.min_leaders_for_blocked) {
    return RbcAssignMode::Blocked;
  }
  return RbcAssignMode::Scalar;
}

std::vector<int> AssignPointsToLeaders(const Matrix& points, const std::vector<int>& ids,
                                       const std::vector<int>& leaders, RbcAssignMode mode,
                                       const RbcAssignConfig& cfg) {
  switch (mode) {
    case RbcAssignMode::Scalar:
      return AssignPointsToLeadersScalar(points, ids, leaders);
    case RbcAssignMode::Blocked:
      return AssignPointsToLeadersBlocked(points, ids, leaders, cfg);
  }
  throw std::invalid_argument("unknown rbc assignment mode");
}
}  // namespace pipnn
