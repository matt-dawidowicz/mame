// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstdint>

#include "catch.hpp"

#include "cdidvc_mpeg_format.h"

TEST_CASE("CD-i DVC MPEG-1 PES first-header-byte classifier covers all 256 values", "[emu][philips][dvc][mpeg]")
{
	for (unsigned value = 0; value <= 0xff; ++value)
	{
		uint8_t const data = uint8_t(value);
		cdi_dvc::mpeg1_pes_header_kind expected = cdi_dvc::mpeg1_pes_header_kind::payload_fallback;
		if (data == 0xff)
			expected = cdi_dvc::mpeg1_pes_header_kind::stuffing;
		else if ((data & 0xc0) == 0x40)
			expected = cdi_dvc::mpeg1_pes_header_kind::std_buffer;
		else if ((data & 0xf0) == 0x20)
			expected = cdi_dvc::mpeg1_pes_header_kind::pts;
		else if ((data & 0xf0) == 0x30)
			expected = cdi_dvc::mpeg1_pes_header_kind::pts_dts;
		else if (data == 0x0f)
			expected = cdi_dvc::mpeg1_pes_header_kind::no_timestamp;

		INFO("data=" << value);
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(data) == expected);
	}
}

TEST_CASE("CD-i DVC MPEG-1 PES classifier preserves parser precedence", "[emu][philips][dvc][mpeg]")
{
	using kind = cdi_dvc::mpeg1_pes_header_kind;

	REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(0xff) == kind::stuffing);
	for (unsigned value = 0x40; value <= 0x7f; ++value)
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(uint8_t(value)) == kind::std_buffer);
	for (unsigned value = 0x20; value <= 0x2f; ++value)
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(uint8_t(value)) == kind::pts);
	for (unsigned value = 0x30; value <= 0x3f; ++value)
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(uint8_t(value)) == kind::pts_dts);
	REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(0x0f) == kind::no_timestamp);
}

TEST_CASE("CD-i DVC MPEG-1 PES headers cannot continue beyond the packet boundary", "[emu][philips][dvc][mpeg]")
{
	using kind = cdi_dvc::mpeg1_pes_header_kind;

	for (kind const value :
			{ kind::stuffing, kind::std_buffer, kind::pts, kind::pts_dts,
				kind::no_timestamp, kind::payload_fallback })
	{
		REQUIRE_FALSE(cdi_dvc::mpeg1_pes_header_can_continue(value, 0));
	}

	for (kind const value : { kind::stuffing, kind::std_buffer, kind::pts, kind::pts_dts })
	{
		REQUIRE(cdi_dvc::mpeg1_pes_header_can_continue(value, 1));
		REQUIRE(cdi_dvc::mpeg1_pes_header_can_continue(value, 0xffff));
	}

	for (kind const value : { kind::no_timestamp, kind::payload_fallback })
	{
		REQUIRE_FALSE(cdi_dvc::mpeg1_pes_header_can_continue(value, 1));
		REQUIRE_FALSE(cdi_dvc::mpeg1_pes_header_can_continue(value, 0xffff));
	}
}
