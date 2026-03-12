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
print("coverage: rm -rf build-cov && cmake -S . -B build-cov -DCMAKE_CXX_FLAGS='--coverage -O0 -g' -DCMAKE_C_FLAGS='--coverage -O0 -g' && cmake --build build-cov -j && ctest --test-dir build-cov --output-on-failure && python3 -m gcovr build-cov -r . --merge-mode-functions=merge-use-line-max --exclude 'build-cov/_deps/' --exclude 'build-cov/CMakeFiles/.*/CompilerIdCXX/' --exclude 'tests/' --txt && python3 -m gcovr build-cov -r . --merge-mode-functions=merge-use-line-max --exclude 'build-cov/_deps/' --exclude 'build-cov/CMakeFiles/.*/CompilerIdCXX/' --exclude 'tests/' --txt --txt-metric branch")
print("mutation: mull-runner --help")
