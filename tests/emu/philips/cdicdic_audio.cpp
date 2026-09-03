// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdicdic_audio.h"

TEST_CASE("CDIC audio coding accepts only Green Book base audio formats", "[emu][philips][cdic][audio][coding]")
{
	const auto level_a_mono = cdic_audio::decode_coding(0x10);
	REQUIRE(level_a_mono.valid);
	REQUIRE_FALSE(level_a_mono.stereo);
	REQUIRE(level_a_mono.bits_per_sample == 8);
	REQUIRE(level_a_mono.sample_rate_divisor == 512);
	REQUIRE(level_a_mono.maximum_range == 8);
	REQUIRE(level_a_mono.sector_count == 4);

	const auto level_a_stereo = cdic_audio::decode_coding(0x11);
	REQUIRE(level_a_stereo.valid);
	REQUIRE(level_a_stereo.stereo);
	REQUIRE(level_a_stereo.bits_per_sample == 8);
	REQUIRE(level_a_stereo.sector_count == 2);

	const auto level_b_stereo = cdic_audio::decode_coding(0x01);
	REQUIRE(level_b_stereo.valid);
	REQUIRE(level_b_stereo.bits_per_sample == 4);
	REQUIRE(level_b_stereo.sample_rate_divisor == 512);
	REQUIRE(level_b_stereo.maximum_range == 12);
	REQUIRE(level_b_stereo.sector_count == 4);

	const auto level_c_mono = cdic_audio::decode_coding(0x04);
	REQUIRE(level_c_mono.valid);
	REQUIRE(level_c_mono.bits_per_sample == 4);
	REQUIRE(level_c_mono.sample_rate_divisor == 1024);
	REQUIRE(level_c_mono.sector_count == 16);

	const auto emphasized = cdic_audio::decode_coding(0x51);
	REQUIRE(emphasized.valid);
	REQUIRE(emphasized.emphasis);

	REQUIRE_FALSE(cdic_audio::decode_coding(0x20).valid); // reserved 16-bit field
	REQUIRE_FALSE(cdic_audio::decode_coding(0x30).valid); // reserved bits/sample field
	REQUIRE_FALSE(cdic_audio::decode_coding(0x08).valid); // reserved sample-rate field
	REQUIRE_FALSE(cdic_audio::decode_coding(0x02).valid); // reserved channel field
	REQUIRE_FALSE(cdic_audio::decode_coding(0x80).valid); // reserved bit 7
}

TEST_CASE("CDIC ADPCM sound parameters honor level-specific range limits", "[emu][philips][cdic][audio][coding][range]")
{
	// Level A is 8-bit and only defines ranges 0-8.
	REQUIRE(cdic_audio::normalize_sound_parameter(0x27, 8) == 0x27);
	REQUIRE(cdic_audio::normalize_sound_parameter(0x28, 8) == 0x28);
	REQUIRE(cdic_audio::normalize_sound_parameter(0x29, 8) == 0x28);
	REQUIRE(cdic_audio::normalize_sound_parameter(0x2f, 8) == 0x28);

	// Levels B/C are 4-bit and define ranges 0-12.
	REQUIRE(cdic_audio::normalize_sound_parameter(0x3b, 12) == 0x3b);
	REQUIRE(cdic_audio::normalize_sound_parameter(0x3c, 12) == 0x3c);
	REQUIRE(cdic_audio::normalize_sound_parameter(0x3d, 12) == 0x3c);
	REQUIRE(cdic_audio::normalize_sound_parameter(0x3f, 12) == 0x3c);
}

TEST_CASE("CDIC attenuation matrix implements one-decibel controls and mute", "[emu][philips][cdic][audio][mixer]")
{
	constexpr uint32_t unity = uint32_t(1) << 30;
	REQUIRE(cdic_audio::attenuation_gain_q30(0x00) == unity);
	REQUIRE(cdic_audio::attenuation_gain_q30(0x80) == 0);

	// Twenty 1 dB stages must be essentially -20 dB (gain 0.1).
	const uint32_t minus_20_db = cdic_audio::attenuation_gain_q30(20);
	REQUIRE(minus_20_db >= unity / 10 - 2);
	REQUIRE(minus_20_db <= unity / 10 + 2);

	// Direct stereo routing: L->L and R->R at 0 dB, cross paths muted.
	auto mixed = cdic_audio::mix_sample(12000, -5000, 0x00, 0x80, 0x00, 0x80);
	REQUIRE(mixed.left == 12000);
	REQUIRE(mixed.right == -5000);

	mixed = cdic_audio::mix_sample(10000, 0, 20, 0x80, 0x80, 0x80);
	REQUIRE(mixed.left >= 999);
	REQUIRE(mixed.left <= 1001);
	REQUIRE(mixed.right == 0);

	mixed = cdic_audio::mix_sample(12345, -23456, 0x80, 0x80, 0x80, 0x80);
	REQUIRE(mixed.left == 0);
	REQUIRE(mixed.right == 0);

	// Applications are responsible for avoiding over-level mixes; the emulator
	// clips the resulting PCM value instead of applying an undocumented gain.
	mixed = cdic_audio::mix_sample(30000, 30000, 0x00, 0x00, 0x00, 0x00);
	REQUIRE(mixed.left == 32767);
	REQUIRE(mixed.right == 32767);
	mixed = cdic_audio::mix_sample(-30000, -30000, 0x00, 0x00, 0x00, 0x00);
	REQUIRE(mixed.left == -32768);
	REQUIRE(mixed.right == -32768);
}

TEST_CASE("CDIC 50/15 microsecond de-emphasis has unity DC and 0.3 Nyquist gain", "[emu][philips][cdic][audio][emphasis]")
{
	for (const uint32_t sample_rate : std::array<uint32_t, 3>{ 44100, 37800, 18900 })
	{
		double previous_input = 0.0;
		double previous_output = 0.0;
		int16_t output = 0;
		for (unsigned sample = 0; sample < 1000; ++sample)
			output = cdic_audio::deemphasis_sample(10000, previous_input, previous_output, sample_rate);
		REQUIRE(output >= 9999);
		REQUIRE(output <= 10000);

		previous_input = 0.0;
		previous_output = 0.0;
		for (unsigned sample = 0; sample < 2000; ++sample)
		{
			const int16_t input = (sample & 1) ? -10000 : 10000;
			output = cdic_audio::deemphasis_sample(input, previous_input, previous_output, sample_rate);
		}
		REQUIRE(output >= -3001);
		REQUIRE(output <= -2999);
	}
}
