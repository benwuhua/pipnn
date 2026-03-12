#!/usr/bin/env python3
import json
import sys
from pathlib import Path

p = Path(sys.argv[1] if len(sys.argv) > 1 else "increment-request.json")
if not p.exists():
    print("increment request file not found")
    sys.exit(1)
json.loads(p.read_text())
print("OK")
