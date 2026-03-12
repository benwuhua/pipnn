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


def mutant_key(file_path: str, mutant: dict) -> tuple:
  location = mutant.get("location", {})
  start = location.get("start", {})
  end = location.get("end", {})
  return (
      file_path,
      mutant.get("id"),
      mutant.get("replacement"),
      start.get("line"),
      start.get("column"),
      end.get("line"),
      end.get("column"),
  )


def status_priority(status: str) -> int:
  if status == "killed":
    return 2
  if status == "survived":
    return 1
  return 0


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


def summarize_survivor(mutant: dict) -> dict:
  location = mutant.get("location", {})
  start = location.get("start", {})
  end = location.get("end", {})
  return {
      "file": mutant["file"],
      "id": mutant.get("id", ""),
      "replacement": mutant.get("replacement", ""),
      "status": mutant["status"],
      "reports": sorted(mutant["reports"]),
      "location": {
          "start": {
              "line": start.get("line"),
              "column": start.get("column"),
          },
          "end": {
              "line": end.get("line"),
              "column": end.get("column"),
          },
      },
  }


def survivor_line(entry: dict) -> str:
  start = entry["location"]["start"]
  line = start.get("line")
  column = start.get("column")
  location = f"{line}:{column}" if line is not None and column is not None else "?:?"
  replacement = entry.get("replacement") or "?"
  reports = ",".join(entry.get("reports", []))
  return f"{entry['file']}:{location} {entry['id']} replacement={replacement} reports={reports}"


def main() -> int:
  parser = argparse.ArgumentParser(description="Aggregate raw mutation report JSON files.")
  parser.add_argument("--input-dir", required=True, help="Directory containing raw report JSON files")
  parser.add_argument("--output", required=True, help="Output JSON summary path")
  parser.add_argument("--score-output", help="Optional output path for aggregate mutation score text")
  parser.add_argument("--survivors-output", help="Optional output path for surviving mutants text")
  args = parser.parse_args()

  input_dir = Path(args.input_dir).resolve()
  output_path = Path(args.output).resolve()
  score_output_path = Path(args.score_output).resolve() if args.score_output else None
  survivors_output_path = Path(args.survivors_output).resolve() if args.survivors_output else None

  if not input_dir.is_dir():
    return fail(f"missing input directory: {input_dir}")

  report_paths = sorted(path for path in input_dir.rglob("*.json") if path.is_file())
  if not report_paths:
    return fail(f"no JSON reports found under {input_dir}")

  totals: Counter = Counter()
  report_summaries: list[dict] = []
  dedup_mutants: dict[tuple, dict] = {}

  try:
    for path in report_paths:
      summary, counts, per_file = summarize_report(path)
      report_summaries.append(summary)
      totals.update(counts)

      payload = json.loads(path.read_text())
      files = validate_report(path, payload)
      for file_path, entry in files.items():
        for mutant in entry["mutants"]:
          key = mutant_key(file_path, mutant)
          normalized = normalize_status(mutant["status"])
          candidate = {
              "file": file_path,
              "id": mutant.get("id", ""),
              "replacement": mutant.get("replacement", ""),
              "status": normalized,
              "location": mutant.get("location", {}),
              "reports": {path.name},
          }
          current = dedup_mutants.get(key)
          if current is None:
            dedup_mutants[key] = candidate
            continue
          current["reports"].add(path.name)
          if status_priority(normalized) > status_priority(current["status"]):
            current["status"] = normalized
  except (OSError, json.JSONDecodeError, ValueError) as exc:
    return fail(str(exc))

  total_mutants = totals.get("mutants_total", 0)
  killed = totals.get("killed", 0)
  survived = totals.get("survived", 0)
  dedup_totals: Counter = Counter()
  dedup_per_file: dict[str, Counter] = {}
  survivors: list[dict] = []
  for entry in dedup_mutants.values():
    file_path = entry["file"]
    if file_path not in dedup_per_file:
      dedup_per_file[file_path] = Counter()
    dedup_totals["mutants_total"] += 1
    dedup_totals[entry["status"]] += 1
    dedup_per_file[file_path]["mutants_total"] += 1
    dedup_per_file[file_path][entry["status"]] += 1
    if entry["status"] == "survived":
      survivors.append(summarize_survivor(entry))

  dedup_total_mutants = dedup_totals.get("mutants_total", 0)
  dedup_killed = dedup_totals.get("killed", 0)
  dedup_survived = dedup_totals.get("survived", 0)
  mutation_score = round((dedup_killed / dedup_total_mutants) * 100.0, 1) if dedup_total_mutants else 0.0

  output = {
      "report_count": len(report_summaries),
      "reports": report_summaries,
      "raw_totals": {
          "mutants_total": total_mutants,
          "killed": killed,
          "survived": survived,
          "other": total_mutants - killed - survived,
          "mutation_score": round((killed / total_mutants) * 100.0, 1) if total_mutants else 0.0,
          "by_status": counter_to_json(totals),
      },
      "totals": {
          "mutants_total": dedup_total_mutants,
          "killed": dedup_killed,
          "survived": dedup_survived,
          "other": dedup_total_mutants - dedup_killed - dedup_survived,
          "mutation_score": mutation_score,
          "by_status": counter_to_json(dedup_totals),
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
          for file_path, counts in sorted(dedup_per_file.items())
      },
      "survivors": sorted(
          survivors,
          key=lambda entry: (
              entry["file"],
              entry["location"]["start"].get("line") or -1,
              entry["location"]["start"].get("column") or -1,
              entry["id"],
          ),
      ),
  }

  output_path.parent.mkdir(parents=True, exist_ok=True)
  output_path.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n")
  if score_output_path:
    score_output_path.parent.mkdir(parents=True, exist_ok=True)
    score_output_path.write_text(f"{mutation_score:.1f}\n")
  if survivors_output_path:
    survivors_output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [survivor_line(entry) for entry in output["survivors"]]
    survivors_output_path.write_text("\n".join(lines) + ("\n" if lines else ""))
  print(f"aggregate=ok\nreports={len(report_summaries)}\noutput={output_path}")
  return 0


if __name__ == "__main__":
  sys.exit(main())
