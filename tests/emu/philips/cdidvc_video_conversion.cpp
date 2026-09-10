// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstddef>
#include <cstdint>

#include "catch.hpp"
#include "cdidvc_presentation.h"

#define PLM_NO_STDIO
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"

TEST_CASE("CD-i DVC PL_MPEG 4-byte conversion preserves RGB values", "[emu][philips][dvc][video]")
{
	constexpr unsigned width = 16;
	constexpr unsigned height = 16;
	constexpr unsigned chroma_width = width / 2;
	constexpr unsigned chroma_height = height / 2;

	std::array<uint8_t, width * height> y{};
	std::array<uint8_t, chroma_width * chroma_height> cr{};
	std::array<uint8_t, chroma_width * chroma_height> cb{};
	for (std::size_t index = 0; index < y.size(); ++index)
		y[index] = uint8_t(16U + (index * 37U) % 220U);
	for (std::size_t index = 0; index < cr.size(); ++index)
	{
		cr[index] = uint8_t(32U + (index * 29U) % 192U);
		cb[index] = uint8_t(48U + (index * 43U) % 176U);
	}

	plm_frame_t frame{};
	frame.width = width;
	frame.height = height;
	frame.y = { width, height, y.data() };
	frame.cr = { chroma_width, chroma_height, cr.data() };
	frame.cb = { chroma_width, chroma_height, cb.data() };

	std::array<uint8_t, width * height * 3> rgb{};
	std::array<uint8_t, width * height * 4> bgra{};
	std::array<uint8_t, width * height * 4> argb{};
	bgra.fill(0xa5);
	argb.fill(0x5a);

	plm_frame_to_rgb(&frame, rgb.data(), width * 3);
	plm_frame_to_bgra(&frame, bgra.data(), width * 4);
	plm_frame_to_argb(&frame, argb.data(), width * 4);

	for (std::size_t pixel = 0; pixel < width * height; ++pixel)
	{
		INFO("pixel=" << pixel);
		REQUIRE(bgra[pixel * 4 + 0] == rgb[pixel * 3 + 2]);
		REQUIRE(bgra[pixel * 4 + 1] == rgb[pixel * 3 + 1]);
		REQUIRE(bgra[pixel * 4 + 2] == rgb[pixel * 3 + 0]);
		REQUIRE(bgra[pixel * 4 + 3] == 0xa5);

		REQUIRE(argb[pixel * 4 + 0] == 0x5a);
		REQUIRE(argb[pixel * 4 + 1] == rgb[pixel * 3 + 0]);
		REQUIRE(argb[pixel * 4 + 2] == rgb[pixel * 3 + 1]);
		REQUIRE(argb[pixel * 4 + 3] == rgb[pixel * 3 + 2]);
	}
}

TEST_CASE("CD-i DVC VCD mode uses the 15 MHz to 13.5 MHz horizontal cadence", "[emu][philips][dvc][video][vcd]")
{
	REQUIRE_FALSE(cdi_dvc::vcd_pixel_clock_enabled(0));
	REQUIRE(cdi_dvc::vcd_pixel_clock_enabled(1));

	REQUIRE(cdi_dvc::video_output_width(0, false) == 0);
	REQUIRE(cdi_dvc::video_output_width(352, false) == 704);
	REQUIRE(cdi_dvc::video_output_width(352, true) == 782);
	REQUIRE(cdi_dvc::video_output_width(345, true) == 767);

	for (unsigned source = 0; source < 16; ++source)
	{
		REQUIRE(cdi_dvc::video_source_x_for_output(source * 2, 16, false) == source);
		REQUIRE(cdi_dvc::video_source_x_for_output(source * 2 + 1, 16, false) == source);
	}

	unsigned previous = 0;
	for (unsigned output = 0; output < cdi_dvc::video_output_width(352, true); ++output)
	{
		unsigned const source = cdi_dvc::video_source_x_for_output(output, 352, true);
		INFO("output=" << output << " source=" << source);
		REQUIRE(source < 352);
		REQUIRE(source >= previous);
		REQUIRE(source <= previous + 1);
		previous = source;
	}
	REQUIRE(cdi_dvc::video_source_x_for_output(781, 352, true) == 351);
}

TEST_CASE("CD-i DVC synchronous video clock follows programmed GEN_SYSCR", "[emu][philips][dvc][video][avsync]")
{
	auto clock = cdi_dvc::select_video_sync_clock(
		true, 45'123, true, 1'000, 100);
	REQUIRE(clock.valid);
	REQUIRE(clock.external_dclk);
	REQUIRE(clock.clock90 == 90'246);

	clock = cdi_dvc::select_video_sync_clock(
		false, 145, true, 1'000, 100);
	REQUIRE(clock.valid);
	REQUIRE_FALSE(clock.external_dclk);
	REQUIRE(clock.clock90 == 1'090);

	clock = cdi_dvc::select_video_sync_clock(
		false, 145, false, 0, 0);
	REQUIRE_FALSE(clock.valid);

	clock = cdi_dvc::select_video_sync_clock(
		true, 0xffffffffU, false, 0, 0);
	REQUIRE(clock.valid);
	REQUIRE(clock.clock90 == cdi_dvc::MPEG_TIMESTAMP_MASK - 1U);
}
