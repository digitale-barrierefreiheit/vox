// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for WasapiAudioSink's device-acquisition error handling,
///        driven through the #68 test seam with mock COM — no real audio device
///        required, so these run anywhere (incl. the coverage job) and
///        fault-inject every failure of the COM device-acquisition chain.
#if defined(_WIN32)

#  include <array>
#  include <atomic>
#  include <chrono>
#  include <cstddef>
#  include <cstdint>
#  include <future>
#  include <memory>
#  include <span>
#  include <thread>
#  include <vector>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/audio/audio_format.hpp>
#  include <vox/audio/errors.hpp>
#  include <vox/audio/pcm_ring.hpp>
#  include <vox/audio/wasapi_audio_sink.hpp>
#  include <vox/audio/wasapi_test_seam.hpp>

#  include "wasapi_com_mocks.hpp"

namespace {

using vox::audio::AudioFormat;
using vox::audio::DeviceError;
using vox::audio::PcmRing;
using vox::audio::WasapiAudioSink;
using vox::audio::detail::renderDeviceBuffer;
using vox::audio::testing::makeMixFormat;
using vox::audio::testing::MockAudioClient;
using vox::audio::testing::MockAudioRenderClient;
using vox::audio::testing::MockMMDevice;
using vox::audio::testing::MockMMDeviceEnumerator;

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

constexpr long ErrorFail = static_cast<long>(0x80004005U); // E_FAIL
constexpr auto WaitTimeout = std::chrono::seconds(2);

/// Restores the real enumerator factory on scope exit, so a test never leaks the
/// seam into the next one.
class SeamGuard {
public:
  SeamGuard() = default;

  ~SeamGuard() {
    vox::audio::testing::setEnumeratorFactory({});
  }

