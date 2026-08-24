// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_MCD212_VIDEO_H
#define MAME_PHILIPS_MCD212_VIDEO_H

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace mcd212_video
{

struct timing_profile
{
	int horizontal_total;
	int total_lines;
	int total_half_lines;
	int active_start_lines;
	int active_lines;
	int blank_lines;
	bool interlace;

	constexpr int active_end_lines() const
	{
		return active_start_lines + active_lines;
	}

	// MCD212 table 5-7: PA=1 (odd) begins on a whole line and PA=0
	// (even) begins one half-line later.
	constexpr int field_halfline_offset(bool odd_field) const
	{
		return interlace && !odd_field ? 1 : 0;
	}
};

constexpr timing_profile make_timing_profile(
		bool crystal_frequency,
		bool sixty_hz,
		bool interlace,
		bool standard)
{
	const int total_lines = sixty_hz ? 262 : 312;
	const int active_lines = sixty_hz || standard ? 240 : 280;
	const int active_start_lines = sixty_hz ? 18 : standard ? 46 : 26;

	return
	{
		crystal_frequency ? 960 : 896,
		total_lines,
		total_lines * 2 + (interlace ? 1 : 0),
		active_start_lines,
		active_lines,
		total_lines - active_lines,
		interlace
	};
}

constexpr uint32_t ica_pointer_word_offset(bool odd_field)
{
	return odd_field ? 0x200 : 0x202;
}

constexpr uint32_t dca_bytes_per_line(bool crystal_frequency)
{
	return crystal_frequency ? 64 : 32;
}

constexpr bool interrupt_line_asserted(
		uint8_t status,
		bool disable_interrupt_1,
		bool disable_interrupt_2)
{
	return
		((status & 0x04) && !disable_interrupt_1) ||
		((status & 0x02) && !disable_interrupt_2);
}

constexpr bool external_video_eligible(
		bool enabled,
		bool transparent_a,
		bool transparent_b,
		bool cursor_present = false)
{
	return enabled && transparent_a && transparent_b && !cursor_present;
}

constexpr uint32_t pack_yuv(uint8_t y, uint8_t u, uint8_t v)
{
	return (uint32_t(y) << 16) | (uint32_t(u) << 8) | uint32_t(v);
}

constexpr uint8_t yuv_y(uint32_t yuv) { return uint8_t(yuv >> 16); }
constexpr uint8_t yuv_u(uint32_t yuv) { return uint8_t(yuv >> 8); }
constexpr uint8_t yuv_v(uint32_t yuv) { return uint8_t(yuv); }

constexpr uint8_t average2(uint8_t a, uint8_t b)
{
	return uint8_t((unsigned(a) + unsigned(b)) / 2);
}

constexpr uint8_t average4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return uint8_t((unsigned(a) + unsigned(b) + unsigned(c) + unsigned(d)) / 4);
}

constexpr uint32_t interpolate_yuv(
		uint32_t p00,
		uint32_t p10,
		uint32_t p01,
		uint32_t p11,
		bool horizontal_half,
		bool vertical_half)
{
	if (horizontal_half && vertical_half)
	{
		return pack_yuv(
			average4(yuv_y(p00), yuv_y(p10), yuv_y(p01), yuv_y(p11)),
			average4(yuv_u(p00), yuv_u(p10), yuv_u(p01), yuv_u(p11)),
			average4(yuv_v(p00), yuv_v(p10), yuv_v(p01), yuv_v(p11)));
	}
	if (horizontal_half)
	{
		return pack_yuv(
			average2(yuv_y(p00), yuv_y(p10)),
			average2(yuv_u(p00), yuv_u(p10)),
			average2(yuv_v(p00), yuv_v(p10)));
	}
	if (vertical_half)
	{
		return pack_yuv(
			average2(yuv_y(p00), yuv_y(p01)),
			average2(yuv_u(p00), yuv_u(p01)),
			average2(yuv_v(p00), yuv_v(p01)));
	}
	return p00;
}

constexpr int qhy_delta(uint8_t level)
{
	return (int(level) - 128) * 2;
}

constexpr uint8_t limit_component(int component)
{
	return uint8_t(component < 0 ? 0 : component > 255 ? 255 : component);
}

constexpr uint32_t add_qhy_level(uint32_t rgb, uint32_t level)
{
	return
		(uint32_t(limit_component(int((rgb >> 16) & 0xff) + qhy_delta(uint8_t(level >> 16)))) << 16) |
		(uint32_t(limit_component(int((rgb >> 8) & 0xff) + qhy_delta(uint8_t(level >> 8)))) << 8) |
		uint32_t(limit_component(int(rgb & 0xff) + qhy_delta(uint8_t(level))));
}

struct qhy_token
{
	uint8_t first_code;
	uint8_t second_code;
	uint8_t pair_count;
	uint8_t byte_count;
	bool to_end_of_line;
	bool valid;
};

constexpr qhy_token decode_qhy_token(uint8_t first, uint8_t second = 0)
{
	const bool run = bool(first & 0x80);
	return
	{
		uint8_t((first >> 4) & 7),
		uint8_t(first & 7),
		run ? second : uint8_t(1),
		run ? uint8_t(2) : uint8_t(1),
		run && !second,
		run ? !(first & 0x08) && second != 1 : bool(first & 0x08)
	};
}

struct qhy_decode_result
{
	std::size_t bytes;
	std::size_t pixels;
	bool end_of_line;
	bool valid;
};

template <typename Reader>
qhy_decode_result decode_qhy_line(
		Reader reader,
		std::size_t max_bytes,
		uint8_t *codes,
		std::size_t width)
{
	qhy_decode_result result{ 0, 0, false, true };
	if (!codes || !width || (width & 1))
		return { 0, 0, false, false };

	while (result.pixels < width && result.bytes < max_bytes)
	{
		const uint8_t first = reader(result.bytes++);
		uint8_t second = 0;
		if (first & 0x80)
		{
			if (result.bytes >= max_bytes)
			{
				result.valid = false;
				break;
			}
			second = reader(result.bytes++);
		}

		const qhy_token token = decode_qhy_token(first, second);
		result.valid = result.valid && token.valid;

		const std::size_t remaining_pairs = (width - result.pixels) / 2;
		std::size_t pairs = token.to_end_of_line ? remaining_pairs : token.pair_count;
		if (token.to_end_of_line && remaining_pairs < 2)
			result.valid = false;
		if (pairs > remaining_pairs)
		{
			pairs = remaining_pairs;
			result.valid = false;
		}

		for (std::size_t pair = 0; pair < pairs; ++pair)
		{
			codes[result.pixels++] = token.first_code;
			codes[result.pixels++] = token.second_code;
		}

		if (token.to_end_of_line)
		{
			result.end_of_line = true;
			break;
		}
	}

	result.valid = result.valid && result.end_of_line && result.pixels == width;
	return result;
}

} // namespace mcd212_video

#endif // MAME_PHILIPS_MCD212_VIDEO_H
