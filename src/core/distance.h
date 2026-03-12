#pragma once

#include "common/types.h"

#include <cmath>
#include <stdexcept>

namespace pipnn {
inline float L2Squared(const Vec& a, const Vec& b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("dimension mismatch");
  }
  float sum = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    float d = a[i] - b[i];
    sum += d * d;
  }
  return sum;
}
}  // namespace pipnn
