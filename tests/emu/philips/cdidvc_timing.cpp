// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "cdi_video_timing.h"
#include "cdidvc_utils.h"

TEST_CASE("CD-i DVC anchored MPEG clock advances from 45 kHz DCLK", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(0, 0, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(1'000, 100, 100) == 1'000);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(1'000, 100, 101) == 1'002);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(1'000, 100, 145) == 1'090);

	constexpr uint64_t wrap = cdi_dvc::MPEG_TIMESTAMP_MODULUS;
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(wrap - 2, 0, 1) == 0);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(wrap - 1, 0, 1) == 1);

	// DCLK is a 32-bit clock.  Unsigned subtraction must preserve elapsed
	// ticks when the anchor is immediately before a wrap.
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(10, 0xffffffffU, 0) == 12);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(10, 0xfffffffeU, 1) == 16);
}

TEST_CASE("CD-i DVC presentation due comparison follows 33-bit timestamp ordering", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_presentation_due(100, 100));
	REQUIRE(cdi_dvc::mpeg_presentation_due(99, 100));
	REQUIRE_FALSE(cdi_dvc::mpeg_presentation_due(101, 100));
	REQUIRE(cdi_dvc::mpeg_presentation_due(101, 100, 1));
	REQUIRE_FALSE(cdi_dvc::mpeg_presentation_due(102, 100, 1));

	constexpr uint64_t mask = cdi_dvc::MPEG_TIMESTAMP_MASK;
	REQUIRE_FALSE(cdi_dvc::mpeg_presentation_due(1, mask));
	REQUIRE(cdi_dvc::mpeg_presentation_due(1, mask, 2));
	REQUIRE(cdi_dvc::mpeg_presentation_due(mask, 1));
}

TEST_CASE("CD-i DVC presentation selection chooses the newest timestamp already due", "[emu][philips][dvc]")
{
	uint64_t const ordered[] = { 90, 100, 110 };

	auto selected = cdi_dvc::select_latest_due_presentation(ordered, 3, 100);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 1);
	REQUIRE(selected.consume_count == 2);

	selected = cdi_dvc::select_latest_due_presentation(ordered, 3, 89);
	REQUIRE_FALSE(selected.valid);
	REQUIRE(selected.consume_count == 0);

	selected = cdi_dvc::select_latest_due_presentation(ordered, 3, 200);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 2);
	REQUIRE(selected.consume_count == 3);

	selected = cdi_dvc::select_latest_due_presentation(nullptr, 3, 100);
	REQUIRE_FALSE(selected.valid);

	uint64_t const early[] = { 100, 102, 104 };
	selected = cdi_dvc::select_latest_due_presentation(early, 3, 100, 2);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 1);
	REQUIRE(selected.consume_count == 2);

	constexpr uint64_t mask = cdi_dvc::MPEG_TIMESTAMP_MASK;
	uint64_t const wrapped[] = { mask - 1, 1, 3 };
	selected = cdi_dvc::select_latest_due_presentation(wrapped, 3, 1);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 1);
	REQUIRE(selected.consume_count == 2);
}

TEST_CASE("CD-i DVC DCLK delays convert to output samples with ceiling semantics", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::dclk_delay_to_samples(-1, 48'000) == 0);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(0, 48'000) == 0);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(1, 0) == 0);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(45'000, 48'000) == 48'000);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(45'000, 44'100) == 44'100);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(1, 48'000) == 2);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(15, 48'000) == 16);
}

