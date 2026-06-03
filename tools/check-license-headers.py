#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
"""Verify every first-party source file carries the required SPDX header.

Enforces the rule in AGENTS.md: each source file must begin with
`SPDX-License-Identifier: Apache-2.0`. Run in CI as a fast gate.

Usage:
  python tools/check-license-headers.py          # report missing headers, exit 1 if any
  python tools/check-license-headers.py --list    # just list the files that would be checked
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# Directories never scanned (build output, restored deps, vendored code, VCS).
EXCLUDED_DIRS = {".git", "build", "out", "vcpkg_installed", "third_party", "doc/reference"}

# A file requires a header if its name ends with one of these. The doubled
# variants (e.g. ".hpp.in") catch CMake `configure_file` templates.
SOURCE_SUFFIXES = (
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hpp", ".hxx", ".inl", ".ipp",
    ".cmake", ".py",
    ".c.in", ".cc.in", ".cpp.in", ".cxx.in",
    ".h.in", ".hpp.in", ".hxx.in",
)
SOURCE_NAMES = ("CMakeLists.txt",)

REQUIRED_TOKEN = "SPDX-License-Identifier: Apache-2.0"
# Read a few lines so a shebang or an opening block comment before the SPDX tag
# is tolerated.
LINES_TO_SCAN = 8


def is_excluded(path: Path) -> bool:
  relative = path.relative_to(REPO_ROOT).as_posix()
  return any(
      relative == d or relative.startswith(d + "/") for d in EXCLUDED_DIRS
  )


def requires_header(path: Path) -> bool:
  name = path.name
  if name in SOURCE_NAMES:
    return True
  return any(name.endswith(suffix) for suffix in SOURCE_SUFFIXES)


def discover() -> list[Path]:
  files = []
  for path in REPO_ROOT.rglob("*"):
    if not path.is_file() or is_excluded(path):
      continue
    if requires_header(path):
      files.append(path)
  return sorted(files)


def has_header(path: Path) -> bool:
  try:
    with path.open("r", encoding="utf-8") as handle:
      for _ in range(LINES_TO_SCAN):
        line = handle.readline()
        if not line:
          break
        if REQUIRED_TOKEN in line:
          return True
  except (OSError, UnicodeDecodeError):
    return False
  return False


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--list", action="store_true",
                      help="list the files that would be checked, then exit")
  args = parser.parse_args()

  files = discover()
  if args.list:
    for path in files:
      print(path.relative_to(REPO_ROOT).as_posix())
    return 0

  missing = [p for p in files if not has_header(p)]
  if missing:
    print("Missing SPDX license header (expected '%s'):" % REQUIRED_TOKEN,
          file=sys.stderr)
    for path in missing:
      print("  " + path.relative_to(REPO_ROOT).as_posix(), file=sys.stderr)
    print("\nAdd the project header to each file (see AGENTS.md).", file=sys.stderr)
    return 1

  print(f"OK: {len(files)} source files carry the SPDX header.")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
