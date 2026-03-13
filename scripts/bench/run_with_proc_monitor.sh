#!/usr/bin/env bash
set -euo pipefail

SNAPSHOT_PATH=""
INTERVAL_SEC=30

while [[ $# -gt 0 ]]; do
  case "$1" in
    --snapshot)
      SNAPSHOT_PATH="$2"
      shift 2
      ;;
    --interval)
      INTERVAL_SEC="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -z "${SNAPSHOT_PATH}" ]]; then
  echo "missing --snapshot <path>" >&2
  exit 1
fi

if [[ $# -eq 0 ]]; then
  echo "usage: $0 --snapshot <path> [--interval sec] -- <command...>" >&2
  exit 1
fi

mkdir -p "$(dirname "${SNAPSHOT_PATH}")"

"$@" &
cmd_pid=$!

cleanup() {
  if [[ -n "${monitor_pid:-}" ]]; then
    kill "${monitor_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

(
  while kill -0 "${cmd_pid}" 2>/dev/null; do
    {
      echo "=== $(date -u +%Y-%m-%dT%H:%M:%SZ) pid=${cmd_pid} ==="
      ps -p "${cmd_pid}" -o pid=,etime=,pcpu=,pmem=,comm=
      if [[ -r "/proc/${cmd_pid}/status" ]]; then
        grep -E 'State|VmRSS|VmPeak|Threads' "/proc/${cmd_pid}/status" || true
      fi
      if [[ -r "/proc/${cmd_pid}/io" ]]; then
        cat "/proc/${cmd_pid}/io"
      fi
      echo
    } >>"${SNAPSHOT_PATH}"
    sleep "${INTERVAL_SEC}"
  done
  {
    echo "=== $(date -u +%Y-%m-%dT%H:%M:%SZ) pid=${cmd_pid} exited ==="
  } >>"${SNAPSHOT_PATH}"
) &
monitor_pid=$!

wait "${cmd_pid}"
cmd_rc=$?
wait "${monitor_pid}" 2>/dev/null || true
exit "${cmd_rc}"
