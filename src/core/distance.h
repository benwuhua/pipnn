#pragma once

#include "common/types.h"

#include <cassert>
#include <cmath>
#include <stdexcept>

namespace pipnn {
enum class MetricKind { L2, InnerProduct };

inline float L2Squared(const Vec& a, const Vec& b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("dimension mismatch");
  }
  float sum = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    assert(i < a.size());
    assert(i < b.size());
    float d = a[i] - b[i];
    sum += d * d;
  }
  return sum;
}

inline float DotProduct(const Vec& a, const Vec& b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("dimension mismatch");
  }
  float sum = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    assert(i < a.size());
    assert(i < b.size());
    sum += a[i] * b[i];
  }
  return sum;
}

inline float MetricScore(const Vec& a, const Vec& b, MetricKind metric) {
  switch (metric) {
    case MetricKind::L2:
      return L2Squared(a, b);
    case MetricKind::InnerProduct:
      return -DotProduct(a, b);
  }
  throw std::invalid_argument("unknown metric");
}
}  // namespace pipnn
