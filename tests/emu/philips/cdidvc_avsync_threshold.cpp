// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "cdidvc_fidelity.h"

#include <algorithm>
#include <cstdint>

TEST_CASE("CD-i DVC A/V arithmetic tolerance is derived from clock quantization", "[emu][philips][dvc][avsync][threshold]")
{
	REQUIRE(cdi_dvc::AV_ARITHMETIC_TOLERANCE_90 == 1);
	REQUIRE(cdi_dvc::AV_ARITHMETIC_TOLERANCE_45 == 1);

	for (int64_t delta = -4; delta <= 4; ++delta)
	{
		INFO("delta90=" << delta);
		REQUIRE(cdi_dvc::clock_delta_within_arithmetic_tolerance90(delta)
			== (delta >= -1 && delta <= 1));
	}
	for (int32_t delta = -4; delta <= 4; ++delta)
	{
		INFO("delta45=" << delta);
		REQUIRE(cdi_dvc::clock_delta_within_arithmetic_tolerance45(delta)
			== (delta >= -1 && delta <= 1));
	}
}

TEST_CASE("CD-i DVC 30-minute 25 Hz MPEG A/V fixture is clock-exact", "[emu][philips][dvc][avsync][longrun][video]")
{
	constexpr uint32_t audio_rate = 44'100;
	constexpr uint64_t anchor90 = 3'456'789;
	constexpr uint32_t video_rate = 25;
	constexpr uint64_t video_period90 = 90'000 / video_rate;
	constexpr uint32_t frames = 30U * 60U * video_rate;

	for (uint32_t frame = 0; frame <= frames; ++frame)
	{
		uint64_t const audio_frames = uint64_t(frame) * audio_rate / video_rate;
		uint64_t const reference90 = cdi_dvc::mpeg_timestamp_normalize(
			anchor90 + uint64_t(frame) * video_period90);
		auto const observed = cdi_dvc::observe_audio_clock(
			anchor90, audio_frames, audio_rate,
			reference90, reference90,
			uint32_t((reference90 >> 1) & 0xffffffffU));

		INFO("frame=" << frame);
		REQUIRE(observed.sample_minus_scr90 == 0);
		REQUIRE(observed.sample_minus_pts90 == 0);
		REQUIRE(observed.sample_minus_dclk45 == 0);
		REQUIRE(cdi_dvc::audio_clock_within_arithmetic_tolerance(observed));
	}
}

TEST_CASE("CD-i DVC 30-minute 30000/1001 MPEG A/V fixture remains inside one-tick quantization", "[emu][philips][dvc][avsync][longrun][video]")
{
	constexpr uint32_t audio_rate = 44'100;
	constexpr uint64_t anchor90 = 8'000'000;
	constexpr uint32_t video_numerator = 30'000;
	constexpr uint32_t video_denominator = 1'001;
	constexpr uint64_t video_period90 = 3'003; // 90 kHz * 1001 / 30000
	constexpr uint32_t frames = (30U * 60U * video_numerator) / video_denominator;

	int64_t max_abs90 = 0;
	int32_t max_abs45 = 0;
	for (uint32_t frame = 0; frame <= frames; ++frame)
	{
		// Choose the nearest representable 44.1 kHz PCM sample to each exact
		// 30000/1001 video boundary.  Any residual is quantization, not drift.
		uint64_t const sample_numerator =
			uint64_t(frame) * audio_rate * video_denominator;
		uint64_t const audio_frames =
			(sample_numerator + video_numerator / 2U) / video_numerator;
		uint64_t const reference90 = cdi_dvc::mpeg_timestamp_normalize(
			anchor90 + uint64_t(frame) * video_period90);
		auto const observed = cdi_dvc::observe_audio_clock(
			anchor90, audio_frames, audio_rate,
			reference90, reference90,
			uint32_t((reference90 >> 1) & 0xffffffffU));

		max_abs90 = std::max<int64_t>(max_abs90,
			observed.sample_minus_scr90 < 0
				? -observed.sample_minus_scr90 : observed.sample_minus_scr90);
		max_abs45 = std::max<int32_t>(max_abs45,
			observed.sample_minus_dclk45 < 0
				? -observed.sample_minus_dclk45 : observed.sample_minus_dclk45);

		INFO("frame=" << frame);
		REQUIRE(cdi_dvc::clock_delta_within_arithmetic_tolerance90(
			observed.sample_minus_scr90));
		REQUIRE(cdi_dvc::clock_delta_within_arithmetic_tolerance90(
			observed.sample_minus_pts90));
		REQUIRE(cdi_dvc::clock_delta_within_arithmetic_tolerance45(
			observed.sample_minus_dclk45));
	}

	REQUIRE(max_abs90 == cdi_dvc::AV_ARITHMETIC_TOLERANCE_90);
	REQUIRE(max_abs45 <= cdi_dvc::AV_ARITHMETIC_TOLERANCE_45);
}
