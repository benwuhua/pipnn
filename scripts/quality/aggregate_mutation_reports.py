#!/usr/bin/env python3

import argparse
import json
import sys
from collections import Counter
from pathlib import Path


def fail(message: str) -> int:
  print(message, file=sys.stderr)
  return 1


def normalize_status(status: str) -> str:
  value = status.strip().lower()
  if value == "killed":
    return "killed"
  if value == "survived":
    return "survived"
  return value or "unknown"


def validate_report(path: Path, payload: object) -> dict:
  if not isinstance(payload, dict):
    raise ValueError(f"{path}: report root must be an object")

  files = payload.get("files")
  if not isinstance(files, dict) or not files:
    raise ValueError(f"{path}: missing files object")

  return files


def summarize_report(path: Path) -> tuple[dict, Counter, dict[str, Counter]]:
  payload = json.loads(path.read_text())
  files = validate_report(path, payload)

  report_counts: Counter = Counter()
  per_file: dict[str, Counter] = {}
  for file_path, entry in sorted(files.items()):
    if not isinstance(entry, dict):
      raise ValueError(f"{path}: file entry {file_path!r} must be an object")
    mutants = entry.get("mutants")
    if not isinstance(mutants, list):
      raise ValueError(f"{path}: file entry {file_path!r} missing mutants list")

    file_counts: Counter = Counter()
    for mutant in mutants:
      if not isinstance(mutant, dict):
        raise ValueError(f"{path}: mutant entry in {file_path!r} must be an object")
      status = mutant.get("status")
      if not isinstance(status, str) or not status.strip():
        raise ValueError(f"{path}: mutant entry in {file_path!r} missing status")
      normalized = normalize_status(status)
      report_counts["mutants_total"] += 1
      report_counts[normalized] += 1
      file_counts["mutants_total"] += 1
      file_counts[normalized] += 1

    per_file[file_path] = file_counts

  killed = report_counts.get("killed", 0)
  total = report_counts.get("mutants_total", 0)
  mutation_score = round((killed / total) * 100.0, 1) if total else 0.0

  summary = {
      "report": path.name,
      "mutants_total": total,
      "killed": killed,
      "survived": report_counts.get("survived", 0),
      "other": total - killed - report_counts.get("survived", 0),
      "mutation_score": mutation_score,
      "files": sorted(per_file.keys()),
  }
  return summary, report_counts, per_file


def counter_to_json(counter: Counter) -> dict[str, int]:
  return {key: counter[key] for key in sorted(counter.keys())}


def main() -> int:
  parser = argparse.ArgumentParser(description="Aggregate raw mutation report JSON files.")
  parser.add_argument("--input-dir", required=True, help="Directory containing raw report JSON files")
  parser.add_argument("--output", required=True, help="Output JSON summary path")
  args = parser.parse_args()

  input_dir = Path(args.input_dir).resolve()
  output_path = Path(args.output).resolve()

  if not input_dir.is_dir():
    return fail(f"missing input directory: {input_dir}")

  report_paths = sorted(path for path in input_dir.rglob("*.json") if path.is_file())
  if not report_paths:
    return fail(f"no JSON reports found under {input_dir}")

  totals: Counter = Counter()
  per_file_totals: dict[str, Counter] = {}
  report_summaries: list[dict] = []

  try:
    for path in report_paths:
      summary, counts, per_file = summarize_report(path)
      report_summaries.append(summary)
      totals.update(counts)
      for file_path, file_counts in per_file.items():
        if file_path not in per_file_totals:
          per_file_totals[file_path] = Counter()
        per_file_totals[file_path].update(file_counts)
  except (OSError, json.JSONDecodeError, ValueError) as exc:
    return fail(str(exc))

  total_mutants = totals.get("mutants_total", 0)
  killed = totals.get("killed", 0)
  survived = totals.get("survived", 0)

  output = {
      "report_count": len(report_summaries),
      "reports": report_summaries,
      "totals": {
          "mutants_total": total_mutants,
          "killed": killed,
          "survived": survived,
          "other": total_mutants - killed - survived,
          "mutation_score": round((killed / total_mutants) * 100.0, 1) if total_mutants else 0.0,
          "by_status": counter_to_json(totals),
      },
      "per_file": {
          file_path: {
              "mutants_total": counts.get("mutants_total", 0),
              "killed": counts.get("killed", 0),
              "survived": counts.get("survived", 0),
              "other": counts.get("mutants_total", 0) - counts.get("killed", 0) -
                       counts.get("survived", 0),
              "by_status": counter_to_json(counts),
          }
          for file_path, counts in sorted(per_file_totals.items())
      },
  }

  output_path.parent.mkdir(parents=True, exist_ok=True)
  output_path.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n")
  print(f"aggregate=ok\nreports={len(report_summaries)}\noutput={output_path}")
  return 0


if __name__ == "__main__":
  sys.exit(main())