TEST_CASE("CD-i DVC Full Motion picture-rate codes expose exact rational rates", "[emu][philips][dvc]")
{
	auto rate = cdi_dvc::full_motion_picture_rate_from_code(1);
	REQUIRE(rate.numerator == 24'000);
	REQUIRE(rate.denominator == 1'001);

	rate = cdi_dvc::full_motion_picture_rate_from_code(2);
	REQUIRE(rate.numerator == 24);
	REQUIRE(rate.denominator == 1);

	rate = cdi_dvc::full_motion_picture_rate_from_code(3);
	REQUIRE(rate.numerator == 25);
	REQUIRE(rate.denominator == 1);

	rate = cdi_dvc::full_motion_picture_rate_from_code(4);
	REQUIRE(rate.numerator == 30'000);
	REQUIRE(rate.denominator == 1'001);

	rate = cdi_dvc::full_motion_picture_rate_from_code(5);
	REQUIRE(rate.numerator == 30);
	REQUIRE(rate.denominator == 1);

	for (uint8_t code : { uint8_t(0), uint8_t(6), uint8_t(7), uint8_t(8), uint8_t(15) })
	{
		rate = cdi_dvc::full_motion_picture_rate_from_code(code);
		REQUIRE(rate.numerator == 0);
		REQUIRE(rate.denominator == 1);
	}
}

TEST_CASE("CD-i MCD212 documented standard profiles remain distinct", "[emu][philips][cdi][video]")
{
	using cdi_video::standard;

	auto const ntsc_monitor = cdi_video::profile(standard::ntsc_monitor);
	REQUIRE(ntsc_monitor.crystal_hz == 28'000'000);
	REQUIRE(ntsc_monitor.normal_width == 360);
	REQUIRE(ntsc_monitor.normal_height == 240);
	REQUIRE(ntsc_monitor.noninterlace_total_lines == 262);
	REQUIRE(ntsc_monitor.interlace_total_half_lines == 525);
	REQUIRE(ntsc_monitor.horizontal_total_cycles == 112);
	REQUIRE(ntsc_monitor.horizontal_active_cycles == 90);
	REQUIRE_FALSE(ntsc_monitor.cf);
	REQUIRE(ntsc_monitor.fd);

	auto const ntsc_tv = cdi_video::profile(standard::ntsc_tv);
	REQUIRE(ntsc_tv.crystal_hz == 30'209'700);
	REQUIRE(ntsc_tv.normal_width == 384);
	REQUIRE(ntsc_tv.normal_height == 240);
	REQUIRE(ntsc_tv.noninterlace_total_lines == 262);
	REQUIRE(ntsc_tv.interlace_total_half_lines == 525);
	REQUIRE(ntsc_tv.horizontal_total_cycles == 120);
	REQUIRE(ntsc_tv.horizontal_active_cycles == 96);
	REQUIRE(ntsc_tv.cf);
	REQUIRE(ntsc_tv.fd);

	auto const pal_tv = cdi_video::profile(standard::pal_tv);
	REQUIRE(pal_tv.crystal_hz == 30'000'000);
	REQUIRE(pal_tv.normal_width == 384);
	REQUIRE(pal_tv.normal_height == 280);
	REQUIRE(pal_tv.noninterlace_total_lines == 312);
	REQUIRE(pal_tv.interlace_total_half_lines == 625);
	REQUIRE(pal_tv.horizontal_total_cycles == 120);
	REQUIRE(pal_tv.horizontal_active_cycles == 96);
	REQUIRE(pal_tv.cf);
	REQUIRE_FALSE(pal_tv.fd);
}

TEST_CASE("CD-i system clocks remain separate from generic MCD212 timing", "[emu][philips][cdi][video]")
{
	using cdi_video::standard;

	REQUIRE(cdi_video::cdi_system_clock_hz(standard::pal_tv) == 30'000'000);
	REQUIRE(cdi_video::cdi_system_clock_hz(standard::ntsc_tv) == 30'209'800);
	REQUIRE(cdi_video::cdi_system_clock_hz(standard::ntsc_monitor) == 28'000'000);

	// Motorola's generic MCD212 NTSC-TV nominal and the Philips CD-i service
	// value are intentionally represented independently rather than rounded
	// into one unexplained constant.
	REQUIRE(cdi_video::profile(standard::ntsc_tv).crystal_hz == 30'209'700);
	REQUIRE(cdi_video::cdi_system_clock_hz(standard::ntsc_tv)
			!= cdi_video::profile(standard::ntsc_tv).crystal_hz);
}

TEST_CASE("CD-i MCD212 FD and ST bits produce documented resolutions", "[emu][philips][cdi][video]")
{
	// FD selects 50/60 Hz timing.  ST is a compatibility/resolution modifier,
	// not the machine television-standard selector.
	REQUIRE(cdi_video::noninterlace_total_lines(false) == 312);
	REQUIRE(cdi_video::noninterlace_total_lines(true) == 262);
	REQUIRE(cdi_video::interlace_total_half_lines(false) == 625);
	REQUIRE(cdi_video::interlace_total_half_lines(true) == 525);

	REQUIRE(cdi_video::noninterlace_active_lines(false, false) == 280);
	REQUIRE(cdi_video::noninterlace_active_lines(false, true) == 240);
	REQUIRE(cdi_video::noninterlace_active_lines(true, false) == 240);
	REQUIRE(cdi_video::noninterlace_active_lines(true, true) == 240);

	REQUIRE(cdi_video::horizontal_normal_pixels(false, false) == 360);
	REQUIRE(cdi_video::horizontal_normal_pixels(false, true) == 360);
	REQUIRE(cdi_video::horizontal_normal_pixels(true, false) == 384);
	REQUIRE(cdi_video::horizontal_normal_pixels(true, true) == 360);

	REQUIRE(cdi_video::horizontal_total_cycles(false) == 112);
	REQUIRE(cdi_video::horizontal_total_cycles(true) == 120);
	REQUIRE(cdi_video::horizontal_active_cycles(false, false) == 90);
	REQUIRE(cdi_video::horizontal_active_cycles(true, false) == 96);
	REQUIRE(cdi_video::horizontal_active_cycles(true, true) == 90);
}

TEST_CASE("CD-i control-driven half-line timing follows CF FD and ST", "[emu][philips][cdi][video]")
{
	auto const pal = cdi_video::controlled_halfline_screen_profile(true, false, false, 30'000'000);
	REQUIRE(pal.pixel_clock_hz == 30'000'000);
	REQUIRE(pal.htotal == 960);
	REQUIRE(pal.hvis_end == 768);
	REQUIRE(pal.vtotal == 625);
	REQUIRE(pal.vvis_start == 64);
	REQUIRE(pal.vvis_end == 624);

	auto const ntsc = cdi_video::controlled_halfline_screen_profile(true, true, false, 30'209'800);
	REQUIRE(ntsc.pixel_clock_hz == 30'209'800);
	REQUIRE(ntsc.htotal == 960);
	REQUIRE(ntsc.hvis_end == 768);
	REQUIRE(ntsc.vtotal == 525);
	REQUIRE(ntsc.vvis_start == 44);
	REQUIRE(ntsc.vvis_end == 524);

	auto const pal_st = cdi_video::controlled_halfline_screen_profile(true, false, true, 30'000'000);
	REQUIRE(pal_st.htotal == 960);
	REQUIRE(pal_st.hvis_end == 720);
	REQUIRE(pal_st.vtotal == 625);
	REQUIRE(pal_st.vvis_start == 144);
	REQUIRE(pal_st.vvis_end == 624);

	auto const monitor = cdi_video::controlled_halfline_screen_profile(false, true, false, 28'000'000);
	REQUIRE(monitor.htotal == 896);
	REQUIRE(monitor.hvis_end == 720);
	REQUIRE(monitor.vtotal == 525);
	REQUIRE(monitor.vvis_start == 44);
	REQUIRE(monitor.vvis_end == 524);
}

TEST_CASE("CD-i exact half-line screen model uses documented crystals and totals", "[emu][philips][cdi][video]")
{
	using cdi_video::standard;

	auto const pal = cdi_video::exact_halfline_screen_profile(standard::pal_tv);
	REQUIRE(pal.pixel_clock_hz == 30'000'000);
	REQUIRE(pal.htotal == 960);
	REQUIRE(pal.hvis_start == 0);
	REQUIRE(pal.hvis_end == 768);
	REQUIRE(pal.vtotal == 625);
	REQUIRE(pal.vvis_start == 64);
	REQUIRE(pal.vvis_end == 624);

	auto const ntsc = cdi_video::exact_halfline_screen_profile(standard::ntsc_tv);
	REQUIRE(ntsc.pixel_clock_hz == 30'209'700);
	REQUIRE(ntsc.htotal == 960);
	REQUIRE(ntsc.hvis_start == 0);
	REQUIRE(ntsc.hvis_end == 768);
	REQUIRE(ntsc.vtotal == 525);
	REQUIRE(ntsc.vvis_start == 44);
	REQUIRE(ntsc.vvis_end == 524);

	auto const monitor = cdi_video::exact_halfline_screen_profile(standard::ntsc_monitor);
	REQUIRE(monitor.pixel_clock_hz == 28'000'000);
	REQUIRE(monitor.htotal == 896);
	REQUIRE(monitor.hvis_end == 720);
	REQUIRE(monitor.vtotal == 525);
	REQUIRE(monitor.vvis_start == 44);
	REQUIRE(monitor.vvis_end == 524);
}

TEST_CASE("Philips CD-i half-line screen model uses player system clocks", "[emu][philips][cdi][video]")
{
	using cdi_video::standard;

	auto const pal = cdi_video::cdi_halfline_screen_profile(standard::pal_tv);
	REQUIRE(pal.pixel_clock_hz == 30'000'000);
	REQUIRE(pal.htotal == 960);
	REQUIRE(pal.vtotal == 625);
	REQUIRE(pal.vvis_start == 64);
	REQUIRE(pal.vvis_end == 624);

	auto const ntsc = cdi_video::cdi_halfline_screen_profile(standard::ntsc_tv);
	REQUIRE(ntsc.pixel_clock_hz == 30'209'800);
	REQUIRE(ntsc.htotal == 960);
	REQUIRE(ntsc.vtotal == 525);
	REQUIRE(ntsc.vvis_start == 44);
	REQUIRE(ntsc.vvis_end == 524);
}

TEST_CASE("CD-i legacy scaled screen model reproduces the validated PAL checkpoint", "[emu][philips][cdi][video]")
{
	using cdi_video::standard;

	auto const pal = cdi_video::legacy_scaled_screen_profile(standard::pal_tv);
	REQUIRE(pal.pixel_clock_hz == 29'952'000);
	REQUIRE(pal.htotal == 960);
	REQUIRE(pal.hvis_start == 0);
	REQUIRE(pal.hvis_end == 768);
	REQUIRE(pal.vtotal == 624);
	REQUIRE(pal.vvis_start == 64);
	REQUIRE(pal.vvis_end == 624);

	auto const ntsc = cdi_video::legacy_scaled_screen_profile(standard::ntsc_tv);
	REQUIRE(ntsc.pixel_clock_hz == 30'152'158);
	REQUIRE(ntsc.htotal == 960);
	REQUIRE(ntsc.hvis_end == 768);
	REQUIRE(ntsc.vtotal == 524);
	REQUIRE(ntsc.vvis_start == 44);
	REQUIRE(ntsc.vvis_end == 524);
}
