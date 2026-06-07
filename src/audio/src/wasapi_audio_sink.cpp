// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Windows WASAPI implementation of vox::audio::WasapiAudioSink.
///
/// `write()` (producer thread) resamples to the device mix format and pushes
/// bytes into the lock-free PcmRing. A dedicated event-driven render thread
/// copies ring -> device buffer and zero-fills on underrun — no allocation, no
/// lock, no exception escaping. `flush()` bumps a generation (so a blocked
/// producer abandons stale audio) and signals the render thread to Reset the
/// device and clear the ring, giving barge-in within one buffer period.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/errors.hpp>
#include <vox/audio/pcm_converter.hpp>
#include <vox/audio/pcm_ring.hpp>
#include <vox/audio/wasapi_audio_sink.hpp>
#include <vox/audio/wasapi_test_seam.hpp>

// The Windows audio headers are include-order sensitive (windows.h must lead),
// so this block is exempt from clang-format's include sorting. NOMINMAX keeps
// the min/max macros from clobbering std::min/std::max.
// clang-format off
#define NOMINMAX
#include <Windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#pragma warning(push)
#pragma warning(disable : 4265)  // WRL: non-virtual dtor in a system header
#include <wrl/client.h>
#pragma warning(pop)
// clang-format on

namespace vox::audio {

namespace {

using Microsoft::WRL::ComPtr;

/// Test seam (issue #68): the installed factory, if any, replaces CoCreateInstance
/// for the device enumerator so device acquisition and its error paths are
/// unit-tested with mock COM objects. Empty in production.
std::function<long(IMMDeviceEnumerator**)>& enumeratorFactory() {
  static std::function<long(IMMDeviceEnumerator**)> factory;
  return factory;
}

/// Creates the device enumerator — via the test factory when one is installed,
/// otherwise the real CoCreateInstance.
HRESULT createDeviceEnumerator(IMMDeviceEnumerator** out) {
  if (const auto& factory = enumeratorFactory()) {
    return static_cast<HRESULT>(factory(out));
  }
  return ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(out));
}

/// The HRESULT to report when a COM call "succeeded" but left its out-param null:
/// keep a genuine failure code, but turn a deceptive S_OK into E_POINTER so the
/// thrown DeviceError carries a meaningful native code.
HRESULT effectiveError(HRESULT hr) {
  return FAILED(hr) ? hr : E_POINTER;
}

/// Owns a device mix format's CoTaskMem allocation, freed on every exit path.
using MixFormat = std::unique_ptr<WAVEFORMATEX, decltype(&::CoTaskMemFree)>;

/// Throws DeviceError carrying the native code when a device call failed or left
/// its out-param empty. Collapsing the repeated "check HRESULT (and null) then
/// throw" lets acquireDevice() read as a flat sequence of steps.
void throwOnFailure(HRESULT hr, const char* message, bool gotOutput = true) {
  if (FAILED(hr) || !gotOutput) {
    throw DeviceError(static_cast<std::uint32_t>(effectiveError(hr)), message);
  }
}

/// Ring capacity as a fraction of a second of device audio — enough to absorb
/// synthesis bursts without adding noticeable latency.
constexpr double RingSeconds = 0.5;

/// Floor on the ring capacity, in device buffer periods, so it is never sized
/// below what bridges normal render-thread scheduling jitter.
constexpr std::size_t MinBufferPeriods = 4;

/// How long the render thread waits on the audio event before re-checking stop.
constexpr DWORD RenderWaitMs = 200;

/// RAII for the COM apartment: CoUninitialize iff this object initialized it.
/// (Mirrors the one in the SAPI backend; each OS-glue TU stays self-contained.)
class ComApartment {
public:
  ComApartment() {
    const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
      // The thread is already an STA. WASAPI interfaces created here would be
      // STA-bound yet called from the MTA render thread without marshaling
      // (undefined), so we require an MTA or COM-uninitialized thread instead.
      throw DeviceError(
          static_cast<std::uint32_t>(hr),
          "WasapiAudioSink: requires an MTA or COM-uninitialized thread (current is STA)");
    }
    if (FAILED(hr)) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: COM initialization failed");
    }
    initialized_ = true;
  }

  ~ComApartment() {
    if (initialized_) {
      ::CoUninitialize();
    }
  }

  ComApartment(const ComApartment&) = delete;
  ComApartment& operator=(const ComApartment&) = delete;
  ComApartment(ComApartment&&) = delete;
  ComApartment& operator=(ComApartment&&) = delete;

