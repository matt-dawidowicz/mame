// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#ifndef MAME_PHILIPS_CDIDVC_FIDELITY_H
#define MAME_PHILIPS_CDIDVC_FIDELITY_H

#pragma once

#include "cdidvc_utils.h"

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

// Arithmetic-only A/V tolerance.  A PCM sample instant generally cannot land
// exactly on the integer 90 kHz system-clock lattice.  Rounding the rational
// sample position to the nearest 90 kHz tick can therefore differ from an
// independently rounded reference by at most one tick.  Projecting that value
// to the 45 kHz DCLK domain similarly permits one DCLK tick.  These limits are
// derived from clock quantization; they are deliberately *not* a claim about
// human perceptual tolerance or physical VMPEG/CDIC servo margins.
constexpr int64_t AV_ARITHMETIC_TOLERANCE_90 = 1;
constexpr int32_t AV_ARITHMETIC_TOLERANCE_45 = 1;

constexpr bool clock_delta_within_arithmetic_tolerance90(int64_t delta90)
{
	return delta90 >= -AV_ARITHMETIC_TOLERANCE_90
		&& delta90 <= AV_ARITHMETIC_TOLERANCE_90;
}

constexpr bool clock_delta_within_arithmetic_tolerance45(int32_t delta45)
{
	return delta45 >= -AV_ARITHMETIC_TOLERANCE_45
		&& delta45 <= AV_ARITHMETIC_TOLERANCE_45;
}

// Convert a cumulative emitted stereo-frame count into the MPEG 90 kHz clock
// domain without accumulating per-sample rounding error.  The quotient and
// remainder split also avoids multiplying the entire 64-bit frame count by
// 90,000 in one operation.
constexpr uint64_t audio_sample_clock90(
		uint64_t anchor90, uint64_t emitted_frames, uint32_t sample_rate)
{
	if (!sample_rate)
		return mpeg_timestamp_normalize(anchor90);

	uint64_t const whole_seconds = emitted_frames / sample_rate;
	uint64_t const remainder_frames = emitted_frames % sample_rate;
	uint64_t const fractional90 =
		(remainder_frames * MPEG_SYSTEM_CLOCK_HZ + sample_rate / 2U) / sample_rate;
	return mpeg_timestamp_normalize(
		anchor90 + whole_seconds * MPEG_SYSTEM_CLOCK_HZ + fractional90);
}

struct audio_clock_observation
{
	uint64_t sample_clock90;
	int64_t sample_minus_scr90;
	int64_t sample_minus_pts90;
	int32_t sample_minus_dclk45;
};

// Put the emitted-audio sample counter, current SCR-derived 90 kHz clock,
// current audio PTS, and raw 45 kHz DCLK into one observation.  This is
// measurement-only telemetry math; it does not adjust scheduling or audio.
constexpr audio_clock_observation observe_audio_clock(
		uint64_t sample_anchor90,
		uint64_t emitted_frames,
		uint32_t sample_rate,
		uint64_t scr_clock90,
		uint64_t audio_pts90,
		uint32_t dclk45)
{
	uint64_t const sample90 = audio_sample_clock90(
		sample_anchor90, emitted_frames, sample_rate);
	uint32_t const sample45 = uint32_t((sample90 >> 1) & 0xffffffffU);
	return {
		sample90,
		mpeg_timestamp_delta(sample90, scr_clock90),
		mpeg_timestamp_delta(sample90, audio_pts90),
		signed_wrap_delta32(sample45, dclk45)
	};
}

constexpr bool audio_clock_within_arithmetic_tolerance(
		audio_clock_observation const &observation)
{
	return clock_delta_within_arithmetic_tolerance90(
			observation.sample_minus_scr90)
		&& clock_delta_within_arithmetic_tolerance45(
			observation.sample_minus_dclk45);
}

struct packet_schedule_deltas
{
	int64_t play90;
	int64_t decode90;
	int32_t play45;
	int32_t decode45;
};

// Packet presentation/decode timestamps are compared with the live MPEG clock,
// not merely the most recently parsed SCR value.  The caller is responsible for
// advancing its SCR anchor through DCLK before supplying current_clock90.
constexpr packet_schedule_deltas measure_packet_schedule(
		uint64_t play_timestamp90,
		uint64_t decode_timestamp90,
		uint64_t current_clock90)
{
	return {
		mpeg_timestamp_delta(play_timestamp90, current_clock90),
		mpeg_timestamp_delta(decode_timestamp90, current_clock90),
		mpeg_dclk_delta(play_timestamp90, current_clock90),
		mpeg_dclk_delta(decode_timestamp90, current_clock90)
	};
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_FIDELITY_H
