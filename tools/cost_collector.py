#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
"""Collect the facts that drive Vox's running costs into the cost-ledger snapshot.

See doc/cost-ledger.md and issue #76 for the methodology. This refreshes the
marker-bounded "snapshot" block of the --doc file (the cost-data branch's
snapshot.md in CI). SonarCloud ncloc is auth-free;
the GitHub Actions minutes line needs a GitHub token (GITHUB_TOKEN / GH_TOKEN,
provided automatically in CI). The org-billing cross-check needs a provisioned
secret, otherwise recording "not configured (prerequisite)". The AI code-review
cost is reported out-of-band (ai-review.json on the cost-data branch) — no
third-party billing credentials live here.

Usage:
  python tools/cost_collector.py [--month YYYY-MM] [--doc PATH] [--ai-review PATH]
  python tools/cost_collector.py --print   # print the snapshot, do not write
"""
from __future__ import annotations

import argparse
import calendar
import json
import math
import os
import re
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

OWNER = "digitale-barrierefreiheit"
REPO = "vox"
SONAR_COMPONENT = "vox"

SNAPSHOT_START = "<!-- cost-snapshot:start -->"
SNAPSHOT_END = "<!-- cost-snapshot:end -->"

# GitHub Actions per-minute LIST prices (USD) for standard GitHub-hosted runners,
# as read 2026-01-01. macOS is informational only (Vox runs Linux + Windows jobs).
ACTIONS_RATES_AS_OF = "2026-01-01"
ACTIONS_RATES = {"Linux": 0.006, "Windows": 0.010, "macOS": 0.062}

GITHUB_API = "https://api.github.com"
SONAR_API = "https://sonarcloud.io/api"
USER_AGENT = "vox-cost-collector"

# IO failures we tolerate per source so one read cannot abort the whole snapshot.
# urllib.error.URLError is an OSError; json.JSONDecodeError is a ValueError.
_READ_ERRORS = (OSError, ValueError, KeyError)


# --------------------------------------------------------------------------- #
# Pure helpers (no IO) — these carry the testable logic.
# --------------------------------------------------------------------------- #
def os_bucket(labels):
  """Map a GitHub-hosted runner's labels to 'Linux'/'Windows'/'macOS', else None.

  Self-hosted runners are not billed per minute, so they are unclassifiable here
  (returned as None and excluded from the imputed list-price cost).
  """
  joined = " ".join(labels or []).lower()
  if "self-hosted" in joined:
    return None
  if "ubuntu" in joined or "linux" in joined:
    return "Linux"
  if "windows" in joined:
    return "Windows"
  if "macos" in joined:
    return "macOS"
  return None


def parse_ts(value):
  """Parse an ISO-8601 timestamp (with a trailing 'Z') to an aware datetime."""
  if not value:
    return None
  try:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))
  except ValueError:
    return None


def job_minutes(started_at, completed_at):
  """Minutes for one job: ceil(duration / 60), never negative."""
  start = parse_ts(started_at)
  end = parse_ts(completed_at)
  if start is None or end is None:
    return 0
  seconds = (end - start).total_seconds()
  if seconds <= 0:
    return 0
  return math.ceil(seconds / 60)


def aggregate_actions(jobs):
  """Sum minutes and job counts per OS bucket; count jobs we cannot classify."""
  minutes = {}
  counts = {}
  unknown = 0
  for job in jobs:
    bucket = os_bucket(job.get("labels"))
    if bucket is None:
      unknown += 1
      continue
    minutes[bucket] = minutes.get(bucket, 0) + job_minutes(
        job.get("started_at"), job.get("completed_at"))
    counts[bucket] = counts.get(bucket, 0) + 1
  return {"minutes": minutes, "counts": counts, "unknown": unknown}


def impute_actions_cost(minutes_by_os, rates=None):
  """Imputed list-price USD per OS and the total, from minutes x per-OS rate."""
  rates = ACTIONS_RATES if rates is None else rates
  per_os = {}
  total = 0.0
  for name, mins in minutes_by_os.items():
    per_os[name] = round(mins * rates.get(name, 0.0), 2)
    total += per_os[name]  # sum the rounded amounts so the total matches what is shown
  return {"per_os": per_os, "total": round(total, 2)}


