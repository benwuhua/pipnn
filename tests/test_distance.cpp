#include "core/distance.h"

#include <cassert>
#include <stdexcept>

// [unit] exact distance arithmetic coverage for core/distance.h
// [no integration test] pure computation helper, no external I/O

int main() {
  float exact = pipnn::L2Squared(pipnn::Vec{1.0f, 2.0f, 3.0f}, pipnn::Vec{4.0f, 6.0f, 8.0f});
  assert(exact == 50.0f);

  float zero = pipnn::L2Squared(pipnn::Vec{0.0f, 0.0f}, pipnn::Vec{0.0f, 0.0f});
  assert(zero == 0.0f);

  bool threw = false;
  try {
    (void)pipnn::L2Squared(pipnn::Vec{1.0f, 2.0f}, pipnn::Vec{1.0f});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
