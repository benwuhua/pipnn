#!/usr/bin/env python3
import json
import sys

if len(sys.argv) != 2:
    print("usage: get_tool_commands.py feature-list.json")
    sys.exit(2)

d = json.load(open(sys.argv[1]))
lang = d["tech_stack"]["language"]
if lang != "cpp":
    print("unsupported language for this project")
    sys.exit(1)

print("env_activate: source .venv/bin/activate  # if python tooling env is used")
print("build: cmake -S . -B build && cmake --build build -j")
print("test: ctest --test-dir build --output-on-failure")
print("coverage: bash scripts/quality/remote_coverage.sh")
print("mutation: bash scripts/quality/remote_mutation_probe.sh")
