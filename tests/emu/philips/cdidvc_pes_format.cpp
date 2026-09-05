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

TEST_CASE("CD-i DVC start-code routing covers every audio and video stream", "[emu][philips][dvc][mpeg][audio][exhaustive]")
{
	using route = cdi_dvc::mpeg1_start_code_route;

	for (bool const for_fma : { false, true })
	{
		unsigned const stream_count = for_fma ? 32 : 16;
		uint8_t const first_stream_id = for_fma ? 0xc0 : 0xe0;
		uint8_t const last_stream_id = for_fma ? 0xdf : 0xef;
		uint8_t const stream_mask = for_fma ? 0x1f : 0x0f;

		for (uint16_t selected_stream = 0; selected_stream < stream_count; ++selected_stream)
		{
			for (unsigned value = 0; value <= 0xff; ++value)
			{
				uint8_t const stream_id = uint8_t(value);
				route expected = route::skipped_packet;
				if (stream_id == 0xba)
					expected = route::pack_header;
				else if (stream_id == 0xb9)
					expected = route::program_end;
				else if (stream_id >= first_stream_id && stream_id <= last_stream_id
						&& (stream_id & stream_mask) == selected_stream)
					expected = route::selected_pes;

				INFO("for_fma=" << for_fma << " selected_stream=" << selected_stream
					<< " stream_id=" << value);
				REQUIRE(cdi_dvc::classify_mpeg1_start_code(
						for_fma, stream_id, selected_stream) == expected);
			}
		}
	}

	// The fifth audio stream-number bit is significant: Cx and Dx packets
	// must never alias one another.  High register bits remain reserved.
	for (uint16_t low = 0; low < 16; ++low)
	{
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xc0 | low), low)
			== route::selected_pes);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xd0 | low), low)
			== route::skipped_packet);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xc0 | low), 0x10 | low)
			== route::skipped_packet);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xd0 | low), 0x10 | low)
			== route::selected_pes);
	}

	for (uint32_t value = 0; value <= 0xffff; ++value)
	{
		REQUIRE(cdi_dvc::normalize_mpeg_stream_number(true, uint16_t(value)) == (value & 0x1f));
		REQUIRE(cdi_dvc::normalize_mpeg_stream_number(false, uint16_t(value)) == (value & 0x0f));
	}
}

TEST_CASE("CD-i DVC selected PES routing preserves every first-header-byte path", "[emu][philips][dvc][mpeg][audio][exhaustive]")
{
	using route = cdi_dvc::mpeg1_start_code_route;
	using kind = cdi_dvc::mpeg1_pes_header_kind;

	for (uint16_t selected_stream = 0; selected_stream < 32; ++selected_stream)
	{
		uint8_t const selected_id = uint8_t(0xc0 | selected_stream);
		uint8_t const other_id = uint8_t(0xc0 | ((selected_stream + 1) & 0x1f));
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, selected_id, selected_stream)
			== route::selected_pes);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, other_id, selected_stream)
			== route::skipped_packet);

		for (unsigned value = 0; value <= 0xff; ++value)
		{
			uint8_t const data = uint8_t(value);
			kind const header = cdi_dvc::classify_mpeg1_pes_header_byte(data);
			bool const continuation = cdi_dvc::mpeg1_pes_header_can_continue(header, 1);
			bool const expected_continuation = header == kind::stuffing
				|| header == kind::std_buffer || header == kind::pts || header == kind::pts_dts;
			INFO("selected_stream=" << selected_stream << " first_header_byte=" << value);
			REQUIRE(continuation == expected_continuation);
		}
	}
}
