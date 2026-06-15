# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
"""Shared helpers for the tools/ script tests.

The scripts under tools/ have hyphenated names, so they cannot be imported as
modules the normal way. We load them by path via importlib; coverage still
attributes the executed lines to the real files because each module's __file__
points back at tools/<name>.py.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
from types import ModuleType

REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools"


def load_script(filename: str) -> ModuleType:
    """Import tools/<filename> (a hyphenated script) as a module object."""
    path = TOOLS_DIR / filename
    module_name = filename.removesuffix(".py").replace("-", "_")
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"cannot load tools script: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
