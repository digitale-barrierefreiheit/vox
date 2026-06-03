#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

"""Run clang-format over all first-party C/C++ sources.

Used by the VSCode task and the CI format gate so local and CI behaviour match.

Usage:
  python tools/run-clang-format.py            # rewrite files in place
  python tools/run-clang-format.py --check    # fail if any file is mis-formatted
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

SOURCE_DIRS = ("src", "tests", "tools")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx", ".inl", ".ipp"}
REPO_ROOT = Path(__file__).resolve().parent.parent


def discover_sources() -> list[Path]:
  files: list[Path] = []
  for directory in SOURCE_DIRS:
    base = REPO_ROOT / directory
    if not base.is_dir():
      continue
    files.extend(p for p in base.rglob("*") if p.suffix in SUFFIXES)
  return sorted(files)


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--check", action="store_true",
                      help="report mis-formatted files without modifying them")
  parser.add_argument("--clang-format", default="clang-format",
                      help="clang-format executable to use")
  args = parser.parse_args()

  sources = discover_sources()
  if not sources:
    print("No sources found.")
    return 0

  command = [args.clang_format]
  command += ["--dry-run", "--Werror"] if args.check else ["-i"]
  command += [str(p) for p in sources]

  try:
    completed = subprocess.run(command, cwd=REPO_ROOT, check=False)
  except FileNotFoundError:
    print(f"error: '{args.clang_format}' not found on PATH", file=sys.stderr)
    return 2
  return completed.returncode


if __name__ == "__main__":
  raise SystemExit(main())
