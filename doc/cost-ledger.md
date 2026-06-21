# Cost Ledger

> **Purpose:** record the facts that drive Vox's running costs — at **list price**, with the
> **actually-billed** amount alongside — so the project can report its true cost transparently
> (contributors, sponsors, the association / e.V.) and spot runaway usage early.
> **Status:** v1 ledger (see [issue #76](https://github.com/digitale-barrierefreiheit/vox/issues/76)).
> **How it stays current:** the [snapshot](#snapshot) is refreshed by a scheduled collector
> (`tools/cost_collector.py`, run by the `cost-collector` workflow; preview locally with
> `just cost`). Everything
> else on this page is maintained by hand on the cadence noted below.

Vox consumes resources that cost real money, even though most is currently provided **free or
discounted**: Claude Code (AI assistance), GitHub Actions, GitHub Copilot, SonarCloud, and
CodeScene. Because the repository is public/OSS, almost nothing is *actually billed* today — so
the headline figure is an **imputed list price** (what it *would* cost) shown next to the
**actually-billed** amount (usually €0/$0).

## Methodology

- **Imputed list price vs. actually billed.** "Imputed" means a list-price number computed from a
  published rate that is **not actually charged** (public-repo Actions minutes, OSS-free Sonar /
  CodeScene). Each line shows both so the discount is explicit.
- **Values are read on a date, never frozen.** Every measured figure carries the date it was read
  (e.g. *"ncloc 13,862 as read on 2026-06-16"*). Numbers in prose are **examples**, not permanent
  facts; the live values live in the [snapshot](#snapshot).
- **Date-stamped rate tables.** List prices change. Each rate table below records the date it was
  taken from the vendor page and is re-stamped when it changes. Numbers flagged **(verify)** are
  unconfirmed against the live source and must be re-checked before they are relied upon.
- **Currency.** GitHub bills in **USD**; SonarCloud and CodeScene list in **EUR**. v1 does **not**
  convert between them (no FX assumption); each line is shown in its vendor's currency.
- **Calendar-month aggregation.** GitHub Actions minutes and the GitHub billing cross-check are
  aggregated **per calendar month** (the billing API is month-bucketed). Claude Code tokens are
  likewise reported **per calendar month** — *month-to-date* for the still-open month, settling
  once it closes. The one caveat is a *source* limit, not a different window: the local session
  transcripts that feed the Claude figure are auto-deleted after ~30 days, so it must be collected
  before that retention rolls off. The snapshot states the window for each line.
- **The collector itself costs a little.** The `cost-collector` workflow consumes a few Linux
  runner-minutes per run; that cost is itself captured in the Actions line.

### Rate tables

**GitHub Actions — per-minute list price** (standard GitHub-hosted runners), as read **2026-01-01**:

| Runner OS | List $/min | Used by Vox? |
|-----------|-----------:|--------------|
| Linux     | $0.006     | Yes |
| Windows   | $0.010     | Yes |
| macOS     | $0.062     | **No** — informational only; Vox runs **Linux + Windows** jobs only |

The legacy "2×/10× multiplier" shorthand is no longer exact under per-OS rates — use the $/min
figures directly **(verify on change)**. Public-repo standard-runner minutes are **billed $0**;
the figure above is therefore an **imputed list price**.

**Other list prices** (re-stamp on change; all **(verify)**):

- **Claude Code** — per-model token prices from <https://claude.com/pricing>; cache-create ≈ 25 %
  and cache-read ≈ 10 % of the input price (verify per model). Committed as a date-stamped table
  when the token line is first populated.
- **SonarCloud** — public projects consume **0 paid LOC** (free); a private project is free up to
  **50k LOC**, paid above (2026 gradual-scaling brackets — verify).
- **CodeScene** — billed **per active author** (committed in the last 3 months), **not** per LOC;
  OSS projects are free (confirm Vox's eligibility); Standard €18 / Pro €27 per active author / mo
  (verify).

## Data sources

One row per cost factor with the **resolved** source, auth/scope, granularity, automatable?, and
gap/(verify). Auth verdicts come from live probes unless flagged **(verify)**.

| # | Factor | Source / endpoint | Auth / scope | Granularity | Automatable? | Gap / (verify) |
|---|--------|-------------------|--------------|-------------|--------------|----------------|
| 1 | GitHub Actions minutes (primary, **imputed list price**) | `GET /repos/{owner}/{repo}/actions/runs` + `.../runs/{run_id}/jobs`; per job `ceil((completed_at−started_at)/60s)`, bucket OS from labels, × per-OS list $/min | repo / `public_repo`; **plain `GITHUB_TOKEN` works** | per-job → per-month | **Yes** | `billable.*` is always 0 on public-repo standard runners; `run_duration_ms` is wall-clock. Imputed list price; actually billed $0. **Linux + Windows only**. |
| 2 | Actions list rates / OS multipliers | Manual, date-stamped: <https://docs.github.com/en/billing/reference/actions-runner-pricing> | none | point-in-time | Partial | See rate table above. Legacy 2×/10× no longer exact **(verify)**. |
| 3 | Actions per-repo $ (**billing cross-check**) | `GET /organizations/{org}/settings/billing/usage?year=&month=` (enhanced billing platform — `/organizations/{org}`, **not** `/orgs/{org}`; both verified to return 200); filter `repositoryName=='vox'`, read `grossAmount`/`netAmount`/`quantity` per SKU | **Org Administration: read** — GitHub App installation token (recommended) or fine-grained PAT; **org owner** to grant; billing-manager **not** sufficient | per-day / per-repo / per-SKU | **Yes**, with org-admin-granted identity | Classic `/orgs/{org}/settings/billing/actions` is **410 Gone**. **(verify)** an App installation token returns 200 on this path (not minted live). |
| 4 | Claude Code tokens + $ estimate (**maintainer-contributed**) | Collected locally with `npx ccusage daily --json --instances` (the hook sums the **vox project's** current-month rows and pins a known-good ccusage version — override with `VOX_CCUSAGE_SPEC`; underlying data is `~/.claude/projects/<p>/<session>.jsonl` `usage.*`); reported in as a **month-to-date** total via a `repository_dispatch` (type `claude-cost`) → `claude.json` on the orphan `cost-data` branch. Fired automatically by the opt-in `pre-push` hook (`just install-hooks`; `tools/hooks/pre-push`), or manually via the full `gh api … dispatches` call in [Credentials & prerequisites](#credentials--prerequisites). **Window: calendar month-to-date** (collect before the ~30-day local JSONL retention rolls off). | **none in this repo** (local `ccusage`; no billing creds stored here) | per-message → month-to-date | **Yes** (reported in) | Claude stores sessions **per project**, so unlike Copilot this is **vox-attributable**. JSONL `input_tokens` undercounted ~100× **(verify)**; output & cache accurate; JSONL auto-deleted after 30 d. |
| 5 | Claude price table | Manual, date-stamped: <https://claude.com/pricing> | none | point-in-time | Partial | cache-create ≈25 % / cache-read ≈10 % of input **(verify per model)**. |
| 6 | SonarCloud LOC (billing metric) | `GET https://sonarcloud.io/api/measures/component?component=vox&metricKeys=ncloc` | **none (public)** | latest analysis, per-branch | **Yes** | Record "as read on <date>". Public repo = 0 paid LOC; private free ceiling 50k; brackets **(verify)**. |
| 7 | CodeScene (billing driver = active authors) | `GET https://api.codescene.io/v2/projects/81008` → `analysis.authors.active` | Bearer PAT (Admin / Architect / RestApi) | latest analysis | **Yes**, with token | 401 anon; bills per active author (last 3 mo); OSS free **(confirm eligibility)**; €18 / €27 per author/mo **(verify)**. |
| 8 | Developer time invested | **Opt-in `Effort:` line in the PR body** (source of truth), suggested by a diff-size heuristic, recorded as an issue comment at feature→dev merge | n/a (self-report) | per-issue / PR | **Semi** (suggested; human confirms) | Opt-in, never gates a merge; divisor `150 LOC/h` + clamp `[0.25h, 8h]` + generated-path exclusions all **(verify)**/tunable. |
| 9 | AI code review, e.g. GitHub Copilot (**maintainer-contributed**) | `ai-review.json` (on the orphan `cost-data` branch), updated out-of-band via a `repository_dispatch` (type `ai-review-cost`) sent from the maintainer's separate billing context | **none in this repo** (no third-party billing credentials are stored here) | per-month, project-level | **Yes** (reported in) | GitHub exposes **no per-repository Copilot attribution**; the figure is the maintainer's good-faith monthly disclosure of the AI-review cost attributable to Vox. The billing entity and mechanism are intentionally kept out of this public repo. |
| 10 | GitHub Copilot engagement metrics | `GET /orgs/{org}/copilot/metrics` | owner; `read:org` | per-day engagement | **n/a for cost** | **Engagement telemetry ONLY — never billing, never $, never per-repo.** Not used for the ledger. |
| 11 | *(deferred)* storage / cache / egress | `GET /organizations/{org}/settings/billing/usage` (storage SKU) | org Administration: read | per-day / SKU | only w/ admin | ≈ $0 (negligible); deferred. |

## Open (verify) items — canonical checklist

Each item is either resolved (with date) or annotated `(unverified — subject to change)`:

- [ ] App **installation**-token returns 200 on `GET /organizations/digitale-barrierefreiheit/settings/billing/usage` (docs + prior art support it; not minted live).
- [ ] The AI-review contribution channel (a private collector → `repository_dispatch` `ai-review-cost` → `ai-review.json`) is wired and produces a monthly figure.
- [ ] The Claude-token contribution channel (`repository_dispatch` `claude-cost` → `claude.json`, rendered month-to-date) and its opt-in `pre-push` hook (`just install-hooks`) produce a monthly figure end-to-end.
- [ ] Claude JSONL `input_tokens` ~100× undercount; cache-create ≈25 % / cache-read ≈10 % of input price, per model.
- [ ] SonarCloud paid-LOC brackets / 2026 gradual scaling; CodeScene OSS-free eligibility and Standard €18 / Pro €27 per-author/mo rates.
- [ ] Legacy Actions 2×/10× OS multipliers no longer exact under the 2026-01-01 per-OS $/min rates.
- [ ] Dev-time heuristic: `150 LOC/h` divisor, `[0.25h, 8h]` clamp, generated-path exclusion globs (`vcpkg_installed`, `dist`, `*.lock`, `node_modules`) — tune after a few PRs.
- [ ] Actions Linux figure — confirm against a live billing-usage probe (only the Windows row was probed live: 12,716 min / $127.16 gross for 2026-06).
- [x] Whether CI can reach a Claude session export for the token pull — *resolved: fed in from the maintainer's machine via the `claude-cost` channel (`ccusage` over the vox project), the same out-of-band pattern as factor 9; CI never reads `~/.claude` directly.*

## Snapshot

The live snapshot is refreshed monthly on the orphan **[`cost-data`](https://github.com/digitale-barrierefreiheit/vox/tree/cost-data)** branch — kept independent of `dev`/`main` so a refresh never triggers the build / test / Sonar / benchmark pipelines (the same pattern as the `benchmark-data` and `cla-signatures` branches):

- **[`snapshot.md`](https://github.com/digitale-barrierefreiheit/vox/blob/cost-data/snapshot.md)** — the aggregated cost lines (Sonar `ncloc`, Actions minutes, billing cross-check, AI review, Claude tokens), regenerated by `tools/cost_collector.py`. The billing cross-check shows *"not configured (prerequisite)"* until its secret is wired; the AI-review and Claude lines show *"not yet reported"* until a figure is reported in (the Claude line then reads *month-to-date* for the still-open month).
- **[`ai-review.json`](https://github.com/digitale-barrierefreiheit/vox/blob/cost-data/ai-review.json)** — the raw monthly AI-review figure, fed in via `repository_dispatch` (see [Credentials & prerequisites](#credentials--prerequisites)).
- **[`claude.json`](https://github.com/digitale-barrierefreiheit/vox/blob/cost-data/claude.json)** — the raw monthly Claude-token figure (with an `updated` date), fed in via `repository_dispatch` (see [Credentials & prerequisites](#credentials--prerequisites)).

## Credentials & prerequisites

The auth-free / plain-token lines (Sonar `ncloc`, Actions minutes) run in CI with no setup. The
org-billing cross-check needs an org owner to provision a secret **once** (until then it records
*"not configured (prerequisite)"*). The AI-review line is **reported in** by a private collector
the maintainer runs in their own billing context — this repo holds **no** credentials for it.

| Read | Identity | Setup (one-time) | Secret(s) |
|------|----------|------------------|-----------|
| Org Actions billing cross-check (factor 3) | **Primary:** the `cost-collector` workflow mints a short-lived installation token from an **org-owned GitHub App** (Organization → **Administration: read**) via `actions/create-github-app-token` and uses it for the billing read. **Fallback:** a fine-grained PAT in `COST_BILLING_TOKEN`. With neither, this line shows *"not configured (prerequisite)"*. | Org owner: create a GitHub App with **Administration: read**, install it on the org, then set repo variable `COST_APP_CLIENT_ID` + secret `COST_APP_PRIVATE_KEY`. *(Or set `COST_BILLING_TOKEN` to a fine-grained PAT with Administration: read.)* | var `COST_APP_CLIENT_ID` + secret `COST_APP_PRIVATE_KEY` *(or `COST_BILLING_TOKEN`)* |
| AI code review (factor 9) | A **private collector** the maintainer runs in their own billing context. This public repo stores no billing identity, endpoint, or token — only the disclosed monthly figure. | Maintainer: wire the private collector to POST a `repository_dispatch` (type `ai-review-cost`, payload `{ month, usd, note? }`); `cost-contribution.yml` records it into `ai-review.json` on the orphan `cost-data` branch. | none here (the sender holds its own token) |
| Claude Code tokens (factor 4) | The maintainer's **local machine** (`ccusage` over the vox project). This public repo stores no billing identity or token — only the disclosed month-to-date figure. | Maintainer: `just install-hooks` enables the opt-in `pre-push` hook, which fires a `repository_dispatch` (type `claude-cost`, payload `{ month, usd, note? }`) on every push; `cost-contribution.yml` upserts it into `claude.json` on the orphan `cost-data` branch. Manual equivalent: `gh api repos/digitale-barrierefreiheit/vox/dispatches -f event_type=claude-cost -f 'client_payload[month]=YYYY-MM' -f 'client_payload[usd]=12.34'`. Dry-run the hook with `sh tools/hooks/pre-push --dry-run`. | none here (the sender uses the maintainer's own `gh` auth) |

Recommended pre-implementation smoke tests are listed in
[issue #76](https://github.com/digitale-barrierefreiheit/vox/issues/76).

## Refresh cadence

- **Automated lines** (Sonar `ncloc`, Actions minutes, and — once wired — the billing cross-check):
  refreshed **monthly** by the `cost-collector` workflow (and on demand via `workflow_dispatch`;
  preview locally with `just cost`). The **AI-review** and **Claude-token** lines are reported in
  separately via `repository_dispatch` (the Claude line as a month-to-date running total, fired by
  the opt-in `pre-push` hook — `just install-hooks` — on every push).
- **Manual lines** (rate tables, Claude price table, CodeScene): reviewed **quarterly** or when a
  vendor changes prices; re-stamp the date.
- **Developer time:** accrues per merged PR (opt-in), read back from issue comments.
