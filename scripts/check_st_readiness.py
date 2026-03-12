#!/usr/bin/env python3
import json
import sys

if len(sys.argv) != 2:
    print("usage: check_st_readiness.py feature-list.json")
    sys.exit(2)

d = json.load(open(sys.argv[1]))
for f in d.get("features", []):
    if f.get("status") != "deprecated" and f.get("status") != "passing":
        print(f"NOT_READY: feature {f.get('id')} status={f.get('status')}")
        sys.exit(1)
print("READY")
sys.exit(0)
