# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
"""Tests for tools/cost_collector.py."""
from __future__ import annotations

import json
import runpy
import urllib.error

import pytest
from script_loader import TOOLS_DIR, load_script

cc = load_script("cost_collector.py")


# --------------------------------------------------------------------------- #
# Pure helpers
# --------------------------------------------------------------------------- #
@pytest.mark.parametrize("labels, expected", [
    (["ubuntu-24.04"], "Linux"),
    (["windows-2025"], "Windows"),
    (["macos-14"], "macOS"),
    (["self-hosted", "linux"], None),  # self-hosted is not billed per-minute
    (["some-runner"], None),
    ([], None),
    (None, None),
])
def test_os_bucket(labels, expected):
    assert cc.os_bucket(labels) == expected


def test_parse_ts():
    assert cc.parse_ts("2026-06-15T10:00:30Z").year == 2026
    assert cc.parse_ts("not-a-date") is None
    assert cc.parse_ts("") is None
    assert cc.parse_ts(None) is None


def test_job_minutes():
    assert cc.job_minutes("2026-06-15T10:00:00Z", "2026-06-15T10:02:30Z") == 3  # ceil(150/60)
    assert cc.job_minutes("2026-06-15T10:00:00Z", "2026-06-15T10:02:00Z") == 2  # exact
    assert cc.job_minutes("2026-06-15T10:05:00Z", "2026-06-15T10:00:00Z") == 0  # negative
    assert cc.job_minutes("2026-06-15T10:00:00Z", None) == 0  # still running
    assert cc.job_minutes(None, None) == 0


def test_aggregate_actions_buckets_and_unknown():
    jobs = [
        {"labels": ["ubuntu-24.04"], "started_at": "2026-06-01T00:00:00Z",
         "completed_at": "2026-06-01T00:01:30Z"},  # 2 min
        {"labels": ["windows-2025"], "started_at": "2026-06-01T00:00:00Z",
         "completed_at": "2026-06-01T00:03:00Z"},  # 3 min
        {"labels": ["ubuntu-24.04"], "started_at": "2026-06-01T00:00:00Z",
         "completed_at": "2026-06-01T00:00:30Z"},  # 1 min
        {"labels": ["mystery"], "started_at": "2026-06-01T00:00:00Z",
         "completed_at": "2026-06-01T00:10:00Z"},  # unknown -> skipped
    ]
    agg = cc.aggregate_actions(jobs)
    assert agg["minutes"] == {"Linux": 3, "Windows": 3}
    assert agg["counts"] == {"Linux": 2, "Windows": 1}
    assert agg["unknown"] == 1


def test_impute_actions_cost():
    cost = cc.impute_actions_cost({"Linux": 1000, "Windows": 500})
    assert cost["per_os"]["Linux"] == pytest.approx(6.0)
    assert cost["per_os"]["Windows"] == pytest.approx(5.0)
    assert cost["total"] == pytest.approx(11.0)
    # An unrecognised OS uses a 0 rate, contributing nothing.
    assert cc.impute_actions_cost({"Plan9": 100})["total"] == 0.0
    # A caller may override the rate table.
    assert cc.impute_actions_cost({"Linux": 10}, {"Linux": 1.0})["total"] == 10.0


def test_month_window():
    import datetime as dt
    assert cc.month_window(month="2026-06") == (2026, 6, "2026-06")
    fixed = dt.datetime(2026, 1, 9, tzinfo=dt.timezone.utc)
    assert cc.month_window(now=fixed) == (2026, 1, "2026-01")


def test_fmt_usd():
    assert cc._fmt_usd(1234.5) == "$1,234.50"


# --------------------------------------------------------------------------- #
# Snapshot rendering / replacement
# --------------------------------------------------------------------------- #
def _full_data():
    return {
        "generated": "2026-06-16",
        "month_label": "2026-06",
        "sonar_ncloc": 13862,
        "actions": {
            "minutes": {"Linux": 8953, "Windows": 12716},
            "counts": {"Linux": 40, "Windows": 30},
            "unknown": 2,
            "cost": {"per_os": {"Linux": 53.72, "Windows": 127.16}, "total": 180.88},
        },
        "billing": None,
        "ai_review": None,
    }


def test_render_snapshot_happy_path():
    text = cc.render_snapshot(_full_data())
    assert text.startswith(cc.SNAPSHOT_START)
    assert text.rstrip().endswith(cc.SNAPSHOT_END)
    assert "13,862 as read on 2026-06-16" in text
    assert "$180.88" in text
    assert "$0.00" in text  # actually billed
    assert "2 job(s) had unrecognised runner labels" in text
    assert "not configured (prerequisite" in text  # billing line
    assert "not yet reported" in text  # ai-review line
    assert "manual / local feed" in text  # claude line


