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
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(wrap + 1, 1) == 0);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(1, wrap + 1) == 0);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(wrap + 5, wrap + 1) == 4);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(0, (wrap >> 1) + 1) == int64_t((wrap >> 1) - 1));
}

TEST_CASE("CD-i DVC MPEG timestamps convert from 90 kHz to 45 kHz deltas", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_dclk_delta(0, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(180, 90) == 45);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(90, 180) == -45);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(181, 90) == 45);

	constexpr uint64_t wrap = uint64_t(1) << 33;
	REQUIRE(cdi_dvc::mpeg_dclk_delta(wrap, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(0, wrap) == 0);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(wrap - 2, 0) == -1);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(0, wrap - 2) == 1);
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

TEST_CASE("CD-i DVC MPEG stream selection covers every stream ID and selector", "[emu][philips][dvc]")
{
	for (unsigned target = 0; target < 2; ++target)
	{
		bool const for_fma = target == 0;
		for (uint16_t selected_stream = 0; selected_stream < 16; ++selected_stream)
		{
			for (unsigned stream = 0; stream <= 0xff; ++stream)
			{
				uint8_t const stream_id = uint8_t(stream);
				bool const in_stream_range = for_fma
					? stream_id >= 0xc0 && stream_id <= 0xdf
					: stream_id >= 0xe0 && stream_id <= 0xef;
				bool const expected = in_stream_range && (stream_id & 0x0f) == selected_stream;

				INFO("for_fma=" << for_fma << " selected_stream=" << selected_stream << " stream_id=" << stream);
				REQUIRE(cdi_dvc::mpeg_stream_selected(for_fma, stream_id, selected_stream) == expected);
			}
		}
	}
}
