// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_FIDELITY_H
#define MAME_PHILIPS_CDIDVC_FIDELITY_H

#pragma once

#include <cstddef>
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

// CURRENT IMPLEMENTATION MODEL, NOT HARDWARE SPECIFICATION.
//
// Break a host DMA transfer into bounded service slices. The caller owns the
// timer/cadence policy; this helper only enforces conservation and prevents a
// service callback from consuming more words than remain.
struct dma_service_slice
{
	uint16_t words;
	uint16_t remaining_after;
	bool complete;
};

constexpr dma_service_slice bounded_dma_service(uint16_t remaining, uint16_t budget)
{
	if (!remaining || !budget)
		return { 0, remaining, remaining == 0 };

	uint16_t const words = remaining < budget ? remaining : budget;
	uint16_t const after = uint16_t(remaining - words);
	return { words, after, after == 0 };
}

// Measurement helper for long-session A/V telemetry. Positive values mean the
// observed stream is ahead of the reference clock; negative values mean it is
// behind. This is diagnostic math, not a servo policy.
constexpr int64_t clock90_delta_microseconds(int64_t delta90)
{
	return (delta90 * 1'000'000LL) / 90'000LL;
}

constexpr int64_t sample_delta_microseconds(int64_t samples, uint32_t sample_rate)
{
	return sample_rate ? (samples * 1'000'000LL) / int64_t(sample_rate) : 0;
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_FIDELITY_H
