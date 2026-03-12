#include "core/leaf_candidates.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>

int main() {
  pipnn::Leaves leaves = {
      {0, 1, 2},
      {2, 3, 4},
      {0, 4, 5},
  };
  std::vector<std::vector<int>> point_memberships = {
      {0, 2},
      {0},
      {0, 1},
      {1},
      {1, 2},
      {2},
  };

  pipnn::ShortlistConfig cap_cfg;
  cap_cfg.candidate_cap = 3;
  auto capped_shortlist = pipnn::BuildShortlistForPoint(2, leaves, point_memberships, cap_cfg);
  assert((capped_shortlist == std::vector<int>{0, 1, 3}));
  auto repeated = pipnn::BuildShortlistForPoint(2, leaves, point_memberships, cap_cfg);
  assert(repeated == capped_shortlist);

  pipnn::ShortlistConfig full_cfg;
  full_cfg.candidate_cap = 0;
  auto full_shortlist = pipnn::BuildShortlistForPoint(2, leaves, point_memberships, full_cfg);
  assert((full_shortlist == std::vector<int>{0, 1, 3, 4}));
  assert(std::find(full_shortlist.begin(), full_shortlist.end(), 2) == full_shortlist.end());

  std::unordered_set<int> allowed = {0, 1, 3, 4};
  for (int candidate : full_shortlist) assert(allowed.count(candidate) == 1);
  assert(pipnn::BuildShortlistForPoint(-1, leaves, point_memberships).empty());
  assert(pipnn::BuildShortlistForPoint(99, leaves, point_memberships).empty());

  pipnn::Matrix points = {
      {0.0f, 0.0f},
      {1.0f, 0.0f},
      {2.0f, 0.0f},
      {4.0f, 0.0f},
  };
  std::vector<int> shortlist = {0, 2, 3};
  auto directed = pipnn::ScoreShortlistExact(points, 1, shortlist, 2, false);
  assert((directed == std::vector<pipnn::Edge>{{1, 0}, {1, 2}}));
  auto bidirected = pipnn::ScoreShortlistExact(points, 1, shortlist, 2, true);
  assert((bidirected == std::vector<pipnn::Edge>{{1, 0}, {0, 1}, {1, 2}, {2, 1}}));
  assert(pipnn::ScoreShortlistExact(points, 1, shortlist, 0, false).empty());

  return 0;
}