private:
  bool initialized_{false};
};

/// Maps a device mix format to the sample encoding we must produce for it.
SampleFormat detectSampleFormat(const WAVEFORMATEX& format) {
  WORD tag = format.wFormatTag;
  if (tag == WAVE_FORMAT_EXTENSIBLE) {
    // Only read the extensible fields if the struct is actually that large; a
    // malformed mix format would otherwise be an out-of-bounds read.
    if (format.cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
      throw DeviceError("WasapiAudioSink: malformed extensible mix format");
    }
    // The extensible SubFormat's Data1 carries the underlying tag (1 = PCM,
    // 3 = IEEE float), so we avoid depending on the KSDATAFORMAT GUID symbols.
    const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
    tag = static_cast<WORD>(extensible.SubFormat.Data1);
  }
  if (tag == WAVE_FORMAT_IEEE_FLOAT && format.wBitsPerSample == 32U) {
    return SampleFormat::Float32;
  }
  if (tag == WAVE_FORMAT_PCM && format.wBitsPerSample == 16U) {
    return SampleFormat::Int16;
  }
  throw DeviceError("WasapiAudioSink: unsupported device mix format");
}

} // namespace

namespace testing {
void setEnumeratorFactory(EnumeratorFactory factory) {
  enumeratorFactory() = std::move(factory);
}
} // namespace testing

namespace detail {
// The render thread's per-tick step, factored out of the loop so its branches
// (silent / partial-underrun / full-copy) are unit-testable with mock COM and no
// real device (issue #68). Declared in wasapi_test_seam.hpp.
void renderDeviceBuffer(IAudioClient& client, IAudioRenderClient& renderClient, PcmRing& ring,
                        UINT32 bufferFrameCount, std::size_t frameBytes) {
  UINT32 padding = 0;
  if (FAILED(client.GetCurrentPadding(&padding))) {
    return;
  }
  const UINT32 frames = bufferFrameCount - padding;
  if (frames == 0U) {
    return;
  }
  BYTE* deviceBuffer = nullptr;
  if (FAILED(renderClient.GetBuffer(frames, &deviceBuffer))) {
    return;
  }
  const std::size_t needed = static_cast<std::size_t>(frames) * frameBytes;
  auto* bytes = reinterpret_cast<std::byte*>(deviceBuffer);
  const std::size_t got = ring.read(std::span<std::byte>(bytes, needed));
  if (got == 0U) {
    // Nothing to play: let WASAPI fill silence instead of memset-ing here.
    renderClient.ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
    return;
  }
  if (got < needed) {
    std::memset(deviceBuffer + got, 0, needed - got); // partial underrun -> pad with silence
  }
  renderClient.ReleaseBuffer(frames, 0);
}
} // namespace detail

class WasapiAudioSink::Impl {
public:
  explicit Impl(AudioFormat sourceFormat) : sourceFormat_(sourceFormat) {}

