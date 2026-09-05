// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "mcd212_video.h"

TEST_CASE("MCD212 PAL and NTSC profiles preserve documented line and half-line timing", "[emu][philips][mcd212][timing]")
{
	using mcd212_video::make_timing_profile;

	auto timing = make_timing_profile(false, false, false, false);
	REQUIRE(timing.horizontal_total == 896);
	REQUIRE(timing.total_lines == 312);
	REQUIRE(timing.total_half_lines == 624);
	REQUIRE(timing.active_start_lines == 26);
	REQUIRE(timing.active_lines == 280);
	REQUIRE(timing.active_end_lines() == 306);
	REQUIRE(timing.blank_lines == 32);
	REQUIRE(timing.field_halfline_offset(false) == 0);

	timing = make_timing_profile(true, false, true, false);
	REQUIRE(timing.horizontal_total == 960);
	REQUIRE(timing.total_half_lines == 625);
	REQUIRE(timing.active_start_lines == 26);
	REQUIRE(timing.active_lines == 280);
	REQUIRE(timing.field_halfline_offset(true) == 0);
	REQUIRE(timing.field_halfline_offset(false) == 1);

	timing = make_timing_profile(true, false, true, true);
	REQUIRE(timing.total_half_lines == 625);
	REQUIRE(timing.active_start_lines == 46);
	REQUIRE(timing.active_lines == 240);
	REQUIRE(timing.active_end_lines() == 286);
	REQUIRE(timing.blank_lines == 72);

	timing = make_timing_profile(true, true, false, true);
	REQUIRE(timing.total_lines == 262);
	REQUIRE(timing.total_half_lines == 524);
	REQUIRE(timing.active_start_lines == 18);
	REQUIRE(timing.active_lines == 240);
	REQUIRE(timing.blank_lines == 22);

	timing = make_timing_profile(true, true, true, false);
	REQUIRE(timing.total_half_lines == 525);
	REQUIRE(timing.field_halfline_offset(true) == 0);
	REQUIRE(timing.field_halfline_offset(false) == 1);
}

TEST_CASE("MCD212 ICA/DCA field selection, fetch limits, and interrupt disables follow the register model", "[emu][philips][mcd212][control]")
{
	REQUIRE(mcd212_video::ica_pointer_word_offset(true) == 0x200);
	REQUIRE(mcd212_video::ica_pointer_word_offset(false) == 0x202);
	REQUIRE(mcd212_video::dca_bytes_per_line(false) == 32);
	REQUIRE(mcd212_video::dca_bytes_per_line(true) == 64);

	REQUIRE_FALSE(mcd212_video::interrupt_line_asserted(0x00, false, false));
	REQUIRE(mcd212_video::interrupt_line_asserted(0x04, false, false));
	REQUIRE_FALSE(mcd212_video::interrupt_line_asserted(0x04, true, false));
	REQUIRE(mcd212_video::interrupt_line_asserted(0x02, false, false));
	REQUIRE_FALSE(mcd212_video::interrupt_line_asserted(0x02, false, true));
	REQUIRE(mcd212_video::interrupt_line_asserted(0x06, true, false));
	REQUIRE(mcd212_video::interrupt_line_asserted(0x06, false, true));
	REQUIRE_FALSE(mcd212_video::interrupt_line_asserted(0x06, true, true));
}

TEST_CASE("MCD212 external-video eligibility requires two transparent planes and no cursor", "[emu][philips][mcd212][overlay]")
{
	for (unsigned enabled = 0; enabled < 2; ++enabled)
	{
		for (unsigned transparent_a = 0; transparent_a < 2; ++transparent_a)
		{
			for (unsigned transparent_b = 0; transparent_b < 2; ++transparent_b)
			{
				const bool expected = enabled && transparent_a && transparent_b;
				REQUIRE(mcd212_video::external_video_eligible(
					enabled, transparent_a, transparent_b) == expected);
			}
		}
	}

	REQUIRE_FALSE(mcd212_video::external_video_eligible(true, true, true, true));
}

