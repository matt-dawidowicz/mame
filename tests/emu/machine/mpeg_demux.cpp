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
