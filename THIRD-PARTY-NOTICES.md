# Third-party notices

Vox **bundles no third-party libraries.** The `vox` executable uses only
Windows system APIs (UI Automation, SAPI, WASAPI, Win32) and the Microsoft C/C++
runtime, which is statically linked (ADR-08) and provided under the Microsoft
Windows SDK / Visual C++ runtime terms.

Build- and test-time dependencies — **GoogleTest** (tests) and **Google Benchmark**
(the optional benchmark build) — are **not** distributed in the release artifact.

Future releases that bundle neural TTS (**ONNX Runtime**) and the **espeak-ng** G2P
worker (ADR-16) will list those components and their licences here.