def month_window(now=None, month=None):
  """Return (year, month, 'YYYY-MM') for the target calendar month.

  A provided month must be 'YYYY-MM' with a 01-12 month; raises ValueError otherwise.
  """
  if month:
    if not re.fullmatch(r"\d{4}-(0[1-9]|1[0-2])", month):
      raise ValueError(f"--month must be YYYY-MM with a 01-12 month, got {month!r}")
    year, mon = int(month[:4]), int(month[5:7])
  else:
    now = now or datetime.now(timezone.utc)
    year, mon = now.year, now.month
  return year, mon, f"{year:04d}-{mon:02d}"


def _fmt_usd(value):
  return f"${value:,.2f}"


def _actions_block(data, out):
  actions = data.get("actions")
  if actions is None:
    out.append("- **GitHub Actions minutes:** not available (read failed).")
    return
  out.append(
      f"- **GitHub Actions minutes** (window {data['month_label']}, GitHub-hosted runners, "
      f"imputed at the {ACTIONS_RATES_AS_OF} list rates):")
  out.append("")
  out.append("  | Runner OS | Minutes | Jobs | Imputed list $ |")
  out.append("  |-----------|--------:|-----:|---------------:|")
  minutes = actions["minutes"]
  counts = actions["counts"]
  per_os = actions["cost"]["per_os"]
  for name in ("Linux", "Windows", "macOS"):
    if name in minutes:
      out.append(
          f"  | {name} | {minutes[name]:,} | {counts.get(name, 0):,} | "
          f"{_fmt_usd(per_os.get(name, 0.0))} |")
  out.append(f"  | **Total** | | | **{_fmt_usd(actions['cost']['total'])}** |")
  out.append("")
  billed = "  Actually billed: **$0.00** (public-repo standard runners are unbilled)."
  if actions["unknown"]:
    billed += (f" {actions['unknown']} job(s) on self-hosted or unrecognised runners "
               "were excluded from the imputed cost.")
  out.append(billed)


def _line_or_prereq(out, label, value, prereq):
  """Append a configured value line, an error line, or the prerequisite note."""
  if value is None:
    out.append(f"- **{label}:** not configured (prerequisite — {prereq}).")
  elif "error" in value:
    out.append(f"- **{label}:** read error ({value['error']}).")
  else:
    out.append(f"- **{label}:** {value['text']}")


def render_snapshot(data):
  """Render the full marker-bounded snapshot block from collected data."""
  out = [SNAPSHOT_START, ""]
  out.append(
      f"_Generated by `tools/cost_collector.py` on **{data['generated']}** (UTC) for month "
      f"**{data['month_label']}**. Imputed = list price (not actually charged); "
      "actually-billed shown alongside._")
  out.append("")

  ncloc = data.get("sonar_ncloc")
  if ncloc is not None:
    out.append(
        f"- **SonarCloud LOC (`ncloc`):** {ncloc:,} as read on {data['generated']} — "
        "**0 paid LOC** (this is a public project; SonarCloud is free for public projects).")
  else:
    out.append("- **SonarCloud LOC (`ncloc`):** not available (read failed).")

  out.append("")
  _actions_block(data, out)

  out.append("")
  _line_or_prereq(
      out, "GitHub Actions billing cross-check (factor 3)", data.get("billing"),
      "see [Credentials & prerequisites](#credentials--prerequisites)")

  out.append("")
  ai = data.get("ai_review")
  if ai is None:
    out.append(
        "- **AI code review, e.g. Copilot (factor 9):** not yet reported for "
        f"{data['month_label']} — contributed by the maintainer and reported into this ledger "
        "out-of-band (see [Credentials & prerequisites](#credentials--prerequisites)).")
  elif "error" in ai:
    out.append(
        f"- **AI code review, e.g. Copilot (factor 9):** could not read "
        f"`doc/cost-data/ai-review.json` ({ai['error']}).")
  else:
    note = f" {ai['note']}" if ai.get("note") else ""
    out.append(
        f"- **AI code review, e.g. Copilot (factor 9):** {_fmt_usd(ai['usd'])} for "
        f"{data['month_label']} — voluntarily disclosed by the maintainer; GitHub exposes no "
        f"per-repository attribution.{note}")

  out.append("")
  out.append(
      "- **Claude Code tokens (factor 4):** manual / local feed in v1 — run "
      "`npx ccusage@latest --json` or parse `~/.claude` (trailing 30 days); not collected "
      "in CI yet.")

  out.append("")
  out.append(
      "_This collector run itself consumed a few Linux runner-minutes, captured in the "
      "Actions line on the next run._")
  out.append("")
  out.append(SNAPSHOT_END)
  return "\n".join(out)


