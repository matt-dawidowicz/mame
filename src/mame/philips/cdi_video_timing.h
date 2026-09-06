// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDI_VIDEO_TIMING_H
#define MAME_PHILIPS_CDI_VIDEO_TIMING_H

#pragma once

#include <cstdint>

namespace cdi_video
{

// Motorola MCD212/D Rev. 0, sections 5.1-5.6 and 9.1.3.
// These are documented VDSC timing/profile values, not emulator heuristics.
enum class standard : uint8_t
{
	ntsc_monitor,
	ntsc_tv,
	pal_tv
};

struct timing_profile
{
	uint32_t crystal_hz;
	uint16_t normal_width;
	uint16_t normal_height;
	uint16_t noninterlace_total_lines;
	uint16_t interlace_total_half_lines;
	uint8_t horizontal_total_cycles;
	uint8_t horizontal_active_cycles;
	bool cf;
	bool fd;
};

constexpr timing_profile profile(standard video_standard)
{
	switch (video_standard)
	{
	case standard::ntsc_monitor:
		// 28 MHz NTSC monitor mode: 360x240 normal display.
		return { 28'000'000, 360, 240, 262, 525, 112, 90, false, true };

	case standard::ntsc_tv:
		// MCD212 documentation gives 30.2097 MHz for generic NTSC-TV timing.
		return { 30'209'700, 384, 240, 262, 525, 120, 96, true, true };

	case standard::pal_tv:
	default:
		// 30 MHz PAL TV mode: 384x280 normal display.
		return { 30'000'000, 384, 280, 312, 625, 120, 96, true, false };
	}
}

// Philips CD-i service documentation specifies a common system clock delivered
// to both the SCC68070 and VDSC: 30.0000 MHz PAL and 30.2098 MHz NTSC.  Keep
// that player-level value separate from the generic MCD212 nominal above.
constexpr uint32_t cdi_system_clock_hz(standard video_standard)
{
	switch (video_standard)
	{
	case standard::ntsc_tv:
		return 30'209'800;
	case standard::pal_tv:
		return 30'000'000;
	case standard::ntsc_monitor:
	default:
		return profile(video_standard).crystal_hz;
	}
}

constexpr uint16_t noninterlace_active_lines(bool fd, bool st)
{
	// MCD212 Table 5-4.  ST only changes vertical resolution in 50 Hz mode.
	return fd ? 240 : (st ? 240 : 280);
}

constexpr uint16_t noninterlace_total_lines(bool fd)
{
	// MCD212 Table 5-6.
	return fd ? 262 : 312;
}

constexpr uint16_t interlace_total_half_lines(bool fd)
{
	// MCD212 Table 5-7 expresses a field as 262.5/312.5 lines.
	// Store half-lines so the value remains exact in integer arithmetic.
	return fd ? 525 : 625;
}

constexpr uint16_t horizontal_normal_pixels(bool cf, bool st)
{
	// MCD212 Table 5-2.  In 30/30.2097 MHz modes ST masks 12 pixels on
	// either side, reducing the normal 384-pixel TV display to 360 pixels.
	return cf ? (st ? 360 : 384) : 360;
}

constexpr uint8_t horizontal_total_cycles(bool cf)
{
	// MCD212 Table 5-5: 112 cycles at 28 MHz, 120 cycles at 30/30.2097 MHz.
	return cf ? 120 : 112;
}

constexpr uint8_t horizontal_active_cycles(bool cf, bool st)
{
	// MCD212 Table 5-5: 90 cycles at 28 MHz, 96 cycles for CF=1/ST=0,
	// and 90 cycles for CF=1/ST=1.
	return cf ? (st ? 90 : 96) : 90;
}

struct mame_screen_timing
{
	uint32_t pixel_clock_hz;
	uint16_t htotal;
	uint16_t hvis_start;
	uint16_t hvis_end;
	uint16_t vtotal;
	uint16_t vvis_start;
	uint16_t vvis_end;
};

// Build the exact half-line representation directly from documented MCD212
// display-control state.  This is intended as the pure timing primitive for a
// future software-driven DCR1 reconfiguration path; it does not itself change
// a MAME screen or infer a player television standard.
constexpr mame_screen_timing controlled_halfline_screen_profile(
		bool cf, bool fd, bool st, uint32_t pixel_clock_hz)
{
	uint16_t const active_half_lines = uint16_t(noninterlace_active_lines(fd, st) * 2U);
	uint16_t const total_half_lines = interlace_total_half_lines(fd);
	uint16_t const htotal = uint16_t(horizontal_total_cycles(cf) * 8U);
	uint16_t const hactive = uint16_t(horizontal_active_cycles(cf, st) * 8U);
	uint16_t const visible_start = uint16_t((total_half_lines - 1U) - active_half_lines);

	return {
		pixel_clock_hz,
		htotal,
		0,
		hactive,
		total_half_lines,
		visible_start,
		uint16_t(visible_start + active_half_lines)
	};
}

constexpr mame_screen_timing halfline_screen_profile(
		standard video_standard, uint32_t pixel_clock_hz)
{
	timing_profile const hardware = profile(video_standard);
	return controlled_halfline_screen_profile(
			hardware.cf, hardware.fd, false, pixel_clock_hz);
}

// Generic MCD212 half-line timing using the controller documentation's
// oscillator value.
constexpr mame_screen_timing exact_halfline_screen_profile(standard video_standard)
{
	return halfline_screen_profile(video_standard, profile(video_standard).crystal_hz);
}

// Philips CD-i machine timing using the player service-documentation clock.
constexpr mame_screen_timing cdi_halfline_screen_profile(standard video_standard)
{
	return halfline_screen_profile(video_standard, cdi_system_clock_hz(video_standard));
}

constexpr uint32_t rounded_ratio(uint64_t numerator, uint32_t denominator)
{
	return denominator
		? uint32_t((numerator + uint64_t(denominator / 2U)) / denominator)
		: 0;
}

// Legacy MAME compatibility representation used by the validated PAL
// checkpoint before the fidelity-polish branch.
//
// IMPLEMENTATION MODEL, NOT HARDWARE SPECIFICATION:
// the final half-line is omitted and the crystal is scaled by (N-1)/N so the
// field period remains correct.  Keep this helper for regression comparison
// while migrating the driver to exact half-line timing.
constexpr mame_screen_timing legacy_scaled_screen_profile(standard video_standard)
{
	timing_profile const hardware = profile(video_standard);
	uint16_t const represented_half_lines = uint16_t(hardware.interlace_total_half_lines - 1U);
	uint16_t const active_half_lines = uint16_t(hardware.normal_height * 2U);
	uint16_t const htotal = uint16_t(hardware.horizontal_total_cycles * 8U);
	uint16_t const hactive = uint16_t(hardware.horizontal_active_cycles * 8U);

	return {
		rounded_ratio(uint64_t(hardware.crystal_hz) * represented_half_lines,
			hardware.interlace_total_half_lines),
		htotal,
		0,
		hactive,
		represented_half_lines,
		uint16_t(represented_half_lines - active_half_lines),
		represented_half_lines
	};
}

} // namespace cdi_video

#endif // MAME_PHILIPS_CDI_VIDEO_TIMING_H
