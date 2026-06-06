# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# End-to-end tests over the portable Reader: focus -> German utterance -> TTS ->
# audio, driven entirely through fakes (no UIA/SAPI/WASAPI). The Reader runs a
# synthesis worker thread, so the test needs Threads.
find_package(Threads REQUIRED)

vox_add_test(
  NAME app_test
  SOURCES reader_test.cpp
  LINK vox::app vox::german vox::testing Threads::Threads)

set_target_properties(app_test PROPERTIES FOLDER "tests")
