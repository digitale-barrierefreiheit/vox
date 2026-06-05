// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::audio::PcmConverter (rate / channels / sample format).
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
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

TEST(PcmConverter, SameRateInt16IsEffectivelyPassthrough) {
  PcmConverter converter{AudioFormat{48000, 16, 1}, 48000, 1, SampleFormat::Int16};
  std::vector<std::byte> out;
  const std::int16_t dc = 1000;
  converter.convert(constantInt16(dc, 64), out);

  ASSERT_GT(out.size(), 0U);
  const std::size_t frames = out.size() / sizeof(std::int16_t);
  // After the one-sample priming ramp, a constant input yields the constant.
  EXPECT_EQ(int16At(out, frames - 1), dc);
}

TEST(PcmConverter, UpsamplingRoughlyDoublesFrameCount) {
  PcmConverter converter{AudioFormat{22050, 16, 1}, 44100, 1, SampleFormat::Int16};
  std::vector<std::byte> out;
  constexpr std::size_t InputSamples = 1000;
  converter.convert(constantInt16(500, InputSamples), out);

  const std::size_t frames = out.size() / sizeof(std::int16_t);
  // 2x rate -> ~2x frames (allow a couple for resampler phase).
  EXPECT_NEAR(static_cast<double>(frames), 2.0 * InputSamples, 3.0);
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
  // At equal rates the stream passes through with a one-sample priming delay, so
  // input[i] appears at output[i+1]: the int16 extremes must survive exactly.
  converter.convert(int16Bytes({1000, -1000, 32767, -32768, 0}), out);

  ASSERT_GE(out.size() / sizeof(std::int16_t), 5U);
  EXPECT_EQ(int16At(out, 3), 32767);
  EXPECT_EQ(int16At(out, 4), -32768);
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

} // namespace
