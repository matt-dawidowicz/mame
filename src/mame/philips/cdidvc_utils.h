// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_UTILS_H
#define MAME_PHILIPS_CDIDVC_UTILS_H

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cdi_dvc
{

constexpr uint64_t MPEG_TIMESTAMP_MODULUS = uint64_t(1) << 33;
constexpr uint64_t MPEG_TIMESTAMP_MASK = MPEG_TIMESTAMP_MODULUS - 1;
constexpr uint32_t MPEG_SYSTEM_CLOCK_HZ = 90'000;
constexpr uint32_t DCLK_HZ = 45'000;

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

struct full_motion_picture_rate
{
	uint32_t numerator;
	uint32_t denominator;
};

// Current scheduler implementation model: when multiple timestamped decoded
// frames are already due at one display opportunity, present the newest due
// frame and consume the older due frames it supersedes.  This helper describes
// emulator queue policy; it is not a claim about an exposed VMPEG FIFO format.
struct presentation_selection
{
	std::size_t selected_index;
	std::size_t consume_count;
	bool valid;
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

constexpr uint64_t mpeg_timestamp_normalize(uint64_t value)
{
	return value & MPEG_TIMESTAMP_MASK;
}

constexpr int64_t mpeg_timestamp_delta(uint64_t lhs, uint64_t rhs)
{
	uint64_t const raw = (lhs - rhs) & MPEG_TIMESTAMP_MASK;
	return (raw & (uint64_t(1) << 32))
		? int64_t(raw) - int64_t(MPEG_TIMESTAMP_MODULUS)
		: int64_t(raw);
}

constexpr int32_t mpeg_dclk_delta(uint64_t timestamp, uint64_t scr)
{
	uint32_t const target45 = uint32_t((timestamp >> 1) & 0xffffffffU);
	uint32_t const scr45 = uint32_t((scr >> 1) & 0xffffffffU);
	return int32_t(target45 - scr45);
}

// Anchor an ISO/IEC 11172 90 kHz system clock to the VMPEG 45 kHz DCLK.
// Unsigned DCLK subtraction intentionally preserves the 32-bit wrap behavior.
constexpr uint64_t mpeg_clock_from_dclk(uint64_t anchor90, uint32_t anchor45, uint32_t current45)
{
	uint32_t const elapsed45 = current45 - anchor45;
	return mpeg_timestamp_normalize(anchor90 + uint64_t(elapsed45) * 2U);
}

constexpr bool mpeg_presentation_due(uint64_t timestamp90, uint64_t clock90, int64_t early_tolerance90 = 0)
{
	return mpeg_timestamp_delta(timestamp90, clock90) <= early_tolerance90;
}

constexpr presentation_selection select_latest_due_presentation(
		uint64_t const *timestamps90, std::size_t count, uint64_t clock90,
		int64_t early_tolerance90 = 0)
{
	presentation_selection result { 0, 0, false };
	if (!timestamps90)
		return result;

	for (std::size_t index = 0; index < count; ++index)
	{
		if (!mpeg_presentation_due(timestamps90[index], clock90, early_tolerance90))
			break;

		result.selected_index = index;
		result.consume_count = index + 1;
		result.valid = true;
	}
	return result;
}

constexpr uint64_t dclk_delay_to_samples(int32_t delta45, uint32_t sample_rate)
{
	if (delta45 <= 0 || sample_rate == 0)
		return 0;

	return (uint64_t(uint32_t(delta45)) * sample_rate + (DCLK_HZ - 1U)) / DCLK_HZ;
}

// ISO/IEC 11172 picture-rate codes permitted by the CD-i Full Motion profile.
// Codes outside the Full Motion set are returned as 0/1 rather than silently
// accepting a rate that the CD-i profile does not require.
constexpr full_motion_picture_rate full_motion_picture_rate_from_code(uint8_t code)
{
	switch (code)
	{
	case 1: return { 24'000, 1'001 }; // 23.976 Hz
	case 2: return { 24, 1 };
	case 3: return { 25, 1 };
	case 4: return { 30'000, 1'001 }; // 29.97 Hz
	case 5: return { 30, 1 };
	default: return { 0, 1 };
	}
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

inline void compact_consumed_audio_samples(std::vector<int16_t> &samples, std::size_t &read)
{
	if (!read)
		return;

	if (read >= samples.size())
	{
		samples.clear();
	}
	else
	{
		samples.erase(samples.begin(), samples.begin() + read);
	}
	read = 0;
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
