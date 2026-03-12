#pragma once

#include <cstddef>
#include <vector>

namespace pipnn {
using Id = int;
using Vec = std::vector<float>;
using Matrix = std::vector<Vec>;

struct Metrics {
  double build_sec = 0.0;
  double recall_at_10 = 0.0;
  double qps = 0.0;
  std::size_t edges = 0;
  const char* mode = "";
};
}  // namespace pipnn
