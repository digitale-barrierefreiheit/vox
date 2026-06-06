// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only test seam over WasapiAudioSink's COM device creation.
///
/// `WasapiAudioSink` acquires its audio device by creating an
/// `IMMDeviceEnumerator` and walking enumerator -> device -> client. This seam
/// lets a test substitute that root creation with a factory that returns a *mock*
/// `IMMDeviceEnumerator` (or a failure `HRESULT`), so the sink's whole
/// device-acquisition path — including every `DeviceError` it throws — is
/// unit-tested with no real audio device (ADR-12, issue #68). Production code
/// never sets a factory and keeps using `CoCreateInstance`.
#ifndef VOX_AUDIO_WASAPI_TEST_SEAM_HPP
#define VOX_AUDIO_WASAPI_TEST_SEAM_HPP

#if defined(_WIN32)

#  include <cstddef>
#  include <functional>

// Forward-declared so this header needs no Windows SDK / audio headers.
struct IMMDeviceEnumerator;
struct IAudioClient;
struct IAudioRenderClient;

namespace vox::audio {

class PcmRing;

namespace testing {

/// @brief Creates the enumerator the sink will use. Mirrors `CoCreateInstance`:
///        returns an `HRESULT` (as `long`) and, on success, sets `*out` to an
///        AddRef'd interface. Return a failure code to exercise the error path.
using EnumeratorFactory = std::function<long(IMMDeviceEnumerator** out)>;

/// @brief Installs @p factory for the next `WasapiAudioSink::start()`; an empty
///        factory restores the real `CoCreateInstance`. Test-only, not thread-safe.
void setEnumeratorFactory(EnumeratorFactory factory);

} // namespace testing

namespace detail {

/// @brief Renders one device buffer's worth of audio: queries padding, acquires
///        the WASAPI buffer, copies from @p ring, and releases it (silent on an
///        empty ring, zero-padded on a partial underrun). This is the render
///        thread's per-tick step, factored out so its silent / partial-underrun /
///        full-copy branches are unit-testable with mock COM and no real device
///        (issue #68). @p bufferFrameCount and @p frameBytes describe the device
///        buffer geometry.
void renderDeviceBuffer(IAudioClient& client, IAudioRenderClient& renderClient, PcmRing& ring,
                        unsigned int bufferFrameCount, std::size_t frameBytes);

} // namespace detail

} // namespace vox::audio

#endif // defined(_WIN32)

#endif // VOX_AUDIO_WASAPI_TEST_SEAM_HPP