TEST_CASE("MCD212 mosaic source selection is defined for zero and nonzero hold counts", "[emu][philips][mcd212][mosaic]")
{
	using mcd212_video::mosaic_source_x;

	// Zero is an evidence-bounded compatibility case: no hold, never modulo zero.
	for (std::size_t x = 0; x < 768; ++x)
		REQUIRE(mosaic_source_x(x, 0) == x);

	for (std::size_t hold = 1; hold <= 510; ++hold)
	{
		for (std::size_t x : { std::size_t(0), std::size_t(1), std::size_t(31),
				std::size_t(255), std::size_t(511), std::size_t(767) })
		{
			std::size_t const source = mosaic_source_x(x, hold);
			INFO("x=" << x << " hold=" << hold);
			REQUIRE(source <= x);
			REQUIRE(source % hold == 0);
			REQUIRE(x - source < hold);
		}
	}
}

TEST_CASE("MCD212 QHY line decoder expands single, finite-run, and end-of-line pairs", "[emu][philips][mcd212][qhy]")
{
	constexpr std::array<uint8_t, 5> encoded =
	{{
		0x2d,       // single pair: 2, 5
		0x97, 0x03, // three pairs: 1, 7
		0xc6, 0x00  // 4, 6 to end of line
	}};
	std::array<uint8_t, 12> decoded{};
	const auto result = mcd212_video::decode_qhy_line(
		[&encoded](std::size_t offset) { return encoded[offset]; },
		encoded.size(), decoded.data(), decoded.size());

	constexpr std::array<uint8_t, 12> expected =
	{{ 2, 5, 1, 7, 1, 7, 1, 7, 4, 6, 4, 6 }};
	REQUIRE(decoded == expected);
	REQUIRE(result.bytes == encoded.size());
	REQUIRE(result.pixels == decoded.size());
	REQUIRE(result.end_of_line);
	REQUIRE(result.valid);
}

TEST_CASE("MCD212 QHY line decoder rejects forbidden and unterminated encodings safely", "[emu][philips][mcd212][qhy]")
{
	std::array<uint8_t, 8> decoded{};

	constexpr std::array<uint8_t, 2> forbidden_length = {{ 0x92, 0x01 }};
	auto result = mcd212_video::decode_qhy_line(
		[&forbidden_length](std::size_t offset) { return forbidden_length[offset]; },
		forbidden_length.size(), decoded.data(), decoded.size());
	REQUIRE_FALSE(result.valid);

	constexpr std::array<uint8_t, 4> no_end = {{ 0x19, 0x19, 0x19, 0x19 }};
	result = mcd212_video::decode_qhy_line(
		[&no_end](std::size_t offset) { return no_end[offset]; },
		no_end.size(), decoded.data(), decoded.size());
	REQUIRE(result.pixels == decoded.size());
	REQUIRE_FALSE(result.end_of_line);
	REQUIRE_FALSE(result.valid);

	REQUIRE_FALSE(mcd212_video::decode_qhy_token(0x21).valid);
	REQUIRE_FALSE(mcd212_video::decode_qhy_token(0x99, 2).valid);
}

TEST_CASE("MCD212 QHY interpolation implements the separable two-dimensional FIR vectors", "[emu][philips][mcd212][qhy]")
{
	using mcd212_video::interpolate_yuv;
	using mcd212_video::pack_yuv;

	constexpr uint32_t p00 = pack_yuv(16, 32, 48);
	constexpr uint32_t p10 = pack_yuv(24, 40, 56);
	constexpr uint32_t p01 = pack_yuv(32, 48, 64);
	constexpr uint32_t p11 = pack_yuv(40, 56, 72);

	REQUIRE(interpolate_yuv(p00, p10, p01, p11, false, false) == p00);
	REQUIRE(interpolate_yuv(p00, p10, p01, p11, true, false) == pack_yuv(20, 36, 52));
	REQUIRE(interpolate_yuv(p00, p10, p01, p11, false, true) == pack_yuv(24, 40, 56));
	REQUIRE(interpolate_yuv(p00, p10, p01, p11, true, true) == pack_yuv(28, 44, 60));
}

TEST_CASE("MCD212 QHY quantization levels retain all eight bits and add independent RGB deltas", "[emu][philips][mcd212][qhy]")
{
	REQUIRE(mcd212_video::qhy_delta(0x00) == -256);
	REQUIRE(mcd212_video::qhy_delta(0x80) == 0);
	REQUIRE(mcd212_video::qhy_delta(0xff) == 254);

	REQUIRE(mcd212_video::add_qhy_level(0x804020, 0x808080) == 0x804020);
	REQUIRE(mcd212_video::add_qhy_level(0x804020, 0x817f82) == 0x823e24);
	REQUIRE(mcd212_video::add_qhy_level(0x1020f0, 0x00ff00) == 0x00ff00);
}
