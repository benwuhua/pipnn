#!/usr/bin/env python3
from __future__ import annotations

import argparse
import binascii
from pathlib import Path


SEARCH_ROOT = Path("/data/work/datasets")
KNOWN_ROOTS = [
    Path("/data/work/datasets/openai-arxiv"),
    Path("/data/work/datasets/simplewiki-openai"),
    Path("/data/work/datasets/crisp/simplewiki-openai"),
    Path("/data/work/datasets/wikipedia-cohere-1m"),
]
PATTERNS = ("*.fbin", "*.ibin", "*.fvecs", "*.ivecs", "*.hdf5")
TOKENS = ("openai", "arxiv", "cohere", "wiki")


def iter_matches(root: Path) -> list[Path]:
    matched: list[Path] = []
    for pattern in PATTERNS:
        matched.extend(sorted(root.rglob(pattern)))
    return matched


def command_candidates() -> int:
    seen: set[Path] = set()
    for path in sorted(SEARCH_ROOT.iterdir()):
        if any(token in path.name.lower() for token in TOKENS):
            print(path)
            seen.add(path)
    for root in KNOWN_ROOTS:
        if root not in seen:
            print(root)
            seen.add(root)
        if not root.exists():
            continue
        for path in sorted(root.glob("*")):
            if not (
                any(token in path.name.lower() for token in TOKENS)
                or path.suffix.lower() in {".fbin", ".ibin", ".fvecs", ".ivecs", ".hdf5"}
            ):
                continue
            if path in seen:
                continue
            print(path)
            seen.add(path)
    return 0


def command_files() -> int:
    for root in KNOWN_ROOTS:
        print(f"ROOT {root}")
        if not root.exists():
            print("  missing")
            continue
        print(f"  resolved={root.resolve()}")
        matched = iter_matches(root)
        if not matched:
            print("  no-matching-files")
            continue
        for path in matched[:80]:
            print(f"  FILE {path} size={path.stat().st_size}")
    return 0


def command_headers() -> int:
    seen: set[Path] = set()
    for root in KNOWN_ROOTS:
        if not root.exists():
            continue
        for path in iter_matches(root):
            if path in seen:
                continue
            seen.add(path)
            print(f"FILE {path}")
            try:
                with path.open("rb") as handle:
                    head = handle.read(32)
                print("  head_hex=" + binascii.hexlify(head).decode("ascii"))
                print("  head_len=" + str(len(head)))
            except Exception as exc:  # pragma: no cover - diagnostic path
                print(f"  error={exc}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("candidates", "files", "headers"))
    args = parser.parse_args()
    if args.mode == "candidates":
        return command_candidates()
    if args.mode == "files":
        return command_files()
    return command_headers()


if __name__ == "__main__":
    raise SystemExit(main())
