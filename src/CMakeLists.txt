# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# First-party source tree.
#
# The architecture (ADR-12) splits the system into OS-independent "portable
# cores" (ring, codec, German front-end, resync, braille translation — TDD'd and
# sanitizer-covered) and Windows "OS glue" (providers, injection, WASAPI). As
# those modules land they each become a small library under this directory with
# its own CMakeLists and a `vox::<module>` alias. The single `core` library
# below is the seed: it anchors the build, namespace, and include layout.
add_subdirectory(core)

# vox::model — normalized, provider-independent Accessibility Object Model.
add_subdirectory(model)

# vox::german — German announcement vocabulary + minimal normalization.
add_subdirectory(german)

# vox::output — Output Manager v0 + inspectable Utterance seam.
add_subdirectory(output)

# vox::provider — accessibility provider seam (UIA today; portable mapper, COM glue).
add_subdirectory(provider)

# vox::audio — small PCM seam (AudioFormat + PcmBuffer) shared by TTS and audio.
add_subdirectory(audio)

# vox::tts — text-to-speech seam (ITtsEngine; portable selection, SAPI5 glue).
add_subdirectory(tts)
