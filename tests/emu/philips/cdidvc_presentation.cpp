// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "cdidvc_presentation.h"

#include <array>

TEST_CASE("CD-i DVC VCD mode uses the 15 MHz to 13.5 MHz horizontal cadence", "[emu][philips][dvc][video][vcd]")
{
	REQUIRE_FALSE(cdi_dvc::vcd_pixel_clock_enabled(0));
	REQUIRE(cdi_dvc::vcd_pixel_clock_enabled(1));

	REQUIRE(cdi_dvc::video_output_width(0, false) == 0);
	REQUIRE(cdi_dvc::video_output_width(352, false) == 704);
	REQUIRE(cdi_dvc::video_output_width(352, true) == 782);
	// Real White Book material commonly crops a handful of encoded edge pixels;
	// 345 samples at 13.5 MHz fill almost exactly the 384-sample CD-i aperture.
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
	// Once GEN_SYSCR has been explicitly programmed, the live VMPEG DCLK is the
	// display clock even if the most recently demuxed pack SCR belongs to a
	// different local anchor.
	auto clock = cdi_dvc::select_video_sync_clock(
		true, 45'123, true, 1'000, 100);
	REQUIRE(clock.valid);
	REQUIRE(clock.external_dclk);
	REQUIRE(clock.clock90 == 90'246);

	// Before guest software establishes GEN_SYSCR, preserve the stream-SCR
	// anchored compatibility path.  This is needed for isolated/reset streams.
	clock = cdi_dvc::select_video_sync_clock(
		false, 145, true, 1'000, 100);
	REQUIRE(clock.valid);
	REQUIRE_FALSE(clock.external_dclk);
	REQUIRE(clock.clock90 == 1'090);

	clock = cdi_dvc::select_video_sync_clock(
		false, 145, false, 0, 0);
	REQUIRE_FALSE(clock.valid);

	// A 32-bit DCLK naturally maps onto the full 33-bit MPEG timestamp domain.
	clock = cdi_dvc::select_video_sync_clock(
		true, 0xffffffffU, false, 0, 0);
	REQUIRE(clock.valid);
	REQUIRE(clock.clock90 == cdi_dvc::MPEG_TIMESTAMP_MASK - 1U);
}
