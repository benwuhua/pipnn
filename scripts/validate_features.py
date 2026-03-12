#!/usr/bin/env python3
import json
import sys
from pathlib import Path

REQUIRED_ROOT = ["project", "created", "tech_stack", "quality_gates", "constraints", "assumptions", "required_configs", "features"]
REQUIRED_TECH = ["language", "test_framework", "coverage_tool", "mutation_tool"]
REQ_FEATURE = ["id", "category", "title", "description", "priority", "status", "verification_steps", "dependencies"]


def fail(msg: str) -> None:
    print(f"ERROR: {msg}")
    sys.exit(1)


def main() -> None:
    if len(sys.argv) != 2:
        print("usage: validate_features.py feature-list.json")
        sys.exit(2)

    p = Path(sys.argv[1])
    if not p.exists():
        fail(f"file not found: {p}")

    data = json.loads(p.read_text())
    for k in REQUIRED_ROOT:
        if k not in data:
            fail(f"missing root key: {k}")

    for k in REQUIRED_TECH:
        if k not in data["tech_stack"]:
            fail(f"missing tech_stack key: {k}")

    ids = set()
    for i, f in enumerate(data["features"], start=1):
        for k in REQ_FEATURE:
            if k not in f:
                fail(f"feature[{i}] missing key: {k}")
        if f["id"] in ids:
            fail(f"duplicate feature id: {f['id']}")
        ids.add(f["id"])
        if not isinstance(f["verification_steps"], list) or len(f["verification_steps"]) == 0:
            fail(f"feature {f['id']} verification_steps must be non-empty list")

    print("OK: feature-list.json is valid")


if __name__ == "__main__":
    main()
