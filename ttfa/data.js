window.BENCHMARK_DATA = {
  "lastUpdate": 1781473906311,
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
          "id": "4861c7f3148f2cfba39b855b8702bf9d0ae72c7d",
          "message": "fix(app): include windows.h after the vox headers (Sonar S1117 on dev)\n\nwindows.h drags in a global enumerator Unknown (winioctl.h, _MEDIA_TYPE); the #61 composition-root TUs were the only ones including it before the vox headers, making Sonar read Role::Unknown / Source::Unknown as shadowing it. windows.h now follows the vox includes (repo convention), restoring the order under which nothing is shadowed.",
          "timestamp": "2026-06-13T00:58:25+02:00",
          "tree_id": "eda19e4fb2fd911ea7f05a2f275723caee4a3008",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/4861c7f3148f2cfba39b855b8702bf9d0ae72c7d"
        },
        "date": 1781306181016,
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
          "id": "ba9bf0a5be9fe82b7ecc21d7b8c2b4fbf594f608",
          "message": "feat(tts+app): couple voice and lexicon language via VOX_LANGUAGE, with per-part overrides (#88)\n\nfeat(tts+app): couple voice and lexicon language via VOX_LANGUAGE, with per-part overrides (#88)\n\nOne requested language (VOX_LANGUAGE, BCP-47 tag, default de) drives both\nthe TTS voice and the announcement lexicon. Voice selection is now\nrequest-driven: VoiceDescriptor/SelectedVoice carry a language tag (SAPI\nLANGID mapped to a BCP-47 primary subtag) instead of an isGerman flag,\nand selectVoice prefers a primary-subtag match, keeps the unchanged\nfallback chain, and records provenance for the app to report.\n\nPer-part overrides win over VOX_LANGUAGE: VOX_VOICE selects a named voice\n(missing name goes straight to the fallback), and VOX_LEXICON's declared\nlanguage stands even against a diverging VOX_LANGUAGE (a warning, not a\nrejection). Fallbacks are untouched; every fallback or divergence is\nreported on stderr by the app, while the tts and lexicon modules expose\noutcomes and do no I/O.\n\nHardened across review: case-insensitive primary-subtag matching on both\nthe voice and lexicon sides, utf8FromWide shrinks to the bytes actually\nwritten, voiceless voices are labelled by id, and isLanguageTag validates\nthe BCP-47 tag shape rather than just its character set.",
          "timestamp": "2026-06-13T12:24:20+02:00",
          "tree_id": "756e5fce6212de1fa8c79d404d356d16942fbe8b",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/ba9bf0a5be9fe82b7ecc21d7b8c2b4fbf594f608"
        },
        "date": 1781347270804,
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
          "id": "c86119c419eb34edf543d3c223b0ff520f151547",
          "message": "feat(audio): windowed-sinc polyphase resampler for PcmConverter (#55)\n\nReplace PcmConverter's linear interpolation with a Kaiser-windowed sinc polyphase FIR (32 taps, 256 phases, beta 9): first-image suppression improves -32 dB -> -88 dB, convert() stays allocation-free, ratio-1 is an exact zero-delay passthrough.\n\nThe FIR's 16-sample group delay would clip each utterance's tail, so this also adds an end-of-stream drain seam: PcmConverter::drain(), a pure-virtual IAudioSink::drain(), a WasapiAudioSink implementation (converter generation + re-arm so a barge-in resets the FIR), and a Reader drain skipped on shutdown.",
          "timestamp": "2026-06-13T19:53:41+02:00",
          "tree_id": "99079d4c8c6e3a6f513592a24ddb8b96a3f7284f",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/c86119c419eb34edf543d3c223b0ff520f151547"
        },
        "date": 1781374129927,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
            "value": 1,
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
          "id": "d1eb53e4a083c57028842aa2bb96a71ea21a89ba",
          "message": "fix(ci): test-summary shows the check only once all expected jobs report (#65)\n\nThe test-results PR comment rendered a green \"All passed\" as soon as the jobs that had reported so far were clean, so a partial run — or a job that died before publishing JUnit — read as all-green. The verdict now keys off the expected job set seeded by the init step (x64, x86, de-DE, asan, tsan): the check appears only once every expected job has reported and none failed; an hourglass while any are still pending; a cross as soon as a job fails or comes back without results.\n\n- Set-based completeness (not a bare count); a no-JUnit job is recorded as an 'unavailable' cross; the init 'jobs' input is trimmed + de-duplicated; the X/Y counter reads 'expected jobs reported' when seeded.\n- The action's IO was extracted behind injectable seams (actions-io.ts: makeIo/liveDeps/main, comment.ts liveClient, util.ts errorMessage) so the entry logic is unit-tested — action coverage 88% -> 100% line with no exclusions.",
          "timestamp": "2026-06-13T23:42:52+02:00",
          "tree_id": "df611e0a8aaa02c903eed520261816e93169c97c",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/d1eb53e4a083c57028842aa2bb96a71ea21a89ba"
        },
        "date": 1781387841886,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
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
          "id": "8fa26d7c2bad4e2f382a9a25732f732927ea04f3",
          "message": "docs(requirements): capture milestone-1 discovered requirements as REQ-001..020 (#83)\n\nCapture milestone 1's discovered domain/business requirements as 20 REQ-<id>\ndocuments (17 implemented, 3 pending) under doc/requirements/, each a single\ntestable \"shall\" traced to its verifying tests/benchmarks, quality goals, and\noriginating issues; replace the README placeholder with a grouped index.\n\nPending: REQ-009 number-to-words integration (#34), REQ-019 cached/continuous\nlatency (#41), REQ-020 dogfooding (#42). The offline-synthesis constraint\n(C6/ADR-16) is intentionally deferred to the neural-worker milestone.",
          "timestamp": "2026-06-14T01:27:53+02:00",
          "tree_id": "f36e7f8d7be0847020b6959bff70a24ceeae1d6f",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/8fa26d7c2bad4e2f382a9a25732f732927ea04f3"
        },
        "date": 1781394221613,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
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
          "id": "803d1ca4af9ca5c0fb2927e9a548942cf6fa60d5",
          "message": "build(release): first-release preparation — branding, metadata, changelog, packaging & workflow (#103)\n\nPrepare Vox for its first dev->main release (#103):\n\n- Branding: the Vox logo (transparent) + mark-only variant + multi-resolution\n  vox.ico; logo in the README.\n- Executable metadata: generated vox.rc with VERSIONINFO + application icon +\n  LICENSE/NOTICE embedded by reference; UTF-8 (#pragma code_page).\n- Version reset to 0.0.0 (the first release bumps patch to 0.0.1).\n- CHANGELOG.md (Keep a Changelog) on the [Unreleased] promotion model.\n- Packaging: CMake install() + CPack x64 ZIP (incl. data/lexicon, licences) +\n  SHA-256; THIRD-PARTY-NOTICES.\n- Automated release workflow (prepare/gate/publish) reusing the back-merge deploy\n  key, with tested helper scripts (prepare-release.sh, changelog-section.sh) and\n  RELEASING.md.\n\nFollow-ups: code signing (#104), application manifest (#105).",
          "timestamp": "2026-06-14T11:28:55+02:00",
          "tree_id": "e1f9ca7d8cefba9e909f18a59b881571afac73de",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/803d1ca4af9ca5c0fb2927e9a548942cf6fa60d5"
        },
        "date": 1781430302923,
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
          "id": "08bfe4ca79486e2c90ede968f2e15185658681d2",
          "message": "refactor: raise CodeScene code health across M1 hotspots; cover error paths (#108)\n\nBehaviour-preserving refactors that raise CodeScene code health across the M1\nhotspots, with the tests to keep new-code coverage at 100%.\n\n- Portable hotspots (default_app, lexicon_loader, lexicon, role, mapper,\n  output_manager, uia_provider, keyboard_hook, reader, pcm_converter, render.ts)\n  refactored toward ~10.0; OS-glue (wasapi, sapi) kept at their first-pass\n  scores where pushing further would only expose untestable error paths.\n- Fix the findings the refactor surfaced (no suppressions): clang-tidy\n  prefer-member-initializer / qualified-auto, Sonar S7035; validate before\n  dividing in PcmConverter.\n- Cover the remaining error paths properly: a render-wait seam for the render\n  loop's WAIT_TIMEOUT branch and a single-shot nothrow operator-new injection\n  for the Make<> OOM branch; exclude one unreachable catch-brace that\n  OpenCppCoverage reports uncovered by design (issue #121).\n\n100% new-code coverage, 0 Sonar issues, CodeScene improved, all gates green.",
          "timestamp": "2026-06-14T18:22:31+02:00",
          "tree_id": "292d2f0946fa54bd613a0fd9b06a6b1e3c834c80",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/08bfe4ca79486e2c90ede968f2e15185658681d2"
        },
        "date": 1781455069498,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
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
          "id": "cb74e06a5443644c16a861892288886d84445ea2",
          "message": "test+refactor: raise main-branch coverage to 100% and finish the CodeScene 10.0 pass (#109)\n\nCloses the remaining main-branch release blockers (#107):\n\n- Raise whole-codebase coverage 96.24% -> 100% with focused unit tests for\n  OS-glue / COM error paths, using vox::testing fault-injection seams (#68):\n  enumerator / render-wait / CoInitializeEx / CreateEventW and an\n  operator-new OOM seam for the SAPI output stream.\n- Push the 5 remaining hotspots to 10.0 Code Health: reader, pcm_converter,\n  wasapi_audio_sink, mapper, sapi_tts_engine (guard-clause restructuring,\n  argument bundling, helper extraction).\n- Resolve the 10 Sonar issues from the coverage pass (S6004, S5008, S3574,\n  S6232, S1186, ...) and 3 Copilot findings (deterministic CreateEventW last\n  error, ComInitFn init/uninit-balance contract doc, renderDeviceBuffer\n  underflow guard) -- no suppressions.\n- 39 audited LCOV_EXCL_LINE exclusions limited to genuinely-untestable\n  defensive guards and OS-callback glue.\n\nCodeScene (dev) and SonarCloud gates pass; new-code coverage 100%, 0 issues.",
          "timestamp": "2026-06-14T23:35:34+02:00",
          "tree_id": "cafa3416ebe459b0a987cc2c518f18571f2bb406",
          "url": "https://github.com/digitale-barrierefreiheit/vox/commit/cb74e06a5443644c16a861892288886d84445ea2"
        },
        "date": 1781473905596,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "ttfaPipeline p50",
            "value": 0.9,
            "unit": "us"
          }
        ]
      }
    ]
  }
}