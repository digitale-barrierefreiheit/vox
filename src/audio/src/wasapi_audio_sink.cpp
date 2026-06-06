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
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/errors.hpp>
#include <vox/audio/pcm_converter.hpp>
#include <vox/audio/pcm_ring.hpp>
#include <vox/audio/wasapi_audio_sink.hpp>

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
    if (const HRESULT hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                              IID_PPV_ARGS(&enumerator_));
        FAILED(hr)) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: cannot create device enumerator");
    }
    if (const HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
        FAILED(hr) || !device_) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: no default render device");
    }
    if (const HRESULT hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                             reinterpret_cast<void**>(audioClient_.GetAddressOf()));
        FAILED(hr)) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: cannot activate the audio client");
    }

    WAVEFORMATEX* rawMixFormat = nullptr;
    if (const HRESULT hr = audioClient_->GetMixFormat(&rawMixFormat);
        FAILED(hr) || rawMixFormat == nullptr) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: cannot read the device mix format");
    }
    // Own the CoTaskMem allocation so it is freed on every path, including the
    // throwing ones below (detectSampleFormat / converter construction).
    const std::unique_ptr<WAVEFORMATEX, decltype(&::CoTaskMemFree)> mixFormat(rawMixFormat,
                                                                              &::CoTaskMemFree);

    const SampleFormat sampleFormat = detectSampleFormat(*mixFormat);
    if (mixFormat->nSamplesPerSec == 0U || mixFormat->nChannels == 0U ||
        mixFormat->nBlockAlign == 0U) {
      // Guard the converter's and ring's preconditions so start() only ever
      // throws DeviceError, never std::invalid_argument from them.
      throw DeviceError("WasapiAudioSink: invalid device mix format");
    }
    frameBytes_ = mixFormat->nBlockAlign;
    converter_.emplace(sourceFormat_, mixFormat->nSamplesPerSec, mixFormat->nChannels,
                       sampleFormat);

    if (const HRESULT hr =
            audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0,
                                     0, mixFormat.get(), nullptr);
        FAILED(hr)) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: IAudioClient::Initialize failed");
    }

    audioEvent_ = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (audioEvent_ == nullptr) {
      throw DeviceError(::GetLastError(), "WasapiAudioSink: cannot create the render event");
    }
    if (const HRESULT hr = audioClient_->SetEventHandle(audioEvent_); FAILED(hr)) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: cannot set the render event handle");
    }
    if (const HRESULT hr = audioClient_->GetBufferSize(&bufferFrameCount_); FAILED(hr)) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: cannot get the buffer size");
    }
    if (const HRESULT hr = audioClient_->GetService(IID_PPV_ARGS(&renderClient_)); FAILED(hr)) {
      throw DeviceError(static_cast<std::uint32_t>(hr),
                        "WasapiAudioSink: cannot get the render client");
    }
    ring_ = std::make_unique<PcmRing>(ringCapacityBytes());
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
      if (flushRequested_.load(std::memory_order_acquire)) {
        const HRESULT stopped = audioClient_->Stop();
        const HRESULT reset = audioClient_->Reset();
        ring_->clear();
        const HRESULT restarted = audioClient_->Start();
        // Clear the flag only after the ring is emptied, so a producer waiting
        // on it cannot push fresh audio that this clear would then drop.
        flushRequested_.store(false, std::memory_order_release);
        if (FAILED(stopped) || FAILED(reset) || FAILED(restarted)) {
          // The stream is wedged; set stop so the loop condition exits on the
          // next check, skipping renderAvailable() on the wedged stream. The flag
          // is already cleared, so a blocked producer (which also checks
          // stopRequested_) is released rather than left hanging.
          stopRequested_.store(true, std::memory_order_release);
          continue;
        }
      }
      renderAvailable();
    }

    if (mmcss != nullptr) {
      ::AvRevertMmThreadCharacteristics(mmcss);
    }
  }

  void renderAvailable() {
    UINT32 padding = 0;
    if (FAILED(audioClient_->GetCurrentPadding(&padding))) {
      return;
    }
    const UINT32 frames = bufferFrameCount_ - padding;
    if (frames == 0U) {
      return;
    }
    BYTE* deviceBuffer = nullptr;
    if (FAILED(renderClient_->GetBuffer(frames, &deviceBuffer))) {
      return;
    }
    const std::size_t needed = static_cast<std::size_t>(frames) * frameBytes_;
    auto* bytes = reinterpret_cast<std::byte*>(deviceBuffer);
    const std::size_t got = ring_->read(std::span<std::byte>(bytes, needed));
    if (got == 0U) {
      // Nothing to play: let WASAPI fill silence instead of memset-ing here.
      renderClient_->ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
      return;
    }
    if (got < needed) {
      std::memset(deviceBuffer + got, 0, needed - got); // partial underrun -> pad with silence
    }
    renderClient_->ReleaseBuffer(frames, 0);
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
