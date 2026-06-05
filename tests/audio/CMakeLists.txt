# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Tests over the portable PCM seam (AudioFormat + PcmBuffer).
vox_add_test(
  NAME audio_test
  SOURCES
    audio_format_test.cpp
    pcm_buffer_test.cpp
  LINK vox::audio)

set_target_properties(audio_test PROPERTIES FOLDER "tests")