  ~Impl() {
    stop();
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  void start() {
    if (running_.load(std::memory_order_acquire)) {
      return;
    }
    stopRequested_.store(false, std::memory_order_relaxed);
    flushRequested_.store(false, std::memory_order_relaxed);
    try {
      // Initialize COM on this (the caller's) thread, where the device
      // interfaces are created and later released — not at construction.
      apartment_.emplace();
      acquireDevice();
    } catch (...) {
      stop(); // release whatever was created, so a retry starts clean
      throw;
    }

    running_.store(true, std::memory_order_release);
    try {
      renderThread_ = std::jthread([this] { renderLoopGuarded(); });
    } catch (...) {
      // Translate a std::jthread failure (e.g. std::system_error) so start()
      // honours its documented DeviceError contract; release the device.
      stop();
      throw DeviceError("WasapiAudioSink: failed to create the render thread");
    }

    if (const HRESULT hr = audioClient_->Start(); FAILED(hr)) {
      stop();
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: IAudioClient::Start failed");
    }
  }

  void write(std::span<const std::byte> pcm) {
    // Shared with flush(); excludes stop()'s teardown of converter_/ring_.
    const std::shared_lock lock(stateMutex_);
    if (!running_.load(std::memory_order_acquire)) {
      return;
    }
    const std::uint64_t generation = flushGeneration_.load(std::memory_order_acquire);

    scratch_.clear();
    converter_->convert(pcm, scratch_);

    std::span<const std::byte> remaining{scratch_};
    while (!remaining.empty()) {
      if (stopRequested_.load(std::memory_order_acquire) ||
          flushGeneration_.load(std::memory_order_acquire) != generation) {
        return; // stopped or barged-in: abandon this (now stale) audio
      }
      if (flushRequested_.load(std::memory_order_acquire)) {
        // A flush is pending; wait for the render thread to clear the ring so
        // this fresh audio is not discarded along with the stale data.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      // `remaining` (whole frames from the converter) and the ring's free space
      // (frame-aligned capacity, frame-aligned in-flight) are both frame
      // multiples, so `written` is too — a frame is never split mid-write.
      const std::size_t written = ring_->write(remaining);
      remaining = remaining.subspan(written);
      if (written == 0U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // full; let it drain
      }
    }
  }

  void flush() {
    // Shared with write(); excludes stop() so audioEvent_ cannot be closed here.
    const std::shared_lock lock(stateMutex_);
    // Bump the generation first so a blocked producer abandons stale audio, then
    // ask the render thread to drop what is buffered and reset the device.
    flushGeneration_.fetch_add(1, std::memory_order_acq_rel);
    flushRequested_.store(true, std::memory_order_release);
    if (audioEvent_ != nullptr) {
      ::SetEvent(audioEvent_); // wake the render thread now, even if the stream is silent
    }
  }

  // Idempotent: also releases a partially-acquired device (e.g. after a failed
  // start()), so it must not early-return on `running_ == false`.
  void stop() {
    running_.store(false, std::memory_order_release);
    stopRequested_.store(true, std::memory_order_release);
    if (audioEvent_ != nullptr) {
      ::SetEvent(audioEvent_); // wake the render thread out of its wait
    }
    if (renderThread_.joinable()) {
      renderThread_.join();
    }
    // The render thread is gone; now exclude any in-flight write()/flush()
    // before tearing down the converter, ring, and event handle.
    const std::unique_lock lock(stateMutex_);
    if (audioClient_) {
      audioClient_->Stop();
    }
    renderClient_.Reset();
    audioClient_.Reset();
    device_.Reset();
    enumerator_.Reset();
    converter_.reset();
    ring_.reset();
    if (audioEvent_ != nullptr) {
      ::CloseHandle(audioEvent_);
      audioEvent_ = nullptr;
    }
    apartment_.reset(); // CoUninitialize last, after every COM object is released
  }

private:
  void acquireDevice() {
    activateAudioClient();
    initializeStream();
  }

  /// device enumerator -> default render endpoint -> activated IAudioClient.
  void activateAudioClient() {
    // Sequence the call before its null-check: argument-evaluation order is
    // unspecified, so the out-param must be inspected after the call returns.
    const HRESULT enumHr = createDeviceEnumerator(enumerator_.ReleaseAndGetAddressOf());
    throwOnFailure(enumHr, "WasapiAudioSink: cannot create device enumerator",
                   enumerator_ != nullptr);

    const HRESULT endpointHr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    throwOnFailure(endpointHr, "WasapiAudioSink: no default render device", device_ != nullptr);

    const HRESULT activateHr =
        device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(audioClient_.GetAddressOf()));
    throwOnFailure(activateHr, "WasapiAudioSink: cannot activate the audio client",
                   audioClient_ != nullptr);
  }

  /// Builds the converter for the device mix format, then starts the event-driven
  /// stream and the ring.
  void initializeStream() {
    const MixFormat mixFormat = readMixFormat();
    configureConverter(*mixFormat);
    startEventDrivenClient(*mixFormat);
    ring_ = std::make_unique<PcmRing>(ringCapacityBytes());
  }

  /// The device mix format, owned so it is freed on every (incl. throwing) path.
  MixFormat readMixFormat() const {
    WAVEFORMATEX* raw = nullptr;
    const HRESULT hr = audioClient_->GetMixFormat(&raw); // sequence before the null-check
    throwOnFailure(hr, "WasapiAudioSink: cannot read the device mix format", raw != nullptr);
    return MixFormat(raw, &::CoTaskMemFree);
  }

  /// Detects the sample encoding, guards the format's preconditions, and builds
  /// the PCM converter (so start() only ever throws DeviceError).
  void configureConverter(const WAVEFORMATEX& mixFormat) {
    const SampleFormat sampleFormat = detectSampleFormat(mixFormat);
    if (const bool formatIsUsable = mixFormat.nSamplesPerSec != 0U && mixFormat.nChannels != 0U &&
                                    mixFormat.nBlockAlign != 0U;
        !formatIsUsable) {
      throw DeviceError("WasapiAudioSink: invalid device mix format");
    }
    frameBytes_ = mixFormat.nBlockAlign;
    converter_.emplace(sourceFormat_, mixFormat.nSamplesPerSec, mixFormat.nChannels, sampleFormat);
  }

  /// Initializes the shared-mode, event-driven stream and fetches the render
  /// client used by the render thread.
  void startEventDrivenClient(const WAVEFORMATEX& mixFormat) {
    throwOnFailure(audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                            AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, &mixFormat,
                                            nullptr),
                   "WasapiAudioSink: IAudioClient::Initialize failed");
    audioEvent_ = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (audioEvent_ == nullptr) {
      throw DeviceError(::GetLastError(), "WasapiAudioSink: cannot create the render event");
    }
    throwOnFailure(audioClient_->SetEventHandle(audioEvent_),
                   "WasapiAudioSink: cannot set the render event handle");
    throwOnFailure(audioClient_->GetBufferSize(&bufferFrameCount_),
                   "WasapiAudioSink: cannot get the buffer size");
    const HRESULT serviceHr = audioClient_->GetService(IID_PPV_ARGS(&renderClient_));
    throwOnFailure(serviceHr, "WasapiAudioSink: cannot get the render client",
                   renderClient_ != nullptr);
  }

