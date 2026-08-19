// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdidvc_utils.h"

namespace
{

constexpr std::array<uint8_t, 5> encode_timestamp(uint64_t value, uint8_t tag)
{
	value &= cdi_dvc::MPEG_TIMESTAMP_MASK;
	return {
		uint8_t((tag & 0xf0) | (((value >> 30) & 0x07) << 1) | 0x01),
		uint8_t(value >> 22),
		uint8_t((((value >> 15) & 0x7f) << 1) | 0x01),
		uint8_t(value >> 7),
		uint8_t(((value & 0x7f) << 1) | 0x01)
	};
}

} // anonymous namespace

TEST_CASE("CD-i DVC MPEG timestamp field decoding round-trips 33-bit values", "[emu][philips][dvc][mpeg]")
{
	constexpr uint64_t values[] = {
		0,
		1,
		2,
		89'999,
		90'000,
		0x123456789ULL,
		cdi_dvc::MPEG_TIMESTAMP_MASK - 1,
		cdi_dvc::MPEG_TIMESTAMP_MASK
	};
	constexpr uint8_t tags[] = { 0x10, 0x20, 0x30 };

	for (uint64_t value : values)
	{
		for (uint8_t tag : tags)
		{
			auto const bytes = encode_timestamp(value, tag);
			INFO("value=" << value << " tag=" << unsigned(tag));
			REQUIRE(cdi_dvc::decode_mpeg1_timestamp_field(
				bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]) == value);
			REQUIRE(cdi_dvc::mpeg1_timestamp_marker_bits_valid(bytes[0], bytes[2], bytes[4]));
		}
	}
}

TEST_CASE("CD-i DVC MPEG timestamp marker validation checks all three marker positions", "[emu][philips][dvc][mpeg]")
{
	auto bytes = encode_timestamp(0x123456789ULL, 0x20);
	REQUIRE(cdi_dvc::mpeg1_timestamp_marker_bits_valid(bytes[0], bytes[2], bytes[4]));

	for (unsigned marker = 0; marker < 3; ++marker)
	{
		auto broken = bytes;
		broken[marker == 0 ? 0 : marker == 1 ? 2 : 4] &= 0xfe;
		INFO("marker=" << marker);
		REQUIRE_FALSE(cdi_dvc::mpeg1_timestamp_marker_bits_valid(broken[0], broken[2], broken[4]));
		REQUIRE(cdi_dvc::decode_mpeg1_timestamp_field(
			broken[0], broken[1], broken[2], broken[3], broken[4]) == 0x123456789ULL);
	}
}
