// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::audio::PcmConverter (rate / channels / sample format).
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_converter.hpp>

namespace {

using vox::audio::AudioFormat;
using vox::audio::PcmConverter;
using vox::audio::SampleFormat;

std::vector<std::byte> int16Bytes(std::initializer_list<std::int16_t> samples) {
  std::vector<std::byte> bytes(samples.size() * sizeof(std::int16_t));
  std::size_t offset = 0;
  for (const std::int16_t sample : samples) {
    std::memcpy(bytes.data() + offset, &sample, sizeof(sample));
    offset += sizeof(sample);
  }
  return bytes;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — test helper, order is obvious here
std::vector<std::byte> constantInt16(std::int16_t value, std::size_t count) {
  std::vector<std::byte> bytes(count * sizeof(std::int16_t));
  for (std::size_t i = 0; i < count; ++i) {
    std::memcpy(bytes.data() + (i * sizeof(std::int16_t)), &value, sizeof(value));
  }
  return bytes;
}

float floatAt(const std::vector<std::byte>& pcm, std::size_t index) {
  float value = 0.0F;
  std::memcpy(&value, pcm.data() + (index * sizeof(float)), sizeof(float));
  return value;
}

std::int16_t int16At(const std::vector<std::byte>& pcm, std::size_t index) {
  std::int16_t value = 0;
  std::memcpy(&value, pcm.data() + (index * sizeof(std::int16_t)), sizeof(value));
  return value;
}

TEST(PcmConverter, MonoToStereoFloatDuplicatesChannels) {
  PcmConverter converter{AudioFormat{48000, 16, 1}, 48000, 2, SampleFormat::Float32};
  std::vector<std::byte> out;
  converter.convert(int16Bytes({1000, 2000, 3000, 4000}), out);

  ASSERT_EQ(out.size() % (2 * sizeof(float)), 0U); // whole stereo float frames
  const std::size_t floats = out.size() / sizeof(float);
  for (std::size_t frame = 0; frame < floats / 2; ++frame) {
    const float left = floatAt(out, frame * 2);
    const float right = floatAt(out, (frame * 2) + 1);
    EXPECT_FLOAT_EQ(left, right);    // mono duplicated to both channels
    EXPECT_LE(std::abs(left), 1.0F); // normalized range
  }
}

TEST(PcmConverter, SameRateInt16IsExactPassthrough) {
  PcmConverter converter{AudioFormat{48000, 16, 1}, 48000, 1, SampleFormat::Int16};
  std::vector<std::byte> out;
  const std::int16_t dc = 1000;
  converter.convert(constantInt16(dc, 64), out);

  // At equal rates the resampler is bypassed: a sample-for-sample passthrough
  // with no priming delay, so every output frame equals the input.
  ASSERT_EQ(out.size() / sizeof(std::int16_t), 64U);
  for (std::size_t frame = 0; frame < 64; ++frame) {
    EXPECT_EQ(int16At(out, frame), dc);
  }
}

TEST(PcmConverter, UpsamplingRoughlyDoublesFrameCount) {
  PcmConverter converter{AudioFormat{22050, 16, 1}, 44100, 1, SampleFormat::Int16};
  std::vector<std::byte> out;
  constexpr std::size_t InputSamples = 1000;
  converter.convert(constantInt16(500, InputSamples), out);

  const std::size_t frames = out.size() / sizeof(std::int16_t);
  // 2x rate -> ~2x frames. The windowed-sinc holds back its half-width of source
  // samples (group delay), so the count falls a bounded few taps short of 2x;
  // the tail flushes once the next chunk streams in. 64 covers 2*HalfTaps.
  EXPECT_NEAR(static_cast<double>(frames), 2.0 * InputSamples, 64.0);
}

TEST(PcmConverter, ResetClearsStreamingState) {
  PcmConverter converter{AudioFormat{22050, 16, 1}, 44100, 1, SampleFormat::Int16};
  std::vector<std::byte> first;
  converter.convert(constantInt16(2000, 100), first);

  converter.reset();
  std::vector<std::byte> second;
  converter.convert(constantInt16(2000, 100), second);

  EXPECT_EQ(first, second); // identical input + reset -> identical output
}

TEST(PcmConverter, ExposesTargetParameters) {
  const PcmConverter converter{AudioFormat{22050, 16, 1}, 48000, 2, SampleFormat::Float32};
  EXPECT_EQ(converter.targetSampleRate(), 48000U);
  EXPECT_EQ(converter.targetChannels(), 2U);
  EXPECT_EQ(converter.targetFormat(), SampleFormat::Float32);
}

TEST(PcmConverter, RejectsUnsupportedSourceFormat) {
  EXPECT_THROW((PcmConverter{AudioFormat{22050, 8, 1}, 48000, 2, SampleFormat::Float32}),
               std::invalid_argument); // not 16-bit
  EXPECT_THROW((PcmConverter{AudioFormat{22050, 16, 2}, 48000, 2, SampleFormat::Float32}),
               std::invalid_argument); // not mono
  EXPECT_THROW((PcmConverter{AudioFormat{0, 16, 1}, 48000, 1, SampleFormat::Int16}),
               std::invalid_argument); // zero source rate (would make step_ == 0)
}

TEST(PcmConverter, SameRateInt16RoundTripsExtremesExactly) {
  PcmConverter converter{AudioFormat{48000, 16, 1}, 48000, 1, SampleFormat::Int16};
  std::vector<std::byte> out;
  // Equal rates bypass the resampler: a zero-delay passthrough, so input[i]
  // appears at output[i] and the int16 extremes survive exactly.
  converter.convert(int16Bytes({1000, -1000, 32767, -32768, 0}), out);

  ASSERT_EQ(out.size() / sizeof(std::int16_t), 5U);
  EXPECT_EQ(int16At(out, 0), 1000);
  EXPECT_EQ(int16At(out, 1), -1000);
  EXPECT_EQ(int16At(out, 2), 32767);
  EXPECT_EQ(int16At(out, 3), -32768);
  EXPECT_EQ(int16At(out, 4), 0);
}

TEST(PcmConverter, RejectsZeroTargetRateOrChannels) {
  EXPECT_THROW((PcmConverter{AudioFormat{22050, 16, 1}, 0, 2, SampleFormat::Float32}),
               std::invalid_argument);
  EXPECT_THROW((PcmConverter{AudioFormat{22050, 16, 1}, 48000, 0, SampleFormat::Float32}),
               std::invalid_argument);
}

TEST(PcmConverter, RejectsNonSampleAlignedInput) {
  PcmConverter converter{AudioFormat{48000, 16, 1}, 48000, 1, SampleFormat::Int16};
  std::vector<std::byte> out;
  const std::vector<std::byte> oddBytes(3); // not a whole number of 16-bit samples
  EXPECT_THROW(converter.convert(oddBytes, out), std::invalid_argument);
}

TEST(PcmConverter, StreamingAcrossChunksMatchesSingleCall) {
  // Feeding the stream in two chunks must produce exactly the same output as one
  // call: the proof that the resampler's history ring and phase carry correctly.
  std::vector<std::byte> input(2000 * sizeof(std::int16_t));
  for (std::size_t k = 0; k < 2000; ++k) {
    const double v =
        12000.0 * std::sin(2.0 * std::numbers::pi * 1000.0 * static_cast<double>(k) / 22050.0);
    const auto s = static_cast<std::int16_t>(std::lround(v));
    std::memcpy(input.data() + (k * sizeof(std::int16_t)), &s, sizeof(s));
  }

  PcmConverter whole{AudioFormat{22050, 16, 1}, 48000, 1, SampleFormat::Float32};
  std::vector<std::byte> single;
  whole.convert(input, single);

  PcmConverter parts{AudioFormat{22050, 16, 1}, 48000, 1, SampleFormat::Float32};
  std::vector<std::byte> split;
  const std::size_t mid = (input.size() / 2) & ~static_cast<std::size_t>(1); // whole samples
  parts.convert(std::span<const std::byte>(input).first(mid), split);
  parts.convert(std::span<const std::byte>(input).subspan(mid), split);

  EXPECT_EQ(single, split);
}

// --- image suppression (quality, #55) ---------------------------------------

// Generates n samples of a pure int16 sine at `freq` Hz sampled at `rate` Hz,
// peak amplitude `amp` (of full scale), as a 16-bit-mono PCM blob.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — labelled call sites below
std::vector<std::byte> sineInt16(double freq, double rate, double amp, std::size_t n) {
  std::vector<std::byte> bytes(n * sizeof(std::int16_t));
  for (std::size_t k = 0; k < n; ++k) {
    const double v =
        amp * 32767.0 * std::sin(2.0 * std::numbers::pi * freq * static_cast<double>(k) / rate);
    const auto s = static_cast<std::int16_t>(std::lround(v));
    std::memcpy(bytes.data() + (k * sizeof(std::int16_t)), &s, sizeof(s));
  }
  return bytes;
}

std::vector<float> decodeFloat(const std::vector<std::byte>& pcm) {
  std::vector<float> out(pcm.size() / sizeof(float));
  for (std::size_t i = 0; i < out.size(); ++i) {
    std::memcpy(&out[i], pcm.data() + (i * sizeof(float)), sizeof(float));
  }
  return out;
}

// The pre-#55 linear-interpolation resampler, kept here as the baseline the
// windowed-sinc must beat — so the quality gate is a real comparison, not just
// an absolute threshold.
std::vector<float> linearResample(const std::vector<std::byte>& srcPcm, double stepInPerOut) {
  std::vector<float> out;
  double pos = 0.0;
  float previous = 0.0F;
  const std::size_t n = srcPcm.size() / sizeof(std::int16_t);
  for (std::size_t i = 0; i < n; ++i) {
    std::int16_t raw = 0;
    std::memcpy(&raw, srcPcm.data() + (i * sizeof(std::int16_t)), sizeof(raw));
    const float current = static_cast<float>(raw) / 32768.0F;
    while (pos < 1.0) {
      out.push_back(previous + ((current - previous) * static_cast<float>(pos)));
      pos += stepInPerOut;
    }
    pos -= 1.0;
    previous = current;
  }
  return out;
}

// Single-bin Goertzel: peak amplitude of `samples` at `freq` Hz (sampled at `rate`).
double goertzelAmplitude(const std::vector<float>& samples, double freq, double rate) {
  const double omega = 2.0 * std::numbers::pi * freq / rate;
  const double coeff = 2.0 * std::cos(omega);
  double s1 = 0.0;
  double s2 = 0.0;
  for (const float x : samples) {
    const double s0 = static_cast<double>(x) + (coeff * s1) - s2;
    s2 = s1;
    s1 = s0;
  }
  const double power = (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
  return std::sqrt(power < 0.0 ? 0.0 : power) / (static_cast<double>(samples.size()) / 2.0);
}

TEST(PcmConverter, WindowedSincSuppressesImagesBetterThanLinear) {
  constexpr double SourceRate = 22050.0;
  constexpr double TargetRate = 48000.0;
  constexpr double ToneHz = 3000.0; // mid-band: its image lands deep in the stopband
  constexpr double Amplitude = 0.5;
  constexpr std::size_t InputSamples = 8192;
  // First spectral image of the tone when upsampling from 22.05 kHz, well above
  // the 11.025 kHz cutoff.
  constexpr double ImageHz = SourceRate - ToneHz; // 19050 Hz

  const std::vector<std::byte> input = sineInt16(ToneHz, SourceRate, Amplitude, InputSamples);

  PcmConverter converter{AudioFormat{22050, 16, 1}, 48000, 1, SampleFormat::Float32};
  std::vector<std::byte> outBytes;
  converter.convert(input, outBytes);
  const std::vector<float> sinc = decodeFloat(outBytes);
  const std::vector<float> linear = linearResample(input, SourceRate / TargetRate);

  // Trim the edges (filter priming / group delay) to measure the steady state.
  constexpr std::size_t Trim = 256;
  ASSERT_GT(sinc.size(), 2 * Trim);
  ASSERT_GT(linear.size(), 2 * Trim);
  const std::vector<float> sincMid(sinc.begin() + Trim, sinc.end() - Trim);
  const std::vector<float> linMid(linear.begin() + Trim, linear.end() - Trim);

  const double sincSignal = goertzelAmplitude(sincMid, ToneHz, TargetRate);
  const double sincImage = goertzelAmplitude(sincMid, ImageHz, TargetRate);
  const double linSignal = goertzelAmplitude(linMid, ToneHz, TargetRate);
  const double linImage = goertzelAmplitude(linMid, ImageHz, TargetRate);

  const double sincImageDb = 20.0 * std::log10(sincImage / sincSignal);
  const double linImageDb = 20.0 * std::log10(linImage / linSignal);

  // Windowed-sinc pushes the image deep into the stopband (measured ~ -88 dB;
  // the -70 dB floor leaves ample margin yet still catches a weakened kernel)...
  EXPECT_LT(sincImageDb, -70.0) << "sinc image at " << sincImageDb << " dB";
  // ...materially better than the linear interpolator it replaced (regression guard).
  EXPECT_LT(sincImageDb, linImageDb - 20.0)
      << "sinc " << sincImageDb << " dB vs linear " << linImageDb << " dB";
  // The wanted tone is essentially untouched (flat passband).
  EXPECT_NEAR(sincSignal, Amplitude, 0.05);
}

TEST(PcmConverter, DownsamplingProducesRoughlyHalfTheFrames) {
  // The rare device-below-source case still runs (cutoff drops to the target
  // Nyquist to anti-alias); here we just confirm the rate and that it streams.
  PcmConverter converter{AudioFormat{48000, 16, 1}, 24000, 1, SampleFormat::Int16};
  std::vector<std::byte> out;
  constexpr std::size_t InputSamples = 2000;
  converter.convert(constantInt16(800, InputSamples), out);

  const std::size_t frames = out.size() / sizeof(std::int16_t);
  EXPECT_NEAR(static_cast<double>(frames), 0.5 * InputSamples, 64.0);
}

} // namespace