def replace_snapshot(doc_text, snapshot_block):
  """Replace the text between the snapshot markers (inclusive) with a new block."""
  start = doc_text.find(SNAPSHOT_START)
  # Search for the end marker after the start so it can never precede it.
  end = doc_text.find(SNAPSHOT_END, start) if start != -1 else -1
  if start == -1 or end == -1:
    raise ValueError("snapshot markers not found in document")
  return doc_text[:start] + snapshot_block + doc_text[end + len(SNAPSHOT_END):]


# --------------------------------------------------------------------------- #
# IO (network). Kept thin so tests mock http_get_json / the fetchers.
# --------------------------------------------------------------------------- #
def http_get_json(url, token=None, accept="application/vnd.github+json"):
  """GET a URL and decode the JSON body."""
  request = urllib.request.Request(url)
  request.add_header("User-Agent", USER_AGENT)
  request.add_header("Accept", accept)
  if token:
    request.add_header("Authorization", f"Bearer {token}")
  with urllib.request.urlopen(request, timeout=30) as response:  # noqa: S310 (trusted hosts)
    return json.loads(response.read().decode("utf-8"))


def _paginate(url, key, token):
  """Follow ?page= pagination, collecting data[key] until a short page."""
  results = []
  page = 1
  while True:
    sep = "&" if "?" in url else "?"
    data = http_get_json(f"{url}{sep}per_page=100&page={page}", token=token)
    items = data.get(key, [])
    results.extend(items)
    if len(items) < 100:
      return results
    page += 1


def fetch_sonar_ncloc(component=SONAR_COMPONENT):
  """Read the live ncloc measure of a public SonarCloud project (no auth)."""
  url = f"{SONAR_API}/measures/component?component={component}&metricKeys=ncloc"
  data = http_get_json(url, accept="application/json")
  for measure in data.get("component", {}).get("measures", []):
    if measure.get("metric") == "ncloc":
      return int(measure["value"])
  return None


def fetch_actions_jobs(year, month, token):
  """All jobs of all OWNER/REPO workflow runs created in the calendar month."""
  last = calendar.monthrange(year, month)[1]
  created = f"{year:04d}-{month:02d}-01..{year:04d}-{month:02d}-{last:02d}"
  runs_url = f"{GITHUB_API}/repos/{OWNER}/{REPO}/actions/runs?created={created}"
  jobs = []
  for run in _paginate(runs_url, "workflow_runs", token):
    jobs_url = f"{GITHUB_API}/repos/{OWNER}/{REPO}/actions/runs/{run['id']}/jobs"
    jobs.extend(_paginate(jobs_url, "jobs", token))
  return jobs


def fetch_org_billing(year, month, token):
  """Sum the OWNER usage-report Actions amounts attributed to the REPO repository."""
  # Enhanced billing platform: the usage report lives under /organizations/{login}
  # (NOT the classic /orgs/{org}/settings/billing/actions, which is 410 Gone).
  # Verified live: both /organizations/{login} and /orgs/{login} return 200 with an
  # identical usageItems body, so this does not 404.
  url = f"{GITHUB_API}/organizations/{OWNER}/settings/billing/usage?year={year}&month={month}"
  data = http_get_json(url, token=token)
  gross = net = 0.0
  for item in data.get("usageItems", []):
    if item.get("repositoryName") == REPO and item.get("product", "").lower() == "actions":
      gross += item.get("grossAmount", 0.0)
      net += item.get("netAmount", 0.0)
  return {"text": (f"gross {_fmt_usd(round(gross, 2))} / net {_fmt_usd(round(net, 2))} "
                   f"for {year:04d}-{month:02d} (repository `{REPO}`).")}


def _valid_usd(value):
  """True if value is a non-negative, finite real number (and not a bool)."""
  if isinstance(value, bool) or not isinstance(value, (int, float)):
    return False
  return math.isfinite(value) and value >= 0


def _safe_path(path):
  """Resolve a CLI-supplied path and confine it to the working directory.

  The collector only reads/writes data files inside the checkout (the repo, or the
  cost-data branch checkout). Canonicalising and confining the path neutralises a
  traversal via a faulty --doc/--ai-review argument before any file-system access.
  """
  root = Path.cwd().resolve()
  resolved = (root / path).resolve()
  if not resolved.is_relative_to(root):
    raise ValueError(f"path escapes the working directory: {path}")
  return resolved


