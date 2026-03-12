#!/usr/bin/env bash

set -euo pipefail

cmake -E rm -rf build-cov
CXXFLAGS='--coverage -O0 -g' CFLAGS='--coverage -O0 -g' cmake -S . -B build-cov
cmake --build build-cov -j
ctest --test-dir build-cov --output-on-failure

cmake -E rm -rf results/st
mkdir -p results/st
python3 -m gcovr build-cov -r . \
  --merge-mode-functions=merge-use-line-max \
  --exclude 'build-cov/_deps/' \
  --exclude 'build-cov/CMakeFiles/.*/CompilerIdCXX/' \
  --exclude 'tests/' \
  --txt > results/st/line_coverage.txt

python3 -m gcovr build-cov -r . \
  --merge-mode-functions=merge-use-line-max \
  --exclude 'build-cov/_deps/' \
  --exclude 'build-cov/CMakeFiles/.*/CompilerIdCXX/' \
  --exclude 'tests/' \
  --exclude-throw-branches \
  --exclude-unreachable-branches \
  --txt --txt-metric branch > results/st/branch_coverage.txt
