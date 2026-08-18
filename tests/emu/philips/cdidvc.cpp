// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "cdidvc_utils.h"

TEST_CASE("CD-i DVC MPEG timestamp deltas wrap at 33 bits", "[emu][philips][dvc]")
{
	constexpr uint64_t wrap = uint64_t(1) << 33;
	constexpr uint64_t mask = wrap - 1;

	REQUIRE(cdi_dvc::mpeg_timestamp_delta(0, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(100, 40) == 60);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(40, 100) == -60);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(1, mask) == 2);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(mask, 1) == -2);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta((wrap >> 1) - 1, 0) == int64_t((wrap >> 1) - 1));
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(wrap >> 1, 0) == -int64_t(wrap >> 1));
}

TEST_CASE("CD-i DVC MPEG timestamps convert from 90 kHz to 45 kHz deltas", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_dclk_delta(0, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(180, 90) == 45);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(90, 180) == -45);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(181, 90) == 45);
}

TEST_CASE("CD-i DVC FMA stream selection accepts matching MPEG audio streams", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_stream_selected(true, 0xc0, 0));
	REQUIRE(cdi_dvc::mpeg_stream_selected(true, 0xcf, 0x000f));
	REQUIRE(cdi_dvc::mpeg_stream_selected(true, 0xd3, 0x00f3));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(true, 0xc1, 0));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(true, 0xbf, 0x000f));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(true, 0xe0, 0));
}

TEST_CASE("CD-i DVC FMV stream selection accepts matching MPEG video streams", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_stream_selected(false, 0xe0, 0));
	REQUIRE(cdi_dvc::mpeg_stream_selected(false, 0xef, 0x000f));
	REQUIRE(cdi_dvc::mpeg_stream_selected(false, 0xe7, 0x00f7));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(false, 0xe1, 0));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(false, 0xdf, 0x000f));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(false, 0xf0, 0));
}