  /// Ring capacity in bytes. Sizing is a smoothness knob, not a correctness one:
  /// overflow back-pressures write() (no loss) and underflow zero-fills (no
  /// crash), and synthesis runs faster than real-time so the ring stays full.
  /// We use ~RingSeconds of audio but floor it at a few device periods, so the
  /// render thread can never outrun a momentarily-behind producer regardless of
  /// the device's period. The result is a whole number of frames: with a
  /// frame-aligned capacity, and the converter emitting whole frames and the
  /// render thread consuming whole frames, the in-flight count and free space
  /// stay frame-aligned, so a ring write can never split a frame.
  [[nodiscard]] std::size_t ringCapacityBytes() const {
    const std::size_t bytesPerSecond =
        static_cast<std::size_t>(converter_->targetSampleRate()) * frameBytes_;
    const auto bySeconds =
        static_cast<std::size_t>(static_cast<double>(bytesPerSecond) * RingSeconds);
    const std::size_t byPeriods = std::size_t{MinBufferPeriods} * bufferFrameCount_ * frameBytes_;
    const std::size_t capacity = std::max(bySeconds, byPeriods);
    return (capacity / frameBytes_) * frameBytes_; // round down to whole frames
  }

  void renderLoopGuarded() noexcept {
    // An exception must never escape the render thread (it would terminate the
    // process), so the whole loop runs inside a catch-all.
    try {
      renderLoop();
    } catch (...) {
      // The render thread is dying (e.g. COM init failure). Signal it so the
      // producer stops enqueueing — write() checks stopRequested_ — and any
      // blocked writer is released, rather than hanging once the ring fills.
      stopRequested_.store(true, std::memory_order_release);
      flushRequested_.store(false, std::memory_order_release);
    }
  }

