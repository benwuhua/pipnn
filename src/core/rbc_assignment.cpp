#include "core/rbc_assignment.h"

#include "core/distance.h"

#include <limits>
#include <stdexcept>

namespace pipnn {
namespace {
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
}  // namespace

std::vector<int> AssignPointsToLeaders(const Matrix& points, const std::vector<int>& ids,
                                       const std::vector<int>& leaders, RbcAssignMode mode,
                                       const RbcAssignConfig& cfg) {
  (void)cfg;
  switch (mode) {
    case RbcAssignMode::Scalar:
      return AssignPointsToLeadersScalar(points, ids, leaders);
    case RbcAssignMode::Blocked:
      return AssignPointsToLeadersScalar(points, ids, leaders);
  }
  throw std::invalid_argument("unknown rbc assignment mode");
}
}  // namespace pipnn
