window.BENCHMARK_DATA = {
  "lastUpdate": 1781302237418,
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
          "id": "dbee5a0c23cf842010d88f0a78700c21adf47472",
          "message": "feat(tts): discover OneCore SAPI voices, not just classic SPCAT_VOICES (#52)\n\nVoices installed through Windows Settings / language packs register only under\nSpeech_OneCore, invisible to classic SPCAT_VOICES enumeration — a user's German\nvoice could be undiscoverable, and PreferGerman silently fell back to English\n(defeating ADR-07). The engine now enumerates both categories with the same\nmachinery (OneCore tokens drive ISpVoice unchanged) and merges them through the\nnew pure mergeVoices(): classic wins on duplicate names (token ids differ\nacross hives, names are the identity — exactly the registry-bridge case),\nunnamed voices never collapse, and the classic default stays the system\ndefault; selectVoice remains the single decision point.\n\nTests: 8 pure merge tests; engine seam tests for OneCore-only discovery and\nthe classic-then-OneCore category order. The OneCore->classic registry-bridge\nsteps are removed from CI: the runner's German voice now exists only in the\nOneCore hive, so VOX_REQUIRE_GERMAN_VOICE genuinely gates that OneCore\ndiscovery works. ADR-07 documents the resolution.\n\nCloses #52.",
          "timestamp": "2026-06-11T21:52:55+02:00",
          "tree_id": "c948f65a844e90ace0bd7dec250182d94d6eb5a2",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/dbee5a0c23cf842010d88f0a78700c21adf47472"
        },
        "date": 1781208622118,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
            "value": 0.7,
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
          "id": "3d1c61d9b3fd6287202a89405ef7b630651fcd50",
          "message": "feat(provider): make UIA focus-handler unregister/re-register robust (#60)\n\nstop() correctness no longer depends on UIA's RemoveFocusChangedEventHandler\nsucceeding. The focus sink is detachable: stop() neutralizes the callback\nbefore attempting removal, so every event reaching the sink afterwards is\nswallowed, whatever HRESULT UIA answers; the callback is copied under the\nsink mutex but invoked outside it (a callback that itself stops the provider\ncannot self-deadlock), and any invocations already past their callback copy\nare dropped by the Reader's focus guard (detached before provider_.stop()).\nA handler whose removal fails is shelved (bounded at 8 — beyond that,\neverything is unregistered at once) instead of blocking the slot, so a\nstop()/start() cycle reliably resumes notifications; every stop() retries\nshelved removals and the allocation-free teardown escalates to\nRemoveAllEventHandlers on the provider's private IUIAutomation instance.\n\nTests: six unit tests on the mock seam (silenced-after-failed-removal,\nfresh-handler-after-failed-removal, retry-and-release, two destructor\nescalation variants, bounded-shelf escalation) replace the test that pinned\nthe old deficient behavior; an integration test drives the real UIA stack\nvia the #40 test app — events flow after start(), stay silent through five\nfocus cycles after stop(), and resume after a restart.\n\nCloses #60.",
          "timestamp": "2026-06-12T11:28:09+02:00",
          "tree_id": "490f6c6cad68729b53101c885e8212a8ee7b329c",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/3d1c61d9b3fd6287202a89405ef7b630651fcd50"
        },
        "date": 1781257483891,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
            "value": 0.7,
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
          "id": "5e7d22a92c3bb4a2610681750592f92349dfd273",
          "message": "feat(app): load the lexicon from per-language files, fall back to the embedded German default (#61)\n\nEvery .lex table declares its language (language = <BCP-47 tag>); Lexicon::parse exposes it while staying filesystem-free. The app-layer loader resolves VOX_LEXICON (authoritative), then lexicon\\<VOX_LANGUAGE>.lex next to the executable (default de), and falls back to the embedded German default on any failure - a file replaces the default wholesale, every fallback is reported on stderr, and the reader always speaks. Hardened per review: any-length env values, regular-files-only opens (no device names/directories), no CWD-relative lookups, never-throwing diagnostics. Ships en.lex as the English reference; the build copies data/lexicon next to vox.exe.",
          "timestamp": "2026-06-12T23:53:09+02:00",
          "tree_id": "e3c1681208f1b17476e693c88f62c0f87c8e247b",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/5e7d22a92c3bb4a2610681750592f92349dfd273"
        },
        "date": 1781302236957,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
            "value": 0.7,
            "unit": "us"
          }
        ]
      }
    ]
  }
}