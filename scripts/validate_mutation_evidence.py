#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path


STATUS_RE = re.compile(r"^status=(available|blocked)\s*$")


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    sys.exit(1)


def parse_status(path: Path) -> str:
    if not path.exists():
      fail(f"missing mutation probe log: {path}")
    for line in path.read_text().splitlines():
        match = STATUS_RE.match(line.strip())
        if match:
            return match.group(1)
    fail(f"missing status in mutation probe log: {path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--local-probe", default="results/st/mutation_probe_local.txt")
    parser.add_argument("--remote-probe", default="results/st/remote/mutation_probe_remote.txt")
    parser.add_argument("--st-report", default="docs/plans/2026-03-12-st-report.md")
    parser.add_argument("--repro-manifest", default="results/repro_manifest.json")
    args = parser.parse_args()

    local_status = parse_status(Path(args.local_probe))
    remote_status = parse_status(Path(args.remote_probe))
    overall_status = "blocked" if "blocked" in {local_status, remote_status} else "available"

    st_report = Path(args.st_report).read_text()
    manifest = json.loads(Path(args.repro_manifest).read_text())
    quality = manifest.get("quality_evidence", {})
    mutation = quality.get("mutation_probe", {})

    if mutation.get("status") != overall_status:
        fail(f"mutation manifest status {mutation.get('status')} does not match probe status {overall_status}")

    if overall_status == "blocked":
        if "blocked-state evidence" not in st_report or "mutation_probe_local.txt" not in st_report or "mutation_probe_remote.txt" not in st_report:
            fail("blocked mutation evidence missing from ST report")
        if "Conditional-Go" not in st_report and "No-Go" not in st_report:
            fail("blocked mutation evidence missing release disposition in ST report")
        if not mutation.get("local_probe") or not mutation.get("remote_probe") or not mutation.get("follow_up"):
            fail("blocked mutation evidence missing from reproducibility manifest")
    else:
        if "mutation score" not in st_report:
            fail("mutation score missing from ST report")
        if mutation.get("score") is None:
            fail("mutation score missing from reproducibility manifest")

    print(f"mutation_status={overall_status}")
    print(f"local_status={local_status}")
    print(f"remote_status={remote_status}")


if __name__ == "__main__":
    main()
