#include "core/hashprune.h"

#include "core/distance.h"

#include <algorithm>
#include <random>
#include <unordered_map>

namespace pipnn {
HashPruner::HashPruner(HashPruneParams params) : params_(params) {}
HashPruner::HashPruner(HashPruneParams params, Matrix manual_hyperplanes)
    : params_(params), hyperplanes_(std::move(manual_hyperplanes)), has_manual_hyperplanes_(true) {}

void HashPruner::EnsureHyperplanes(std::size_t dim) const {
  if (has_manual_hyperplanes_) return;
  std::call_once(init_once_, [&]() {
    std::mt19937 gen(params_.seed);
    std::normal_distribution<float> nd(0.0f, 1.0f);
    hyperplanes_.assign(params_.hash_bits, Vec(dim, 0.0f));
    for (auto& hp : hyperplanes_) {
      for (float& x : hp) x = nd(gen);
    }
  });
}

std::uint64_t HashPruner::HashResidual(const Vec& p, const Vec& c) const {
  std::uint64_t code = 0;
  for (int b = 0; b < params_.hash_bits; ++b) {
    float dot = 0.0f;
    const auto& hp = hyperplanes_[b];
    for (std::size_t i = 0; i < p.size(); ++i) {
      dot += hp[i] * (c[i] - p[i]);
    }
    if (dot >= 0) code |= (1ULL << b);
  }
  return code;
}

std::uint64_t HashPruner::HashResidualForTest(const Vec& p, const Vec& c) const {
  EnsureHyperplanes(p.size());
  return HashResidual(p, c);
}

std::vector<int> HashPruner::PruneNode(const Matrix& points, int p, const std::vector<int>& candidates,
                                       HashPruneNodeStats* stats) const {
  if (stats != nullptr) *stats = {};
  if (points.empty()) return {};
  EnsureHyperplanes(points[0].size());

  struct Entry {
    int id;
    float dist;
  };

  if (params_.max_degree <= 0) {
    if (stats != nullptr) {
      for (int c : candidates) {
        if (c == p) {
          ++stats->self_skipped;
        } else {
          ++stats->dropped;
        }
      }
    }
    return {};
  }

  std::unordered_map<std::uint64_t, Entry> buckets;
  buckets.reserve(candidates.size());

  for (int c : candidates) {
    if (c == p) {
      if (stats != nullptr) ++stats->self_skipped;
      continue;
    }
    float d = L2Squared(points[p], points[c]);
    auto h = HashResidual(points[p], points[c]);

    auto it = buckets.find(h);
    if (it != buckets.end()) {
      const auto& cur = it->second;
      if (d < cur.dist || (d == cur.dist && c < cur.id)) {
        it->second = Entry{c, d};
        if (stats != nullptr) {
          ++stats->kept;
          ++stats->replaced;
        }
      } else if (stats != nullptr) {
        ++stats->dropped;
      }
      continue;
    }

    if (static_cast<int>(buckets.size()) < params_.max_degree) {
      buckets[h] = Entry{c, d};
      if (stats != nullptr) ++stats->kept;
      continue;
    }

    auto far_it = buckets.begin();
    for (auto iter = buckets.begin(); iter != buckets.end(); ++iter) {
      if (iter->second.dist > far_it->second.dist ||
          (iter->second.dist == far_it->second.dist && iter->second.id > far_it->second.id)) {
        far_it = iter;
      }
    }

    if (d < far_it->second.dist || (d == far_it->second.dist && c < far_it->second.id)) {
      buckets.erase(far_it);
      buckets[h] = Entry{c, d};
      if (stats != nullptr) {
        ++stats->kept;
        ++stats->evicted;
      }
    } else if (stats != nullptr) {
      ++stats->dropped;
    }
  }

  std::vector<Entry> entries;
  entries.reserve(buckets.size());
  for (const auto& [_, e] : buckets) entries.push_back(e);
  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    if (a.dist != b.dist) return a.dist < b.dist;
    return a.id < b.id;
  });

  std::vector<int> out;
  out.reserve(entries.size());
  for (const auto& e : entries) out.push_back(e.id);
  if (stats != nullptr) stats->final_degree = out.size();
  return out;
}
}  // namespace pipnn
