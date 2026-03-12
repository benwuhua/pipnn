#!/usr/bin/env python3
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else "docs/test-cases")
if not root.exists():
    print("missing test-cases dir")
    sys.exit(1)
print("OK")
