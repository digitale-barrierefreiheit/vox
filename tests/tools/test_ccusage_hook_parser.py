# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
"""Regression tests for the inline ccusage parser in tools/hooks/pre-push.

The parser is embedded in the hook (rather than a standalone .js) on purpose: a
separate JS file would be analysed by Sonar with no JS coverage report, dragging
new-code coverage down. To still exercise the real runtime logic, these tests
extract the `node -e '...'` script straight from the hook and run it under node
against fixtures — so a refactor that breaks month/project filtering or the cost
field fallbacks fails here. Skipped where node is unavailable (e.g. a WSL distro
without node); the sonar CI job runs on a runner that has it.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest

HOOK = Path(__file__).resolve().parents[2] / "tools" / "hooks" / "pre-push"

pytestmark = pytest.mark.skipif(shutil.which("node") is None, reason="node not on PATH")


def _parser_script():
    """Pull the single inline `node -e '<script>'` parser out of the hook.

    The embedded JS contains no single quotes, so a lazy match to the next quote
    is exact.
    """
    text = HOOK.read_text(encoding="utf-8")
    match = re.search(r"node -e '(.*?)'", text, re.DOTALL)
    assert match, "could not find the inline `node -e` parser in the hook"
    return match.group(1)


def _run(payload, month="2026-06", project="vox"):
    env = {**os.environ, "VOX_COST_MONTH": month, "VOX_CCUSAGE_PROJECT": project}
    body = payload if isinstance(payload, str) else json.dumps(payload)
    proc = subprocess.run(["node", "-e", _parser_script()], input=body,
                          capture_output=True, text=True, env=env, check=False)
    return proc.stdout.strip()


def test_sums_only_vox_current_month():
    payload = {"projects": {
        "c--Users-x-source-repos-vox": [
            {"date": "2026-06-01", "totalCost": 1.50},
            {"date": "2026-06-15", "totalCost": 2.25},
            {"date": "2026-05-30", "totalCost": 9.99},  # prior month -> excluded
        ],
        "some-other-project": [{"date": "2026-06-10", "totalCost": 5.00}],  # not vox
    }}
    assert _run(payload) == "3.75"


def test_cost_field_fallbacks():
    # totalCost is preferred, then costUSD, then cost.
    payload = {"projects": {"vox": [
        {"date": "2026-06-01", "costUSD": 2.00},
        {"date": "2026-06-02", "cost": 0.50},
    ]}}
    assert _run(payload) == "2.50"


def test_project_match_is_configurable():
    payload = {"projects": {"my-vault": [{"date": "2026-06-01", "totalCost": 4.00}]}}
    assert _run(payload, project="vault") == "4.00"
    assert _run(payload, project="vox") == ""  # no key contains "vox"


def test_dateless_row_excluded_when_month_set():
    # A row without a usable date must not be counted into the target month.
    payload = {"projects": {"vox": [
        {"date": "2026-06-01", "totalCost": 1.00},
        {"totalCost": 99.00},  # no date -> excluded
        {"date": "", "totalCost": 88.00},  # empty date -> excluded
    ]}}
    assert _run(payload) == "1.00"


def test_empty_and_malformed_print_nothing():
    assert _run({}) == ""  # no projects
    assert _run({"projects": {"vox": [{"date": "2026-06-01", "totalCost": 0}]}}) == ""  # zero
    assert _run("{not valid json") == ""  # best-effort: no crash, no output
