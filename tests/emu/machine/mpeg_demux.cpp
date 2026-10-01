// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#include "emu.h"
#include "catch.hpp"

#include "machine/mpeg_demux.h"

#include <cstdint>
#include <limits>

TEST_CASE("MPEG 33-bit timestamp delta handles wrap boundaries", "[emu][machine][mpeg][timestamp]")
{
	constexpr s64 mask = 0x1ffffffffLL;
	constexpr s64 half = 0x100000000LL;

	REQUIRE(mpeg_timestamp_diff(0, 0) == 0);
	REQUIRE(mpeg_timestamp_diff(1, 0) == 1);
	REQUIRE(mpeg_timestamp_diff(0, 1) == -1);
	REQUIRE(mpeg_timestamp_diff(0, mask) == 1);
	REQUIRE(mpeg_timestamp_diff(mask, 0) == -1);
	REQUIRE(mpeg_timestamp_diff(half - 1, 0) == half - 1);
	REQUIRE(mpeg_timestamp_diff(half, 0) == -half);
	REQUIRE(mpeg_timestamp_diff(0, half) == -half);
}

TEST_CASE("MPEG 33-bit timestamp delta reconstructs endpoints", "[emu][machine][mpeg][timestamp]")
{
	constexpr uint64_t mask = 0x1ffffffffULL;
	constexpr int64_t half = 0x100000000LL;
	uint64_t state = 0xbb67ae8584caa73bULL;

	for (unsigned iteration = 0; iteration < 16384; ++iteration)
	{
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		uint64_t const lhs = state & mask;
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		uint64_t const rhs = state & mask;

		int64_t const delta = mpeg_timestamp_diff(lhs, rhs);
		int64_t const reverse = mpeg_timestamp_diff(rhs, lhs);

		INFO("iteration=" << iteration << " lhs=" << lhs << " rhs=" << rhs << " delta=" << delta);
		REQUIRE(((rhs + uint64_t(delta)) & mask) == lhs);

		if (delta == -half)
			REQUIRE(reverse == -half);
		else
			REQUIRE(reverse == -delta);
	}
}


namespace
{

void feed_start_code(mpeg_demux &demux, u8 code, u8 stream_filter)
{
	demux.byte(0x00, stream_filter);
	demux.byte(0x00, stream_filter);
	demux.byte(0x01, stream_filter);
	demux.byte(code, stream_filter);
}

} // anonymous namespace

TEST_CASE("MPEG demux selects the requested low-nibble audio/video stream", "[emu][machine][mpeg][demux]")
{
	for (u8 base : { u8(0xc0), u8(0xe0) })
	{
		for (u8 selected = 0; selected < 16; ++selected)
		{
			for (u8 candidate = 0; candidate < 16; ++candidate)
			{
				mpeg_demux demux;
				demux.reset();

				feed_start_code(demux, base | candidate, selected);
				demux.byte(0x00, selected); // PES length high
				demux.byte(0x02, selected); // PES length low
				demux.byte(0x0f, selected); // MPEG-1 PES: no timestamp

				INFO("base=" << unsigned(base) << " selected=" << unsigned(selected)
					<< " candidate=" << unsigned(candidate));
				REQUIRE(demux.packet_body == (selected == candidate));

				demux.byte(0x5a, selected); // single payload byte
				REQUIRE_FALSE(demux.packet_body);
			}
		}
	}
}

TEST_CASE("MPEG demux ignores non-selected PES packets without leaking payload state", "[emu][machine][mpeg][demux]")
{
	mpeg_demux demux;
	demux.reset();

	feed_start_code(demux, 0xc3, 7);
	demux.byte(0x00, 7);
	demux.byte(0x04, 7);
	demux.byte(0x0f, 7);
	REQUIRE_FALSE(demux.packet_body);

	for (u8 data : { u8(0x00), u8(0x00), u8(0x01), u8(0xb9) })
	{
		demux.byte(data, 7);
		REQUIRE_FALSE(demux.packet_body);
	}
}

TEST_CASE("MPEG demux emits a one-byte program-end pulse", "[emu][machine][mpeg][demux]")
{
	mpeg_demux demux;
	demux.reset();

	feed_start_code(demux, 0xb9, 0);
	REQUIRE(demux.program_end);
	REQUIRE_FALSE(demux.packet_body);

	demux.byte(0x12, 0);
	REQUIRE_FALSE(demux.program_end);
	REQUIRE_FALSE(demux.packet_body);
}