def read_ai_review(month_label, path):
  """Read the maintainer-contributed AI-review cost for a month.

  Returns {'usd', 'note'} when a valid figure exists, None when none is reported
  for that month, or {'error': ...} when the data file is missing/unreadable or
  malformed — so the snapshot can distinguish "not yet reported" from "read failed".

  The figure is reported into this repo out-of-band: a repository_dispatch updates
  ai-review.json on the cost-data branch. No billing-account identity or mechanism is
  stored in this public repo — only the voluntarily disclosed monthly amount.
  """
  try:
    payload = json.loads(_safe_path(path).read_text(encoding="utf-8"))
  except (OSError, ValueError) as exc:
    return {"error": str(exc)}  # file missing/unreadable or malformed JSON
  entry = (payload.get("months") or {}).get(month_label)
  if not isinstance(entry, dict):
    return None  # no figure reported for this month yet
  usd = entry.get("usd")
  if not _valid_usd(usd):  # guard a hand-edited / corrupted figure
    return None
  # Collapse whitespace and cap the note so a corrupt/oversized value cannot inject
  # multi-line or runaway content into the rendered public ledger.
  note = " ".join(str(entry.get("note") or "").split())[:200]
  return {"usd": float(usd), "note": note}


# --------------------------------------------------------------------------- #
# Orchestration. IO functions are referenced as module globals so tests can
# monkeypatch them; this also keeps the signature small (no fetcher injection).
# --------------------------------------------------------------------------- #
def _guarded(fetch):
  """Run a fetch, returning its value or an {'error': ...} marker."""
  try:
    return fetch()
  except _READ_ERRORS as exc:
    return {"error": str(exc)}


def collect(now=None, month=None, *, github_token=None, billing_token=None,
            ai_review_path=None):
  """Gather every available cost line for the target month into a dict."""
  now = now or datetime.now(timezone.utc)
  year, mon, label = month_window(now, month)
  data = {"generated": now.strftime("%Y-%m-%d"), "month_label": label}

  try:
    data["sonar_ncloc"] = fetch_sonar_ncloc(SONAR_COMPONENT)
  except _READ_ERRORS:
    data["sonar_ncloc"] = None

  try:
    aggregate = aggregate_actions(fetch_actions_jobs(year, mon, github_token))
    aggregate["cost"] = impute_actions_cost(aggregate["minutes"])
    data["actions"] = aggregate
  except _READ_ERRORS:
    data["actions"] = None

  data["billing"] = (
      _guarded(lambda: fetch_org_billing(year, mon, billing_token))
      if billing_token else None)
  data["ai_review"] = read_ai_review(label, ai_review_path) if ai_review_path else None
  return data


def main(argv=None):
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument("--month", help="target calendar month YYYY-MM (default: current)")
  parser.add_argument("--doc", type=Path, default=None,
                      help="document holding the snapshot block; omit (or pass "
                           "--print) to print instead of writing")
  parser.add_argument("--ai-review", dest="ai_review", type=Path, default=None,
                      help="AI-review data file (ai-review.json); omit to leave the "
                           "AI-review line as 'not yet reported'")
  parser.add_argument("--print", dest="print_only", action="store_true",
                      help="print the snapshot instead of writing the document")
  args = parser.parse_args(argv)
  if args.month and not re.fullmatch(r"\d{4}-(0[1-9]|1[0-2])", args.month):
    # Clean CLI error (exit 2) instead of a traceback from month_window().
    parser.error(f"--month must be YYYY-MM with a 01-12 month, got {args.month!r}")

  data = collect(
      month=args.month,
      github_token=os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN"),
      billing_token=os.environ.get("COST_BILLING_TOKEN"),
      ai_review_path=args.ai_review)
  snapshot = render_snapshot(data)

  if args.print_only or args.doc is None:
    print(snapshot)
  else:
    doc = _safe_path(args.doc)
    updated = replace_snapshot(doc.read_text(encoding="utf-8"), snapshot)
    doc.write_text(updated, encoding="utf-8")
    print(f"Updated snapshot in {doc} (month {data['month_label']}).")

  # Non-zero exit when the collection produced nothing usable (a CI failure signal).
  collected = data.get("sonar_ncloc") is not None or data.get("actions") is not None
  return 0 if collected else 1


if __name__ == "__main__":
  raise SystemExit(main())
