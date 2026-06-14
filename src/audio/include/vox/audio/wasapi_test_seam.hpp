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

/// @brief Replaces the render thread's event wait with @p waitFn, which returns
///        the wait result (a `WaitForSingleObject` code, e.g. `WAIT_OBJECT_0` or
///        `WAIT_TIMEOUT`). Lets a test drive the render loop's timeout branch
///        deterministically — no real wait, no sleep. An empty function restores
///        the real wait. Test-only, not thread-safe.
using RenderWaitFn = std::function<unsigned long()>;
void setRenderWaitFn(RenderWaitFn waitFn);

/// @brief Replaces the sink's `CoInitializeEx` with @p initFn, which returns the
///        `HRESULT` (as `long`) the apartment guard then checks. Lets a test
///        exercise the COM-initialization-failed branch — on the `start()`/`stop()`
///        thread or, by failing a later call, on the render thread — without
///        actually breaking COM for the process. The function is called once per
///        apartment the sink creates. An empty function restores the real
///        `CoInitializeEx`. Test-only, not thread-safe.
using ComInitFn = std::function<long()>;
void setComInitFn(ComInitFn initFn);

/// @brief When @p failFn returns `true`, the sink's `CreateEventW` is faulted (it
///        yields a null handle, as the real call does on failure), driving the
///        "cannot create the render event" branch deterministically. An empty
///        function restores the real `CreateEventW`. Test-only, not thread-safe.
using FailCreateEventFn = std::function<bool()>;
void setFailCreateEventFn(FailCreateEventFn failFn);

} // namespace testing

namespace detail {

/// @brief The device buffer's geometry: how many frames it holds and how many
///        bytes each frame is. Bundled so renderDeviceBuffer takes one descriptor
///        rather than two loosely-related scalars.
struct DeviceBufferLayout {
  unsigned int frameCount; ///< Total frames the device buffer holds.
  std::size_t frameBytes;  ///< Bytes per frame (channels * bytes-per-sample).
};

/// @brief Renders one device buffer's worth of audio: queries padding, acquires
///        the WASAPI buffer, copies from @p ring, and releases it (silent on an
///        empty ring, zero-padded on a partial underrun). This is the render
///        thread's per-tick step, factored out so its silent / partial-underrun /
///        full-copy branches are unit-testable with mock COM and no real device
///        (issue #68). @p layout describes the device buffer geometry.
void renderDeviceBuffer(IAudioClient& client, IAudioRenderClient& renderClient, PcmRing& ring,
                        DeviceBufferLayout layout);

} // namespace detail

} // namespace vox::audio

#endif // defined(_WIN32)

#endif // VOX_AUDIO_WASAPI_TEST_SEAM_HPP