def test_render_snapshot_degraded_and_configured():
    data = _full_data()
    data["sonar_ncloc"] = None
    data["actions"] = None
    data["billing"] = {"error": "boom"}
    data["ai_review"] = {"usd": 42.50, "note": "Contributed by the maintainer."}
    text = cc.render_snapshot(data)
    assert "ncloc`):** not available" in text
    assert "Actions minutes:** not available" in text
    assert "read error (boom)" in text
    assert "$42.50" in text
    assert "Contributed by the maintainer." in text


def test_render_snapshot_billing_configured():
    data = _full_data()
    data["billing"] = {"text": "gross $1.00 / net $0.00 for 2026-06 (repository `vox`)."}
    text = cc.render_snapshot(data)
    assert "gross $1.00 / net $0.00" in text


def test_render_snapshot_no_unknown_jobs():
    data = _full_data()
    data["actions"]["unknown"] = 0
    text = cc.render_snapshot(data)
    assert "unrecognised runner labels" not in text


def test_replace_snapshot_roundtrip():
    doc = f"intro\n{cc.SNAPSHOT_START}\nOLD\n{cc.SNAPSHOT_END}\noutro\n"
    out = cc.replace_snapshot(doc, f"{cc.SNAPSHOT_START}\nNEW\n{cc.SNAPSHOT_END}")
    assert "intro" in out and "outro" in out
    assert "OLD" not in out and "NEW" in out


def test_replace_snapshot_missing_markers():
    with pytest.raises(ValueError, match="markers not found"):
        cc.replace_snapshot("no markers here", "x")


# --------------------------------------------------------------------------- #
# IO (mocked)
# --------------------------------------------------------------------------- #
class _FakeResponse:
    def __init__(self, payload):
        self._payload = payload

    def read(self):
        return json.dumps(self._payload).encode("utf-8")

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


def test_http_get_json_sets_headers(monkeypatch):
    captured = {}

    def fake_urlopen(request, timeout=None):
        captured["request"] = request
        captured["timeout"] = timeout
        return _FakeResponse({"ok": True})

    monkeypatch.setattr("urllib.request.urlopen", fake_urlopen)
    assert cc.http_get_json("https://x/y", token="t0ken") == {"ok": True}
    req = captured["request"]
    assert req.get_header("Authorization") == "Bearer t0ken"
    assert req.get_header("User-agent") == cc.USER_AGENT
    assert captured["timeout"] == 30

    # No token -> no Authorization header.
    monkeypatch.setattr("urllib.request.urlopen", fake_urlopen)
    cc.http_get_json("https://x/y")
    assert captured["request"].get_header("Authorization") is None


def test_paginate_follows_pages(monkeypatch):
    pages = [{"items": list(range(100))}, {"items": [1, 2, 3]}]
    seen = []

    def fake_get(url, token=None, accept=None):
        seen.append(url)
        return pages.pop(0)

    monkeypatch.setattr(cc, "http_get_json", fake_get)
    result = cc._paginate("https://api/x", "items", "tok")
    assert len(result) == 103
    assert "page=1" in seen[0] and "page=2" in seen[1]


def test_fetch_sonar_ncloc(monkeypatch):
    monkeypatch.setattr(cc, "http_get_json", lambda url, accept=None: {
        "component": {"measures": [{"metric": "ncloc", "value": "13862"}]}})
    assert cc.fetch_sonar_ncloc("vox") == 13862
    # Missing measure -> None.
    monkeypatch.setattr(cc, "http_get_json", lambda url, accept=None: {"component": {}})
    assert cc.fetch_sonar_ncloc("vox") is None


def test_fetch_actions_jobs(monkeypatch):
    def fake_paginate(url, key, token):
        if key == "workflow_runs":
            return [{"id": 11}, {"id": 22}]
        assert "/runs/" in url and "/jobs" in url
        return [{"labels": ["ubuntu-24.04"], "started_at": "2026-06-01T00:00:00Z",
                 "completed_at": "2026-06-01T00:01:00Z"}]

    monkeypatch.setattr(cc, "_paginate", fake_paginate)
    jobs = cc.fetch_actions_jobs(2026, 6, "tok")
    assert len(jobs) == 2  # one job per run


def test_fetch_org_billing(monkeypatch):
    monkeypatch.setattr(cc, "http_get_json", lambda url, token=None: {"usageItems": [
        {"repositoryName": "vox", "product": "Actions", "grossAmount": 100.0, "netAmount": 0.0},
        {"repositoryName": "vox", "product": "Actions", "grossAmount": 27.16, "netAmount": 0.0},
        {"repositoryName": "other", "product": "Actions", "grossAmount": 999.0, "netAmount": 9.0},
    ]})
    out = cc.fetch_org_billing(2026, 6, "tok")
    assert "gross $127.16 / net $0.00" in out["text"]
    assert "999" not in out["text"]


