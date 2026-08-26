// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#ifndef MAME_PHILIPS_CDIDVC_FIDELITY_H
#define MAME_PHILIPS_CDIDVC_FIDELITY_H

#pragma once

#include <cstdint>

namespace cdi_dvc
{

// CURRENT IMPLEMENTATION MODEL, NOT HARDWARE SPECIFICATION.
//
// CD-i DVC video register coordinates and decoded MPEG pixels currently pass
// through different presentation scales before composition with the MCD212
// output. Keep those scales explicit and testable while their physical
// clock-domain provenance remains unknown.
constexpr unsigned VIDEO_REGISTER_X_SCALE = 4;
constexpr unsigned VIDEO_REGISTER_Y_SCALE = 2;
constexpr unsigned VIDEO_PIXEL_X_SCALE = 2;
constexpr unsigned VIDEO_PIXEL_Y_SCALE = 2;

struct video_present_geometry
{
	int dst_x;
	int dst_y;
	unsigned output_width;
	unsigned output_height;
};

constexpr video_present_geometry current_video_present_geometry(
		uint16_t screen_x, uint16_t screen_y, int visible_top,
		unsigned source_width, unsigned source_height)
{
	return {
		int(screen_x) * int(VIDEO_REGISTER_X_SCALE),
		visible_top + int(screen_y) * int(VIDEO_REGISTER_Y_SCALE),
		source_width * VIDEO_PIXEL_X_SCALE,
		source_height * VIDEO_PIXEL_Y_SCALE
	};
}

// Measurement helper for long-session A/V telemetry. Positive values mean the
// observed stream is ahead of the reference clock; negative values mean it is
// behind. This is diagnostic math, not a servo policy.
constexpr int64_t clock90_delta_microseconds(int64_t delta90)
{
	return (delta90 * 1'000'000LL) / 90'000LL;
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_FIDELITY_H
