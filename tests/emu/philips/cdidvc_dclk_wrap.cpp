// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstdint>
#include <limits>

#include "catch.hpp"

#include "cdidvc_utils.h"

TEST_CASE("CD-i DVC signed 32-bit wrap delta handles boundary cases explicitly", "[emu][philips][dvc][timing]")
{
	REQUIRE(cdi_dvc::signed_wrap_delta32(0, 0) == 0);
	REQUIRE(cdi_dvc::signed_wrap_delta32(1, 0) == 1);
	REQUIRE(cdi_dvc::signed_wrap_delta32(0, 1) == -1);
	REQUIRE(cdi_dvc::signed_wrap_delta32(0, 0xffffffffU) == 1);
	REQUIRE(cdi_dvc::signed_wrap_delta32(0xffffffffU, 0) == -1);
	REQUIRE(cdi_dvc::signed_wrap_delta32(0x7fffffffU, 0) == std::numeric_limits<int32_t>::max());
	REQUIRE(cdi_dvc::signed_wrap_delta32(0x80000000U, 0) == std::numeric_limits<int32_t>::min());
	REQUIRE(cdi_dvc::signed_wrap_delta32(0, 0x80000000U) == std::numeric_limits<int32_t>::min());
}

TEST_CASE("CD-i DVC signed 32-bit wrap delta reconstructs endpoints", "[emu][philips][dvc][timing]")
{
	uint64_t state = 0xbb67ae8584caa73bULL;
	for (unsigned iteration = 0; iteration < 16'384; ++iteration)
	{
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		uint32_t const lhs = uint32_t(state);
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		uint32_t const rhs = uint32_t(state);
		int32_t const delta = cdi_dvc::signed_wrap_delta32(lhs, rhs);
		int32_t const reverse = cdi_dvc::signed_wrap_delta32(rhs, lhs);

		INFO("iteration=" << iteration << " lhs=" << lhs << " rhs=" << rhs << " delta=" << delta);
		REQUIRE(uint32_t(rhs + uint32_t(delta)) == lhs);
		if (delta == std::numeric_limits<int32_t>::min())
			REQUIRE(reverse == std::numeric_limits<int32_t>::min());
		else
			REQUIRE(reverse == -delta);

		uint64_t const lhs90 = uint64_t(lhs) << 1;
		uint64_t const rhs90 = uint64_t(rhs) << 1;
		REQUIRE(cdi_dvc::mpeg_dclk_delta(lhs90, rhs90) == delta);
	}
}
