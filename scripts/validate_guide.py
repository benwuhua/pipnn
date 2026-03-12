#!/usr/bin/env python3
import sys
from pathlib import Path

REQUIRED = [
  "## Orient",
  "## Bootstrap",
  "## Config Gate",
  "## TDD Red",
  "## TDD Green",
  "## Coverage Gate",
  "## TDD Refactor",
  "## Mutation Gate",
  "## Verification Enforcement",
  "## Code Review",
  "## Examples",
  "## Persist",
  "## Critical Rules",
  "## Environment Commands",
  "## Config Management",
  "## Real Test Convention"
]

if len(sys.argv) < 2:
    print("usage: validate_guide.py long-task-guide.md --feature-list feature-list.json")
    sys.exit(2)
text = Path(sys.argv[1]).read_text()
missing = [h for h in REQUIRED if h not in text]
if missing:
    print("MISSING HEADERS:")
    for h in missing:
        print("-", h)
    sys.exit(1)
print("OK: guide sections present")
