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


def read_score(path: Path) -> float:
    if not path.exists():
        fail(f"missing mutation score artifact: {path}")
    try:
        return float(path.read_text().strip())
    except ValueError as exc:
        fail(f"invalid mutation score artifact {path}: {exc}")


def read_report(path: Path) -> dict:
    if not path.exists():
        fail(f"missing mutation report artifact: {path}")
    try:
        return json.loads(path.read_text())
    except json.JSONDecodeError as exc:
        fail(f"invalid mutation report artifact {path}: {exc}")


def validate_scored_state(st_report: str, mutation: dict, score_path: Path, report_path: Path,
                          survivors_path: Path, min_score: float) -> None:
    score_path_manifest = mutation.get("score_path")
    report_path_manifest = mutation.get("report")
    survivors_path_manifest = mutation.get("survivors")
    score = mutation.get("score")
    tool_versions = mutation.get("tool_versions", {})

    if score is None:
        fail("mutation score missing from reproducibility manifest")
    if score_path_manifest is None or report_path_manifest is None or survivors_path_manifest is None:
        fail("mutation report paths missing from reproducibility manifest")
    if not tool_versions.get("llvm") or not tool_versions.get("mull"):
        fail("mutation tool versions missing from reproducibility manifest")

    if Path(score_path_manifest).name != score_path.name:
        fail("mutation score path mismatch between manifest and validator input")
    if Path(report_path_manifest).name != report_path.name:
        fail("mutation report path mismatch between manifest and validator input")
    if Path(survivors_path_manifest).name != survivors_path.name:
        fail("mutation survivors path mismatch between manifest and validator input")

    if not survivors_path.exists():
        fail(f"missing mutation survivors artifact: {survivors_path}")

    score_value = read_score(score_path)
    report_payload = read_report(report_path)
    report_totals = report_payload.get("totals", {})
    report_score = report_totals.get("mutation_score")
    if report_score is None:
        fail("mutation report missing totals.mutation_score")
    if float(score) != score_value or float(score) != float(report_score):
        fail("mutation score mismatch across manifest, score artifact, and report artifact")
    if float(score) < min_score:
        fail(f"mutation score {float(score):.1f} is below threshold {min_score:.1f}")

    score_name = score_path.name
    report_name = report_path.name
    survivors_name = survivors_path.name
    report_text = st_report.lower()
    if "mutation score" not in report_text:
        fail("mutation score missing from ST report")
    if str(score) not in st_report:
        fail("mutation score value missing from ST report")
    if score_name not in st_report or report_name not in st_report or survivors_name not in st_report:
        fail("mutation report paths missing from ST report")


def validate_blocked_state(st_report: str, mutation: dict) -> None:
    if (
        "blocked-state evidence" not in st_report
        or "mutation_probe_local.txt" not in st_report
        or "mutation_probe_remote.txt" not in st_report
    ):
        fail("blocked mutation evidence missing from ST report")
    if "Conditional-Go" not in st_report and "No-Go" not in st_report:
        fail("blocked mutation evidence missing release disposition in ST report")
    if not mutation.get("local_probe") or not mutation.get("remote_probe") or not mutation.get("follow_up"):
        fail("blocked mutation evidence missing from reproducibility manifest")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--local-probe", default="results/st/mutation_probe_local.txt")
    parser.add_argument("--remote-probe", default="results/st/remote/mutation_probe_remote.txt")
    parser.add_argument("--st-report", default="docs/plans/2026-03-12-st-report.md")
    parser.add_argument("--repro-manifest", default="results/repro_manifest.json")
    parser.add_argument("--mutation-score", default="results/st/mutation_score.txt")
    parser.add_argument("--mutation-report", default="results/st/mutation_report.json")
    parser.add_argument("--mutation-survivors", default="results/st/mutation_survivors.txt")
    parser.add_argument("--min-score", type=float, default=80.0)
    args = parser.parse_args()

    st_report = Path(args.st_report).read_text()
    manifest = json.loads(Path(args.repro_manifest).read_text())
    quality = manifest.get("quality_evidence", {})
    mutation = quality.get("mutation_probe", {})
    overall_status = mutation.get("status")
    if overall_status not in {"blocked", "scored"}:
        fail(f"unsupported mutation manifest status: {overall_status}")

    if overall_status == "blocked":
        local_status = parse_status(Path(args.local_probe))
        remote_status = parse_status(Path(args.remote_probe))
        if "blocked" not in {local_status, remote_status}:
            fail("blocked mutation evidence requires at least one blocked probe status")
        validate_blocked_state(st_report, mutation)
    else:
        local_status = "n/a"
        remote_status = "n/a"
        validate_scored_state(st_report, mutation, Path(args.mutation_score), Path(args.mutation_report),
                              Path(args.mutation_survivors), args.min_score)

    print(f"mutation_status={overall_status}")
    print(f"local_status={local_status}")
    print(f"remote_status={remote_status}")


if __name__ == "__main__":
    main()
