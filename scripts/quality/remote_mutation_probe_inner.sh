#!/usr/bin/env bash

set -uo pipefail

cmake -E rm -rf results/st
mkdir -p results/st
{
  echo '$ mull-runner --help'
  if mull-runner --help; then
    echo 'status=available'
  else
    rc=$?
    echo "status=blocked"
    echo "exit_code=${rc}"
  fi
} > results/st/mutation_probe_remote.txt 2>&1 || true
