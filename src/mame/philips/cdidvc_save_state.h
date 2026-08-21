// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_SAVE_STATE_H
#define MAME_PHILIPS_CDIDVC_SAVE_STATE_H

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace cdi_dvc
{

// CURRENT IMPLEMENTATION MODEL, NOT HARDWARE SPECIFICATION.
// Fixed save mirrors let MAME serialize dynamic decoder/presentation state
// without pretending PL_MPEG's private pointers are stable save-state data.
constexpr std::size_t SAVE_AUDIO_REPLAY_CAPACITY = 8U * 1024U * 1024U;
constexpr std::size_t SAVE_VIDEO_REPLAY_CAPACITY = 32U * 1024U * 1024U;
constexpr std::size_t SAVE_AUDIO_PCM_VALUES = 1U * 1024U * 1024U;
constexpr std::size_t SAVE_VIDEO_QUEUE_FRAMES = 64U;
constexpr std::size_t SAVE_VIDEO_MAX_WIDTH = 384U;
constexpr std::size_t SAVE_VIDEO_MAX_HEIGHT = 288U;
constexpr std::size_t SAVE_VIDEO_PIXELS_PER_FRAME = SAVE_VIDEO_MAX_WIDTH * SAVE_VIDEO_MAX_HEIGHT;
constexpr std::size_t SAVE_VIDEO_QUEUE_PIXELS = SAVE_VIDEO_QUEUE_FRAMES * SAVE_VIDEO_PIXELS_PER_FRAME;
constexpr std::size_t SAVE_PICTURE_EVENTS = 512U;
constexpr std::size_t SAVE_VIDEO_REPLAY_PUMP_EVENTS = 16U * 1024U;
constexpr uint32_t SAVE_VIDEO_REPLAY_FLUSH_FLAG = 0x80000000U;
constexpr uint32_t SAVE_VIDEO_REPLAY_OFFSET_MASK = 0x7fffffffU;

static_assert(SAVE_VIDEO_REPLAY_CAPACITY <= SAVE_VIDEO_REPLAY_OFFSET_MASK);

constexpr bool save_replay_fits(std::size_t bytes, std::size_t capacity)
{
	return bytes <= capacity;
}

constexpr bool save_audio_pcm_fits(std::size_t values)
{
	return values <= SAVE_AUDIO_PCM_VALUES;
}

constexpr bool save_video_frame_fits(uint16_t width, uint16_t height, std::size_t pixels)
{
	return width <= SAVE_VIDEO_MAX_WIDTH
		&& height <= SAVE_VIDEO_MAX_HEIGHT
		&& pixels <= SAVE_VIDEO_PIXELS_PER_FRAME
		&& pixels == std::size_t(width) * std::size_t(height);
}

constexpr bool save_video_queue_fits(std::size_t frames)
{
	return frames <= SAVE_VIDEO_QUEUE_FRAMES;
}

constexpr bool save_picture_events_fit(std::size_t events)
{
	return events <= SAVE_PICTURE_EVENTS;
}

constexpr bool save_video_replay_pumps_fit(std::size_t events)
{
	return events <= SAVE_VIDEO_REPLAY_PUMP_EVENTS;
}

constexpr uint32_t save_video_replay_pump_event(std::size_t offset, bool flush)
{
	return uint32_t(offset) | (flush ? SAVE_VIDEO_REPLAY_FLUSH_FLAG : 0U);
}

constexpr std::size_t save_video_replay_pump_offset(uint32_t event)
{
	return std::size_t(event & SAVE_VIDEO_REPLAY_OFFSET_MASK);
}

constexpr bool save_video_replay_pump_flush(uint32_t event)
{
	return (event & SAVE_VIDEO_REPLAY_FLUSH_FLAG) != 0;
}

constexpr bool save_video_replay_pump_offset_valid(
		std::size_t previous, std::size_t current, std::size_t replay_bytes)
{
	return previous <= current && current <= replay_bytes;
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_SAVE_STATE_H
