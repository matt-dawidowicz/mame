// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cmath>
#include <cstdint>

#include "catch.hpp"

#include "cdiaudio.h"
#include "cdiaudio_dsp56001.h"

TEST_CASE("VMPEG FMA attenuation uses the complete firmware Q22 coefficient curve", "[emu][philips][audio][dvc][fma][attenuation]")
{
	REQUIRE(cdi_audio::FMA_ATTENUATION_FRACTIONAL_BITS == 22);
	REQUIRE(cdi_audio::FMA_ATTENUATION_SCALE == 0x400000U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22.size() == 128);

	uint32_t previous = cdi_audio::FMA_ATTENUATION_SCALE;
	for (unsigned db = 0; db < 128; ++db)
	{
		uint32_t const coefficient = cdi_audio::FMA_ATTENUATION_Q22[db];
		uint32_t const formula = uint32_t(std::llround(
			std::pow(10.0, -double(db) / 20.0)
			* double(cdi_audio::FMA_ATTENUATION_SCALE)));

		INFO("attenuation=" << db << " coefficient=" << coefficient);
		REQUIRE(coefficient == formula);
		REQUIRE(coefficient <= previous);
		REQUIRE(cdi_audio::fma_attenuation_coefficient(uint8_t(db)) == coefficient);
		REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(uint8_t(db), 22).coefficient == coefficient);
		previous = coefficient;
	}

	// Pin both ends and several interior points so an accidental scale change is
	// immediately visible instead of being hidden by the formula cross-check.
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[0] == 0x400000U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[1] == 0x390a41U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[6] == 0x201374U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[20] == 0x066666U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[40] == 0x00a3d7U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[80] == 0x0001a3U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[120] == 0x000004U);
	REQUIRE(cdi_audio::FMA_ATTENUATION_Q22[127] == 0x000002U);

	// The mute bit remains an exact zero independent of the lower seven bits.
	for (unsigned raw = 0x80; raw <= 0xff; ++raw)
	{
		INFO("muted raw=" << raw);
		REQUIRE(cdi_audio::fma_attenuation_coefficient(uint8_t(raw)) == 0U);
	}
}

TEST_CASE("VMPEG FMA Q22 matrix preserves routing and high attenuation", "[emu][philips][audio][dvc][fma][attenuation][matrix]")
{
	cdi_audio::attenuation_gains const straight =
		cdi_audio::make_fma_attenuation_gains(cdi_audio::STRAIGHT_ATTENUATION);
	REQUIRE(straight.ll == cdi_audio::FMA_ATTENUATION_SCALE);
	REQUIRE(straight.lr == 0U);
	REQUIRE(straight.rr == cdi_audio::FMA_ATTENUATION_SCALE);
	REQUIRE(straight.rl == 0U);

	auto unchanged = cdi_audio::mix_fma_attenuated_pcm16(straight, 12345, -23456);
	REQUIRE(unchanged.left == 12345);
	REQUIRE(unchanged.right == -23456);

	cdi_audio::attenuation_matrix const quiet_matrix = { 0x7f, 0x80, 0x7f, 0x80 };
	cdi_audio::attenuation_gains const quiet =
		cdi_audio::make_fma_attenuation_gains(quiet_matrix);
	REQUIRE(quiet.ll == 2U);
	REQUIRE(quiet.rr == 2U);
	REQUIRE(quiet.lr == 0U);
	REQUIRE(quiet.rl == 0U);

	auto quiet_full_scale = cdi_audio::mix_fma_attenuated_pcm16(quiet, 32767, -32768);
	REQUIRE(quiet_full_scale.left == 0);
	REQUIRE(quiet_full_scale.right == 0);
}

TEST_CASE("VMPEG FMA Q22 reduction uses convergent ties and explicit PCM saturation", "[emu][philips][audio][dvc][fma][rounding][saturation]")
{
	constexpr int64_t unit = int64_t(1) << cdi_audio::FMA_ATTENUATION_FRACTIONAL_BITS;
	constexpr int64_t half = unit >> 1;

	REQUIRE(cdi_audio::fma_reduce_q22_nearest_even(2 * unit + half) == 2);
	REQUIRE(cdi_audio::fma_reduce_q22_nearest_even(3 * unit + half) == 4);
	REQUIRE(cdi_audio::fma_reduce_q22_nearest_even(-(2 * unit + half)) == -2);
	REQUIRE(cdi_audio::fma_reduce_q22_nearest_even(-(3 * unit + half)) == -4);

	// Both matrix inputs at unity can exceed a signed 16-bit output but are still
	// far inside the documented 56-bit DSP accumulator.
	REQUIRE(cdi_audio::FMA_Q22_MAX_ABS_ACCUMULATOR == (int64_t(1) << 38));
	REQUIRE(cdi_audio::FMA_Q22_MAX_ABS_ACCUMULATOR < (int64_t(1) << 55));
	REQUIRE(cdi_audio::fma_mix_q22_accumulator(
		-32768, cdi_audio::FMA_ATTENUATION_SCALE,
		-32768, cdi_audio::FMA_ATTENUATION_SCALE)
		== -cdi_audio::FMA_Q22_MAX_ABS_ACCUMULATOR);

	cdi_audio::attenuation_matrix const all_unity = { 0x00, 0x00, 0x00, 0x00 };
	cdi_audio::attenuation_gains const gain =
		cdi_audio::make_fma_attenuation_gains(all_unity);

	auto clipped_high = cdi_audio::mix_fma_attenuated_pcm16(gain, 20000, 20000);
	REQUIRE(clipped_high.left == 32767);
	REQUIRE(clipped_high.right == 32767);

	auto clipped_low = cdi_audio::mix_fma_attenuated_pcm16(gain, -20000, -20000);
	REQUIRE(clipped_low.left == -32768);
	REQUIRE(clipped_low.right == -32768);
}

TEST_CASE("VMPEG FMA normalized wrapper round-trips decoded int16 PCM exactly", "[emu][philips][audio][dvc][fma][pcm]")
{
	cdi_audio::attenuation_gains const straight =
		cdi_audio::make_nominal_attenuation_gains(cdi_audio::STRAIGHT_ATTENUATION);

	for (int32_t sample = -32768; sample <= 32767; ++sample)
	{
		double const normalized = double(sample) / 32768.0;
		cdi_audio::stereo_sample const mixed =
			cdi_audio::mix_attenuated_stereo(straight, normalized, normalized);
		INFO("sample=" << sample);
		REQUIRE(mixed.left == normalized);
		REQUIRE(mixed.right == normalized);
	}
}
