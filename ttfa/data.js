window.BENCHMARK_DATA = {
  "lastUpdate": 1781206820377,
  "repoUrl": "https://github.com/digitale-barrierefreiheit/vox",
  "entries": {
    "TTFA pipeline": [
      {
        "commit": {
          "author": {
            "email": "79368115+thomas-ej-worm@users.noreply.github.com",
            "name": "Thomas Worm",
            "username": "thomas-ej-worm"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e96c3a95305049d6308bcfcb8edc16e7af50b404",
          "message": "feat(perf): time-to-first-audio benchmark with percentile budgets + CI gate (#41)\n\nBootstraps the §8.6.4 performance gate. Two benchmarks over one harness: the\npipeline benchmark drives the real Reader/OutputManager over the deterministic\nfakes (focus event -> first PCM write at a timeout-bound instrumented sink) and\nenforces p99 <= 10% of the 200 ms Q1 budget in-process; the real-SAPI benchmark\nmeasures synthesize() to the first delivered chunk (warmed up, cancelled per\ncycle, exception-guarded) against the full Q1 budget (p50 <= 200 ms), opt-in\nvia VOX_BENCH_SAPI / VOX_REQUIRE_TTFA_BUDGET. Both report true per-sample\np50/p99/p99.9; budget violations exit non-zero.\n\nWiring: benchmarks/ behind VOX_BUILD_BENCHMARKS + the 'benchmarks' vcpkg\nfeature; x64-msvc-bench / linux-clang-bench presets (Release); just bench;\nclang-tidy covers benchmarks/. CI: a read-only benchmarks job (German voice\nprovisioned as in windows-de-DE) runs the budgets, posts a per-run results\nsummary + PR comment (the fourth §8.6.7 comment family), and compares the\npipeline percentiles against the dev baseline (benchmark-data branch, 200%\nalert) via github-action-benchmark; a separate contents:write\nbenchmarks-baseline job (dev pushes only, gated on success) publishes the\nbaseline. Measured on CI: pipeline p99 ~1-29 µs; SAPI first chunk p50 ~2 ms.\n\nCloses #41.",
          "timestamp": "2026-06-11T07:08:50+02:00",
          "tree_id": "ca435d952cdb1eb5a97bcf724b18658c52cac374",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/e96c3a95305049d6308bcfcb8edc16e7af50b404"
        },
        "date": 1781155576033,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
            "value": 0.4,
            "unit": "us"
          },
          {
            "name": "ttfaPipeline p99.9",
            "value": 8.3,
            "unit": "us"
          },
          {
            "name": "ttfaPipeline p99",
            "value": 0.5,
            "unit": "us"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "79368115+thomas-ej-worm@users.noreply.github.com",
            "name": "Thomas Worm",
            "username": "thomas-ej-worm"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3d8c3badf0b646e9591a38c27119d5c383cb4159",
          "message": "fix(ci): benchmark relative gate on the median only; pin *.jq to LF (#41)\n\nThe first PRs after the baseline existed tripped the relative benchmark gate on\npure runner noise: pipeline tail percentiles varied up to 8x between runs of\nidentical pipeline code (p99 0.5 -> 4.1 us), while the absolute budgets held\nwith ~5000x headroom and the medians stayed stable (0.5-1.0 us). At us scale,\ntails on shared hosted runners are scheduler-dominated; a relative tail gate is\na coin flip. The action now compares only the pipeline p50 (at 300%); the tails\nremain protected by the absolute in-process budget (p99 <= 10% of Q1, the\nauthoritative gate per 8.6.4) and reported in the per-run results comment.\nAlso pins *.jq to LF in .gitattributes (jq's lexer rejects CRLF checkouts).",
          "timestamp": "2026-06-11T21:20:41+02:00",
          "tree_id": "e0d6f996938fe82e3c16fc9d1afcd6654b1f2210",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/3d8c3badf0b646e9591a38c27119d5c383cb4159"
        },
        "date": 1781206820070,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
            "value": 0.6,
            "unit": "us"
          }
        ]
      }
    ]
  }
}