  void renderLoop() {
    // The render thread issues WASAPI COM calls, and COM must be initialized
    // per-thread. The interfaces were created on the MTA, so this thread joins
    // the MTA too (no marshaling needed).
    const ComApartment renderThreadCom;

    // Real-time scheduling for the render thread (avoids priority inversion,
    // ADR-10). Best-effort: if MMCSS is unavailable we still render.
    DWORD taskIndex = 0;
    HANDLE mmcss = ::AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (!stopRequested_.load(std::memory_order_acquire)) {
      const DWORD waited = ::WaitForSingleObject(audioEvent_, RenderWaitMs);
      if (stopRequested_.load(std::memory_order_acquire)) {
        break;
      }
      if (waited != WAIT_OBJECT_0) {
        continue; // timed out — loop back and re-check the stop flag
      }
      if (flushRequested_.load(std::memory_order_acquire) && !serviceFlush()) {
        // The stream is wedged; set stop so the loop exits on the next check,
        // skipping renderAvailable() on it. The flush flag is already cleared, so
        // a blocked producer (which also checks stopRequested_) is released.
        stopRequested_.store(true, std::memory_order_release);
        continue;
      }
      renderAvailable();
    }

    if (mmcss != nullptr) {
      ::AvRevertMmThreadCharacteristics(mmcss);
    }
  }

  /// Services a pending flush: stop the stream, drop buffered audio, restart.
  /// Returns false if the stream is wedged (a Stop/Reset/Start failed) so the
  /// render loop can give up. Clears the flush flag only after the ring is
  /// emptied, so a producer waiting on it cannot push audio this clear would drop.
  bool serviceFlush() {
    const HRESULT stopped = audioClient_->Stop();
    const HRESULT reset = audioClient_->Reset();
    ring_->clear();
    const HRESULT restarted = audioClient_->Start();
    flushRequested_.store(false, std::memory_order_release);
    return SUCCEEDED(stopped) && SUCCEEDED(reset) && SUCCEEDED(restarted);
  }

  void renderAvailable() {
    detail::renderDeviceBuffer(*audioClient_.Get(), *renderClient_.Get(), *ring_, bufferFrameCount_,
                               frameBytes_);
  }

  AudioFormat sourceFormat_;
  std::optional<ComApartment> apartment_; // COM for the start()/stop() thread

  ComPtr<IMMDeviceEnumerator> enumerator_;
  ComPtr<IMMDevice> device_;
  ComPtr<IAudioClient> audioClient_;
  ComPtr<IAudioRenderClient> renderClient_;
  HANDLE audioEvent_{nullptr};
  UINT32 bufferFrameCount_{0};
  std::size_t frameBytes_{0};

  std::optional<PcmConverter> converter_;
  std::unique_ptr<PcmRing> ring_;
  std::vector<std::byte> scratch_; // reused by write() on the producer thread

  std::jthread renderThread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopRequested_{false};
  std::atomic<bool> flushRequested_{false};
  std::atomic<std::uint64_t> flushGeneration_{0};
  // Shared by write()/flush(), exclusive in stop(): lets stop() tear down the
  // converter/ring/event only once no producer or barge-in call is in flight.
  // The render thread is joined before the exclusive section, so it never locks.
  std::shared_mutex stateMutex_;
};

WasapiAudioSink::WasapiAudioSink(AudioFormat sourceFormat)
    : impl_(std::make_unique<Impl>(sourceFormat)) {}

WasapiAudioSink::~WasapiAudioSink() = default;

void WasapiAudioSink::start() {
  impl_->start();
}

void WasapiAudioSink::write(std::span<const std::byte> pcm) {
  impl_->write(pcm);
}

void WasapiAudioSink::flush() {
  impl_->flush();
}

void WasapiAudioSink::stop() {
  impl_->stop();
}

} // namespace vox::audio
