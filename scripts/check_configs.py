#!/usr/bin/env python3
import json
import os
import sys
from pathlib import Path


def load_remote_env() -> None:
    p = Path.home() / ".config" / "generic-x86-remote" / "remote.env"
    if not p.exists():
        return
    for line in p.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip())


def main() -> None:
    if len(sys.argv) < 2:
        print("usage: check_configs.py feature-list.json [--feature <id>]")
        sys.exit(2)

    load_remote_env()
    data = json.load(open(sys.argv[1]))

    target_ids = None
    if len(sys.argv) == 4 and sys.argv[2] == "--feature":
        target_ids = {int(sys.argv[3])}

    missing = []
    for c in data.get("required_configs", []):
        req_by = set(c.get("required_by", []))
        if target_ids is not None and req_by.isdisjoint(target_ids):
            continue
        if c.get("type") == "env":
            key = c.get("key")
            if not os.environ.get(key, ""):
                missing.append((c.get("name"), key, c.get("check_hint", "")))
        elif c.get("type") == "file":
            path = c.get("path", "")
            if not Path(path).exists():
                missing.append((c.get("name"), path, c.get("check_hint", "")))

    if missing:
        for name, key_or_path, hint in missing:
            print(f"MISSING: {name} ({key_or_path})")
            if hint:
                print(f"  hint: {hint}")
        sys.exit(1)

    print("OK: all required configs present")


if __name__ == "__main__":
    main()
