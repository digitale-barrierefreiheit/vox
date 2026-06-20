# vox cost-data (orphan branch)

This branch holds the **auto-refreshed cost-ledger data**, kept independent of `dev`/`main`
so refreshing it never triggers the build / test / Sonar / benchmark pipelines — the same
pattern as the [`benchmark-data`](https://github.com/digitale-barrierefreiheit/vox/tree/benchmark-data)
and [`cla-signatures`](https://github.com/digitale-barrierefreiheit/vox/tree/cla-signatures) branches.

- **`ai-review.json`** — the maintainer-contributed monthly AI code-review (e.g. Copilot) cost,
  written by `.github/workflows/cost-contribution.yml` on a `repository_dispatch`.
- **`snapshot.md`** — the aggregated cost-ledger snapshot, regenerated monthly by
  `.github/workflows/cost-collector.yml` (`tools/cost_collector.py`).

**Methodology, the data-sources table, and the `(verify)` checklist live in
[`doc/cost-ledger.md`](https://github.com/digitale-barrierefreiheit/vox/blob/main/doc/cost-ledger.md)
on the code branches** (issue #76). Both files here are machine-written — do not edit by hand.

This branch has no shared history with the code; it is never built, tested, or merged.
