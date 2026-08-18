// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_UTILS_H
#define MAME_PHILIPS_CDIDVC_UTILS_H

#pragma once

#include <cstdint>

namespace cdi_dvc
{

struct video_command_effects
{
	uint8_t video_buffer;
	bool scroll;
	bool register_update;
	bool swap_buffer;
	bool video_on;
	bool video_off;
	bool hide;
	bool show_immediate;
	bool show_on_next;
};

struct picture_event_reorder_state
{
	uint16_t reference_interrupts;
	bool reference_valid;
};

struct picture_event_reorder_result
{
	picture_event_reorder_state state;
	uint16_t output_interrupts;
	bool output_valid;
};

enum : uint16_t
{
	FMA_IRQ_END_ISO = 0x0001,
	FMA_IRQ_STREAM_CHANGE = 0x0002,
	FMA_IRQ_FRAME_DECODED = 0x0004,
	FMA_IRQ_UNDERFLOW = 0x0008,
	FMA_IRQ_DECODING_STARTED = 0x0010,
	FMA_IRQ_TIMER = 0x0100
};

enum : uint16_t
{
	FMV_IRQ_SEQUENCE = 0x0001,
	FMV_IRQ_GOP = 0x0002,
	FMV_IRQ_PICTURE = 0x0004,
	FMV_IRQ_END_OF_DATA = 0x0008,
	FMV_IRQ_DCL = 0x0080,
	FMV_IRQ_TIMER = 0x0100,
	FMV_IRQ_END_SEQUENCE = 0x0200,
	FMV_IRQ_END_ISO = 0x0400,
	FMV_IRQ_VSYNC = 0x0800,
	FMV_IRQ_CLIP_UPDATE = 0x2000,
	FMV_IRQ_GEOMETRY_LATCH = FMV_IRQ_DCL | FMV_IRQ_CLIP_UPDATE
};

constexpr int64_t mpeg_timestamp_delta(uint64_t lhs, uint64_t rhs)
{
	constexpr uint64_t mask = (uint64_t(1) << 33) - 1;
	uint64_t const raw = (lhs - rhs) & mask;
	return (raw & (uint64_t(1) << 32))
		? int64_t(raw) - int64_t(uint64_t(1) << 33)
		: int64_t(raw);
}

constexpr int32_t mpeg_dclk_delta(uint64_t timestamp, uint64_t scr)
{
	uint32_t const target45 = uint32_t((timestamp >> 1) & 0xffffffffU);
	uint32_t const scr45 = uint32_t((scr >> 1) & 0xffffffffU);
	return int32_t(target45 - scr45);
}

constexpr bool mpeg_stream_selected(bool for_fma, uint8_t stream_id, uint16_t selected_stream)
{
	if (for_fma)
	{
		return stream_id >= 0xc0 && stream_id <= 0xdf
			&& (stream_id & 0x0f) == (selected_stream & 0x0f);
	}

	return stream_id >= 0xe0 && stream_id <= 0xef
		&& (stream_id & 0x0f) == (selected_stream & 0x0f);
}

constexpr video_command_effects decode_video_command(uint16_t command)
{
	return {
		uint8_t(command & 0x0003),
		bool(command & 0x0004),
		bool(command & 0x0008),
		bool(command & 0x0010),
		bool(command & 0x0020),
		bool(command & 0x0040),
		bool(command & 0x0100),
		bool(command & 0x0200),
		bool(command & 0x0400)
	};
}

constexpr picture_event_reorder_result reorder_picture_events(
		picture_event_reorder_state state, uint8_t picture_type, uint16_t picture_interrupts)
{
	picture_event_reorder_result result { state, 0, false };
	switch (picture_type)
	{
	case 1: // Intra-coded picture.
	case 2: // Predictive-coded picture.
		result.output_interrupts = state.reference_interrupts;
		result.output_valid = state.reference_valid;
		result.state.reference_interrupts = picture_interrupts;
		result.state.reference_valid = true;
		break;

	case 3: // Bidirectionally predictive-coded picture.
		result.output_interrupts = picture_interrupts;
		result.output_valid = true;
		break;

	default:
		break;
	}
	return result;
}

constexpr picture_event_reorder_result flush_picture_events(picture_event_reorder_state state)
{
	return { { 0, false }, state.reference_interrupts, state.reference_valid };
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_UTILS_H
