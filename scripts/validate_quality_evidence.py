#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path


TOTAL_RE = re.compile(r"^TOTAL\s+\d+\s+\d+\s+(\d+)%\s*$")


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    sys.exit(1)


def parse_total_percent(path: Path) -> int:
    if not path.exists():
        fail(f"missing coverage report: {path}")
    for line in path.read_text().splitlines():
        match = TOTAL_RE.match(line.strip())
        if match:
            return int(match.group(1))
    fail(f"missing TOTAL row in coverage report: {path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--feature-list", default="feature-list.json")
    parser.add_argument("--line-report", default="results/st/line_coverage.txt")
    parser.add_argument("--branch-report", default="results/st/branch_coverage.txt")
    args = parser.parse_args()

    feature_list = Path(args.feature_list)
    data = json.loads(feature_list.read_text())
    gates = data["quality_gates"]

    line_percent = parse_total_percent(Path(args.line_report))
    branch_percent = parse_total_percent(Path(args.branch_report))

    line_min = int(gates["line_coverage_min"])
    branch_min = int(gates["branch_coverage_min"])

    if line_percent < line_min:
        fail(f"line coverage {line_percent} is below required {line_min}")
    if branch_percent < branch_min:
        fail(f"branch coverage {branch_percent} is below required {branch_min}")

    print(f"line_coverage={line_percent}")
    print(f"branch_coverage={branch_percent}")


if __name__ == "__main__":
    main()
