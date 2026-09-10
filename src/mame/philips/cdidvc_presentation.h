// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_PRESENTATION_H
#define MAME_PHILIPS_CDIDVC_PRESENTATION_H

#pragma once

#include "cdidvc_utils.h"

#include <cstdint>

namespace cdi_dvc
{

// VMPEG's Green Book MPEG sample cadence is 15 MHz while White Book/VCD
// cartridges use an on-board sample-rate converter to 13.5 MHz.  MAME's CD-i
// compositor represents the Green Book cadence as two destination pixels per
// decoded MPEG sample, so the corresponding White Book scale is 2 * 15/13.5
// = 20/9 destination pixels per decoded sample.
constexpr uint32_t DVC_GREEN_X_SCALE_NUM = 2;
constexpr uint32_t DVC_GREEN_X_SCALE_DEN = 1;
constexpr uint32_t DVC_VCD_X_SCALE_NUM = 20;
constexpr uint32_t DVC_VCD_X_SCALE_DEN = 9;

constexpr bool vcd_pixel_clock_enabled(uint16_t vcd_control)
{
	return (vcd_control & 0x0001U) != 0;
}

constexpr unsigned video_output_width(unsigned source_width, bool vcd_mode)
{
	if (!source_width)
		return 0;

	uint64_t const numerator = uint64_t(source_width)
		* (vcd_mode ? DVC_VCD_X_SCALE_NUM : DVC_GREEN_X_SCALE_NUM);
	uint32_t const denominator = vcd_mode ? DVC_VCD_X_SCALE_DEN : DVC_GREEN_X_SCALE_DEN;
	return unsigned((numerator + denominator / 2U) / denominator);
}

// Map destination pixel centres back to decoded MPEG sample centres.  This is
// exact 2x replication in Green Book mode and nearest-centre resampling for the
// fractional 20/9 White Book/VCD cadence.  The latter preserves source detail
// without pretending that PL_MPEG's decoded 4:2:0 samples have extra resolution.
constexpr unsigned video_source_x_for_output(
		unsigned output_x, unsigned source_width, bool vcd_mode)
{
	if (!source_width)
		return 0;

	uint32_t const scale_num = vcd_mode ? DVC_VCD_X_SCALE_NUM : DVC_GREEN_X_SCALE_NUM;
	uint32_t const scale_den = vcd_mode ? DVC_VCD_X_SCALE_DEN : DVC_GREEN_X_SCALE_DEN;
	uint64_t const source =
		(uint64_t(output_x * 2U + 1U) * scale_den) / (uint64_t(scale_num) * 2U);
	return unsigned(source < source_width ? source : source_width - 1U);
}

struct video_sync_clock
{
	uint64_t clock90;
	bool valid;
	bool external_dclk;
};

// GEN_SYSCR is the VMPEG display clock.  Once guest software has explicitly
// programmed it, synchronous display is driven directly by that 45 kHz DCLK,
// matching the external-clock path that ties video timing to the FMA/audio
// clock.  Before GEN_SYSCR has been established, retain the stream-SCR anchor
// used by the existing fallback model so reset/stand-alone streams still have
// a usable presentation clock.
constexpr video_sync_clock select_video_sync_clock(
		bool syscr_programmed,
		uint32_t current_fmv_dclk45,
		bool have_stream_scr,
		uint64_t stream_scr90,
		uint32_t stream_scr_anchor45)
{
	if (syscr_programmed)
	{
		return {
			mpeg_timestamp_normalize(uint64_t(current_fmv_dclk45) * 2U),
			true,
			true
		};
	}

	if (have_stream_scr)
	{
		return {
			mpeg_clock_from_dclk(stream_scr90, stream_scr_anchor45, current_fmv_dclk45),
			true,
			false
		};
	}

	return { 0, false, false };
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_PRESENTATION_H