def test_read_ai_review(tmp_path):
    path = tmp_path / "ai-review.json"
    path.write_text('{"months": {"2026-06": {"usd": 42.50, "note": "x"}}}', encoding="utf-8")
    assert cc.read_ai_review("2026-06", path) == {"usd": 42.50, "note": "x"}
    assert cc.read_ai_review("2026-05", path) is None  # month absent
    assert cc.read_ai_review("2026-06", tmp_path / "nope.json") is None  # missing file
    bad = tmp_path / "bad.json"
    bad.write_text('{"months": {"2026-06": {"note": "no usd"}}}', encoding="utf-8")
    assert cc.read_ai_review("2026-06", bad) is None  # malformed entry


# --------------------------------------------------------------------------- #
# Orchestration
# --------------------------------------------------------------------------- #
def test_guarded():
    assert cc._guarded(lambda: {"text": "ok"}) == {"text": "ok"}
    assert cc._guarded(lambda: (_ for _ in ()).throw(OSError("net")))["error"] == "net"


def test_collect_without_credentials(monkeypatch):
    monkeypatch.setattr(cc, "fetch_sonar_ncloc", lambda component: 13862)
    monkeypatch.setattr(cc, "fetch_actions_jobs", lambda y, m, t: [
        {"labels": ["ubuntu-24.04"], "started_at": "2026-06-01T00:00:00Z",
         "completed_at": "2026-06-01T00:01:00Z"}])
    monkeypatch.setattr(cc, "read_ai_review", lambda label: None)
    data = cc.collect(month="2026-06")
    assert data["sonar_ncloc"] == 13862
    assert data["actions"]["minutes"] == {"Linux": 1}
    assert data["actions"]["cost"]["total"] == pytest.approx(0.01)
    assert data["billing"] is None and data["ai_review"] is None


def test_collect_with_billing_and_ai_review(monkeypatch):
    monkeypatch.setattr(cc, "fetch_sonar_ncloc", lambda component: 1)
    monkeypatch.setattr(cc, "fetch_actions_jobs", lambda y, m, t: [])
    monkeypatch.setattr(cc, "fetch_org_billing", lambda y, m, t: {"text": "billed"})
    monkeypatch.setattr(cc, "read_ai_review", lambda label: {"usd": 12.5, "note": "n"})
    data = cc.collect(month="2026-06", billing_token="b")
    assert data["billing"] == {"text": "billed"}
    assert data["ai_review"] == {"usd": 12.5, "note": "n"}


def test_collect_tolerates_read_failures(monkeypatch):
    def boom(*args, **kwargs):
        raise OSError("offline")

    monkeypatch.setattr(cc, "fetch_sonar_ncloc", boom)
    monkeypatch.setattr(cc, "fetch_actions_jobs", boom)
    monkeypatch.setattr(cc, "fetch_org_billing", boom)
    data = cc.collect(month="2026-06", billing_token="b")
    assert data["sonar_ncloc"] is None
    assert data["actions"] is None
    assert data["billing"]["error"] == "offline"


# --------------------------------------------------------------------------- #
# main()
# --------------------------------------------------------------------------- #
def test_main_print(monkeypatch, capsys):
    monkeypatch.setattr(cc, "collect", lambda **kw: _full_data())
    monkeypatch.delenv("GITHUB_TOKEN", raising=False)
    monkeypatch.delenv("GH_TOKEN", raising=False)
    assert cc.main(["--print"]) == 0
    out = capsys.readouterr().out
    assert cc.SNAPSHOT_START in out and "$180.88" in out


def test_main_writes_doc(tmp_path, monkeypatch, capsys):
    doc = tmp_path / "cost-ledger.md"
    doc.write_text(f"head\n{cc.SNAPSHOT_START}\nplaceholder\n{cc.SNAPSHOT_END}\ntail\n",
                   encoding="utf-8")
    monkeypatch.setattr(cc, "collect", lambda **kw: _full_data())
    monkeypatch.setenv("GITHUB_TOKEN", "tok")
    assert cc.main(["--doc", str(doc), "--month", "2026-06"]) == 0
    written = doc.read_text(encoding="utf-8")
    assert "head" in written and "tail" in written
    assert "placeholder" not in written
    assert "13,862 as read on" in written
    assert "Updated snapshot" in capsys.readouterr().out


def test_entry_guard_runs_as_main(monkeypatch):
    # urlopen raises so every read falls back to None; --print still exits 0.
    def offline(*args, **kwargs):
        raise urllib.error.URLError("offline")

    monkeypatch.setattr("urllib.request.urlopen", offline)
    monkeypatch.setattr("sys.argv", ["cost_collector.py", "--print"])
    monkeypatch.delenv("COST_BILLING_TOKEN", raising=False)
    with pytest.raises(SystemExit) as exc:
        runpy.run_path(str(TOOLS_DIR / "cost_collector.py"), run_name="__main__")
    assert exc.value.code == 1  # offline: nothing collected -> non-zero exit
