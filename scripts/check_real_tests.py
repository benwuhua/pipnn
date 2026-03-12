#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path


def find_test_files(root: Path, marker_pattern: str) -> list[Path]:
  regex = re.compile(marker_pattern)
  return sorted(
      path for path in root.rglob("*") if path.is_file() and regex.match(path.name)
  )


def main() -> int:
  parser = argparse.ArgumentParser(description="Validate repository real-test markers.")
  parser.add_argument("feature_list", help="Path to feature-list.json")
  parser.add_argument("--feature", type=int, help="Feature id for reporting only")
  args = parser.parse_args()

  feature_list_path = Path(args.feature_list).resolve()
  repo_root = feature_list_path.parent
  try:
    data = json.loads(feature_list_path.read_text())
  except FileNotFoundError:
    print(f"FAIL: missing feature list: {feature_list_path}")
    return 1

  real_test_cfg = data.get("real_test", {})
  test_dir = repo_root / real_test_cfg.get("test_dir", "tests")
  marker_pattern = real_test_cfg.get("marker_pattern", r"^test_.*\.cpp$")
  mock_words = real_test_cfg.get("mock_patterns", [])
  mock_regex = re.compile(r"\b(" + "|".join(re.escape(word) for word in mock_words) + r")\b",
                          re.IGNORECASE) if mock_words else None

  if not test_dir.exists():
    print(f"FAIL: missing test directory: {test_dir}")
    return 1

  test_files = find_test_files(test_dir, marker_pattern)
  if not test_files:
    print(f"FAIL: no test files matched marker_pattern={marker_pattern!r} under {test_dir}")
    return 1

  integration_hits: list[tuple[Path, int]] = []
  mock_hits: list[tuple[Path, int, str]] = []
  for path in test_files:
    lines = path.read_text().splitlines()
    file_has_integration = any("[integration]" in line for line in lines)
    for lineno, line in enumerate(lines, start=1):
      if "[integration]" in line:
        integration_hits.append((path.relative_to(repo_root), lineno))
      if "mock_patterns" in line:
        continue
      if file_has_integration and mock_regex and mock_regex.search(line):
        mock_hits.append((path.relative_to(repo_root), lineno, line.strip()))

  feature_label = f"feature={args.feature}" if args.feature is not None else "feature=all"
  print(
      f"scan_scope={feature_label} test_dir={test_dir.relative_to(repo_root)} "
      f"matched_files={len(test_files)} real_test_markers={len(integration_hits)}"
  )

  if not integration_hits:
    print("FAIL: no [integration] markers found in matching test files")
    return 1

  if mock_hits:
    print("WARN: potential mock keywords detected in real-test scan")
    for path, lineno, line in mock_hits[:10]:
      print(f"  {path}:{lineno}: {line}")
    return 0

  print("PASS: real-test markers found and no mock keywords detected")
  return 0


if __name__ == "__main__":
  sys.exit(main())