  SeamGuard(const SeamGuard&) = delete;
  SeamGuard& operator=(const SeamGuard&) = delete;
  SeamGuard(SeamGuard&&) = delete;
  SeamGuard& operator=(SeamGuard&&) = delete;
};

/// A failing factory (no chain at all) covers the very first link: the sink
/// cannot even create the device enumerator.
TEST(WasapiAudioSinkErrors, ThrowsDeviceErrorWhenEnumeratorCreationFails) {
  [[maybe_unused]] const SeamGuard guard;
  vox::audio::testing::setEnumeratorFactory([](struct IMMDeviceEnumerator** out) {
    *out = nullptr;
    return ErrorFail;
  });

  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

/// Fixture wiring the full mock COM chain: the seam hands back `enumerator_`,
/// whose default actions walk enumerator -> device -> client -> render client so
/// every acquisition link succeeds. Each test then breaks exactly one link and
/// asserts the sink translates it to `DeviceError`.
class WasapiAcquisitionTest : public ::testing::Test {
protected:
  void SetUp() override {
    vox::audio::testing::setEnumeratorFactory([this](struct IMMDeviceEnumerator** out) {
      *out = static_cast<IMMDeviceEnumerator*>(&enumerator_);
      return 0L; // S_OK
    });
    installHappyChain();
  }

  void TearDown() override {
    vox::audio::testing::setEnumeratorFactory({});
    vox::audio::testing::setRenderWaitFn({});
  }

  /// Default behaviours for the whole chain — overridden per-test to inject one
  /// failure. `NiceMock` keeps render-thread calls (the success path) quiet.
  void installHappyChain() {
    ON_CALL(enumerator_, GetDefaultAudioEndpoint(_, _, _))
        .WillByDefault(DoAll(SetArgPointee<2>(static_cast<IMMDevice*>(&device_)), Return(S_OK)));
    ON_CALL(device_, Activate(_, _, _, _))
        .WillByDefault(
            DoAll(SetArgPointee<3>(static_cast<void*>(static_cast<IAudioClient*>(&client_))),
                  Return(S_OK)));
    ON_CALL(client_, GetMixFormat(_)).WillByDefault([](WAVEFORMATEX** out) {
      *out = makeMixFormat();
      return S_OK;
    });
    ON_CALL(client_, Initialize(_, _, _, _, _, _)).WillByDefault(Return(S_OK));
    ON_CALL(client_, SetEventHandle(_)).WillByDefault(Return(S_OK));
    ON_CALL(client_, GetBufferSize(_)).WillByDefault(DoAll(SetArgPointee<0>(480U), Return(S_OK)));
    ON_CALL(client_, GetService(_, _))
        .WillByDefault(DoAll(
            SetArgPointee<1>(static_cast<void*>(static_cast<IAudioRenderClient*>(&renderClient_))),
            Return(S_OK)));
    ON_CALL(client_, Start()).WillByDefault(Return(S_OK));
    ON_CALL(client_, Stop()).WillByDefault(Return(S_OK));
    ON_CALL(client_, Reset()).WillByDefault(Return(S_OK));
    ON_CALL(client_, GetCurrentPadding(_))
        .WillByDefault(DoAll(SetArgPointee<0>(480U), Return(S_OK)));
  }

  /// Makes the client's Reset() return @p result and fulfil a future the first
  /// time it is called, so a test can block deterministically on the render
  /// thread servicing a flush — no polling, no sleeping. The shared state is
  /// owned by the ON_CALL action, so it outlives this call.
  std::future<void> resetSignal(HRESULT result) {
    auto promise = std::make_shared<std::promise<void>>();
    auto signaled = std::make_shared<std::atomic<bool>>(false);
    std::future<void> future = promise->get_future();
    ON_CALL(client_, Reset()).WillByDefault([promise, signaled, result] {
      if (!signaled->exchange(true)) {
        promise->set_value();
      }
      return result;
    });
    return future;
  }

  NiceMock<MockMMDeviceEnumerator> enumerator_;
  NiceMock<MockMMDevice> device_;
  NiceMock<MockAudioClient> client_;
  NiceMock<MockAudioRenderClient> renderClient_;
};

TEST_F(WasapiAcquisitionTest, ThrowsWhenGetDefaultEndpointFails) {
  EXPECT_CALL(enumerator_, GetDefaultAudioEndpoint(_, _, _)).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenDefaultEndpointIsNull) {
  EXPECT_CALL(enumerator_, GetDefaultAudioEndpoint(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(nullptr), Return(S_OK)));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenActivateFails) {
  EXPECT_CALL(device_, Activate(_, _, _, _)).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenGetMixFormatFails) {
  EXPECT_CALL(client_, GetMixFormat(_)).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenMixFormatIsNull) {
  EXPECT_CALL(client_, GetMixFormat(_))
      .WillOnce(DoAll(SetArgPointee<0>(static_cast<WAVEFORMATEX*>(nullptr)), Return(S_OK)));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenMixFormatIsUnsupported) {
  // 24-bit PCM is neither Int16 nor Float32 — detectSampleFormat rejects it.
  EXPECT_CALL(client_, GetMixFormat(_)).WillOnce([](WAVEFORMATEX** out) {
    *out = makeMixFormat(WAVE_FORMAT_PCM, 24, 48000, 2);
    return S_OK;
  });
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenExtensibleMixFormatIsMalformed) {
  // WAVE_FORMAT_EXTENSIBLE with cbSize too small for the extensible fields.
  EXPECT_CALL(client_, GetMixFormat(_)).WillOnce([](WAVEFORMATEX** out) {
    *out = makeMixFormat(WAVE_FORMAT_EXTENSIBLE, 16, 48000, 2);
    return S_OK;
  });
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenMixFormatHasZeroChannels) {
  EXPECT_CALL(client_, GetMixFormat(_)).WillOnce([](WAVEFORMATEX** out) {
    *out = makeMixFormat(WAVE_FORMAT_PCM, 16, 48000, 0);
    return S_OK;
  });
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenInitializeFails) {
  EXPECT_CALL(client_, Initialize(_, _, _, _, _, _)).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenSetEventHandleFails) {
  EXPECT_CALL(client_, SetEventHandle(_)).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenGetBufferSizeFails) {
  EXPECT_CALL(client_, GetBufferSize(_)).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenGetServiceFails) {
  EXPECT_CALL(client_, GetService(_, _)).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenStartFails) {
  // Start() runs after the render thread is launched; the sink must tear it back
  // down (join) and surface a DeviceError.
  EXPECT_CALL(client_, Start()).WillOnce(Return(ErrorFail));
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_THROW(sink.start(), DeviceError);
}

TEST_F(WasapiAcquisitionTest, StartsAndStopsCleanlyOnTheHappyPath) {
  // The whole chain succeeds: start() returns, the render thread spins, and
  // stop() joins it without throwing — covering acquireDevice's success branch.
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_NO_THROW(sink.start());
  EXPECT_NO_THROW(sink.stop());
}

TEST_F(WasapiAcquisitionTest, RenderThreadSurvivesAWaitTimeout) {
  // Drive the render thread's event wait through the seam: report WAIT_TIMEOUT
  // once (the idle "timed out, loop back" branch), then WAIT_OBJECT_0 so the
  // thread renders normally instead of spinning. Block on a promise the seam
  // fulfils, so this is deterministic — no real 200 ms wait and no sleep.
  auto timedOut = std::make_shared<std::promise<void>>();
  std::future<void> timeoutHappened = timedOut->get_future();
  auto fired = std::make_shared<std::atomic<bool>>(false);
  vox::audio::testing::setRenderWaitFn([timedOut, fired]() -> unsigned long {
    if (!fired->exchange(true)) {
      timedOut->set_value();
      return WAIT_TIMEOUT; // the timeout branch under test
    }
    return WAIT_OBJECT_0;
  });

  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  ASSERT_NO_THROW(sink.start());
  ASSERT_EQ(timeoutHappened.wait_for(WaitTimeout), std::future_status::ready);
  EXPECT_NO_THROW(sink.stop());
}

TEST_F(WasapiAcquisitionTest, DrivesWriteFlushAndRenderAcrossALiveCycle) {
  // The render thread Resets the client when it services a flush; block on that
  // deterministically (a future, not a sleep).
  std::future<void> reset = resetSignal(S_OK);

  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  ASSERT_NO_THROW(sink.start());
  EXPECT_NO_THROW(sink.start()); // a second start() is a no-op while running

  // write() resamples mono 16-bit source PCM into the ring (whole frames).
  const std::vector<std::byte> pcm(441 * sizeof(std::int16_t), std::byte{0});
  EXPECT_NO_THROW(sink.write(pcm));

  // flush() wakes the render thread, which Stops/Resets/clears/Starts the client.
  sink.flush();
  ASSERT_EQ(reset.wait_for(WaitTimeout), std::future_status::ready);

  EXPECT_NO_THROW(sink.stop());
}

/// Allocates a well-formed WAVE_FORMAT_EXTENSIBLE IEEE-float mix format (the
/// SubFormat's Data1 carries the underlying tag), so detectSampleFormat takes its
/// extensible/Float32 branch. CoTaskMem-allocated: the sink frees it.
WAVEFORMATEX* makeExtensibleFloatFormat() {
  auto* format = static_cast<WAVEFORMATEXTENSIBLE*>(::CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
  if (format == nullptr) {
    return nullptr;
  }
  *format = WAVEFORMATEXTENSIBLE{};
  format->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->Format.nChannels = 2;
  format->Format.nSamplesPerSec = 48000;
  format->Format.wBitsPerSample = 32;
  format->Format.nBlockAlign = static_cast<WORD>(2 * (32 / 8));
  format->Format.nAvgBytesPerSec = 48000 * format->Format.nBlockAlign;
  format->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  format->Samples.wValidBitsPerSample = 32;
  format->SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT; // 0x0003
  // WAVEFORMATEXTENSIBLE leads with its WAVEFORMATEX, so &Format is the same
  // CoTaskMem allocation the sink later frees — no reinterpret_cast needed.
  return &format->Format;
}

TEST_F(WasapiAcquisitionTest, AcceptsAnExtensibleFloatMixFormat) {
  ON_CALL(client_, GetMixFormat(_)).WillByDefault([](WAVEFORMATEX** out) {
    *out = makeExtensibleFloatFormat();
    return S_OK;
  });
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  EXPECT_NO_THROW(sink.start());
  EXPECT_NO_THROW(sink.stop());
}

TEST_F(WasapiAcquisitionTest, WriteDrainAndFlushBeforeStartAreNoOps) {
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  const std::vector<std::byte> pcm(8, std::byte{0});
  EXPECT_NO_THROW(sink.write(pcm)); // not running: write() returns immediately
  EXPECT_NO_THROW(sink.drain());    // not running: drain() returns immediately
  EXPECT_NO_THROW(sink.flush());    // no audio event yet: flush() is a no-op
}

// At end of stream the producer calls drain() so the resampler's group-delay tail
// reaches the device — covering the converter's drain path on the live sink.
TEST_F(WasapiAcquisitionTest, DrainFlushesTheResamplerTailAfterAWrite) {
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  ASSERT_NO_THROW(sink.start());
  const std::vector<std::byte> pcm(441 * sizeof(std::int16_t), std::byte{0});
  EXPECT_NO_THROW(sink.write(pcm));
  EXPECT_NO_THROW(sink.drain()); // buffered tail still belongs to this stream: emit it
  EXPECT_NO_THROW(sink.stop());
}

// A barge-in bumps the flush generation, so a drain that lands afterwards finds a
// stale tail: it drops it (the flush already cleared the ring) instead of pushing.
TEST_F(WasapiAcquisitionTest, DrainAfterAFlushDropsTheStaleTail) {
  std::future<void> reset = resetSignal(S_OK);
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  ASSERT_NO_THROW(sink.start());
  const std::vector<std::byte> pcm(441 * sizeof(std::int16_t), std::byte{0});
  sink.write(pcm);
  sink.flush(); // barge-in -> the converter's buffered tail is now stale
  ASSERT_EQ(reset.wait_for(WaitTimeout), std::future_status::ready);
  EXPECT_NO_THROW(sink.drain());
  EXPECT_NO_THROW(sink.stop());
}

// The first write of a new stream (a new generation after barge-in) resets the
// converter, so the previous utterance's filter history never bleeds into it.
TEST_F(WasapiAcquisitionTest, AWriteAfterAFlushResetsTheConverter) {
  std::future<void> reset = resetSignal(S_OK);
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  ASSERT_NO_THROW(sink.start());
  const std::vector<std::byte> pcm(441 * sizeof(std::int16_t), std::byte{0});
  sink.write(pcm);
  sink.flush();
  ASSERT_EQ(reset.wait_for(WaitTimeout), std::future_status::ready);
  EXPECT_NO_THROW(sink.write(pcm)); // new generation: converter is reset, then pushes
  EXPECT_NO_THROW(sink.stop());
}

// A barge-in that lands while a write is back-pressuring must leave nothing behind:
// pushAll re-arms the flush so a frame that slipped into the ring after the render
// cleared it is dropped too. Drives the post-push re-arm under real threads — the
// mock render only services audio on a flush, so this oversized write fills the ring
// and blocks until a flush bumps the generation and the producer abandons.
TEST_F(WasapiAcquisitionTest, AWriteRacingABargeInReArmsTheFlush) {
  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  ASSERT_NO_THROW(sink.start());
  const std::vector<std::byte> big(256U * 1024U, std::byte{0}); // far larger than the ring
  std::atomic done{false};
  std::jthread producer([&sink, &big, &done] {
    sink.write(big);
    done.store(true, std::memory_order_release);
  });
  // Flush until the back-pressured producer observes the generation bump and
  // abandons; on the way out pushAll re-arms the flush (the branch under test). A
  // deadline keeps a regression in the abandon path from hanging the suite: on
  // timeout, stop() releases the producer (stopRequested_) before we join.
  const auto deadline = std::chrono::steady_clock::now() + WaitTimeout;
  while (!done.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= deadline) {
      sink.stop();
      producer.join();
      FAIL() << "write() did not abandon after a barge-in within the deadline";
    }
    sink.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  producer.join();
  EXPECT_NO_THROW(sink.stop());
}

TEST_F(WasapiAcquisitionTest, RenderThreadStopsWhenAFlushResetFails) {
  // Reset fails: the render thread must give up. Block on it deterministically.
  std::future<void> reset = resetSignal(ErrorFail);

  WasapiAudioSink sink(AudioFormat{22050, 16, 1});
  ASSERT_NO_THROW(sink.start());
  sink.flush();
  ASSERT_EQ(reset.wait_for(WaitTimeout), std::future_status::ready);
  EXPECT_NO_THROW(sink.stop());
}

TEST_F(WasapiAcquisitionTest, ThrowsWhenStartedOnAnStaThread) {
  // WASAPI requires an MTA (or COM-uninitialized) thread; on an STA the sink's
  // ComApartment hits RPC_E_CHANGED_MODE and start() must surface a DeviceError.
  // CoInitializeEx may return S_OK or S_FALSE; both are a success to balance.
  ASSERT_TRUE(SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
  {
    WasapiAudioSink sink(AudioFormat{22050, 16, 1});
    EXPECT_THROW(sink.start(), DeviceError);
  }
  ::CoUninitialize();
}

// Direct tests for the render thread's per-tick step. Driving it through the
// real thread is non-deterministic (an auto-reset event the OS would normally
// pulse), so the step is factored out (issue #68) and exercised here with mock
// COM and a real ring — covering its silent / partial-underrun / full-copy
// branches without a real device.
constexpr UINT32 BufferFrames = 8;
constexpr std::size_t FrameBytes = 4; // 16-bit stereo
constexpr std::size_t DeviceBytes = BufferFrames * FrameBytes;

TEST(WasapiRenderStep, ReturnsEarlyWhenGetPaddingFails) {
  NiceMock<MockAudioClient> client;
  NiceMock<MockAudioRenderClient> renderClient;
  PcmRing ring(64);
  EXPECT_CALL(client, GetCurrentPadding(_)).WillOnce(Return(ErrorFail));
  EXPECT_CALL(renderClient, GetBuffer(_, _)).Times(0);
  EXPECT_CALL(renderClient, ReleaseBuffer(_, _)).Times(0);
  renderDeviceBuffer(client, renderClient, ring, BufferFrames, FrameBytes);
}

TEST(WasapiRenderStep, ReturnsEarlyWhenDeviceBufferIsFull) {
  NiceMock<MockAudioClient> client;
  NiceMock<MockAudioRenderClient> renderClient;
  PcmRing ring(64);
  // padding == bufferFrameCount -> zero free frames, so nothing to render.
  EXPECT_CALL(client, GetCurrentPadding(_))
      .WillOnce(DoAll(SetArgPointee<0>(BufferFrames), Return(S_OK)));
  EXPECT_CALL(renderClient, GetBuffer(_, _)).Times(0);
  EXPECT_CALL(renderClient, ReleaseBuffer(_, _)).Times(0);
  renderDeviceBuffer(client, renderClient, ring, BufferFrames, FrameBytes);
}

TEST(WasapiRenderStep, ReturnsEarlyWhenGetBufferFails) {
  NiceMock<MockAudioClient> client;
  NiceMock<MockAudioRenderClient> renderClient;
  PcmRing ring(64);
  EXPECT_CALL(client, GetCurrentPadding(_)).WillOnce(DoAll(SetArgPointee<0>(0U), Return(S_OK)));
  EXPECT_CALL(renderClient, GetBuffer(BufferFrames, _)).WillOnce(Return(ErrorFail));
  EXPECT_CALL(renderClient, ReleaseBuffer(_, _)).Times(0);
  renderDeviceBuffer(client, renderClient, ring, BufferFrames, FrameBytes);
}

TEST(WasapiRenderStep, ReleasesSilentWhenRingIsEmpty) {
  NiceMock<MockAudioClient> client;
  NiceMock<MockAudioRenderClient> renderClient;
  PcmRing ring(64); // empty: nothing was written
  std::array<BYTE, DeviceBytes> deviceBuffer{};
  EXPECT_CALL(client, GetCurrentPadding(_)).WillOnce(DoAll(SetArgPointee<0>(0U), Return(S_OK)));
  EXPECT_CALL(renderClient, GetBuffer(BufferFrames, _))
      .WillOnce(DoAll(SetArgPointee<1>(deviceBuffer.data()), Return(S_OK)));
  EXPECT_CALL(renderClient, ReleaseBuffer(BufferFrames, AUDCLNT_BUFFERFLAGS_SILENT));
  renderDeviceBuffer(client, renderClient, ring, BufferFrames, FrameBytes);
}

TEST(WasapiRenderStep, ZeroPadsAndReleasesOnPartialUnderrun) {
  NiceMock<MockAudioClient> client;
  NiceMock<MockAudioRenderClient> renderClient;
  PcmRing ring(64);
  // Half a device buffer of audio: the rest must be zero-filled.
  std::array<std::byte, DeviceBytes / 2> source{};
  source.fill(std::byte{0xAB});
  ASSERT_EQ(ring.write(source), source.size());

  std::array<BYTE, DeviceBytes> deviceBuffer{};
  deviceBuffer.fill(0xFF); // so we can see the zero-fill happen
  EXPECT_CALL(client, GetCurrentPadding(_)).WillOnce(DoAll(SetArgPointee<0>(0U), Return(S_OK)));
  EXPECT_CALL(renderClient, GetBuffer(BufferFrames, _))
      .WillOnce(DoAll(SetArgPointee<1>(deviceBuffer.data()), Return(S_OK)));
  EXPECT_CALL(renderClient, ReleaseBuffer(BufferFrames, 0));
  renderDeviceBuffer(client, renderClient, ring, BufferFrames, FrameBytes);

  for (std::size_t i = 0; i < source.size(); ++i) {
    EXPECT_EQ(deviceBuffer[i], 0xAB) << "copied byte " << i;
  }
  for (std::size_t i = source.size(); i < deviceBuffer.size(); ++i) {
    EXPECT_EQ(deviceBuffer[i], 0x00) << "zero-padded byte " << i;
  }
}

TEST(WasapiRenderStep, CopiesFullBufferWhenRingHasEnough) {
  NiceMock<MockAudioClient> client;
  NiceMock<MockAudioRenderClient> renderClient;
  PcmRing ring(64);
  std::array<std::byte, DeviceBytes> source{};
  source.fill(std::byte{0xCD});
  ASSERT_EQ(ring.write(source), source.size());

  std::array<BYTE, DeviceBytes> deviceBuffer{};
  EXPECT_CALL(client, GetCurrentPadding(_)).WillOnce(DoAll(SetArgPointee<0>(0U), Return(S_OK)));
  EXPECT_CALL(renderClient, GetBuffer(BufferFrames, _))
      .WillOnce(DoAll(SetArgPointee<1>(deviceBuffer.data()), Return(S_OK)));
  EXPECT_CALL(renderClient, ReleaseBuffer(BufferFrames, 0));
  renderDeviceBuffer(client, renderClient, ring, BufferFrames, FrameBytes);

  for (const BYTE value : deviceBuffer) {
    EXPECT_EQ(value, 0xCD);
  }
}

} // namespace

#endif // defined(_WIN32)
