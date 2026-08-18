// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_UTILS_H
#define MAME_PHILIPS_CDIDVC_UTILS_H

#pragma once

#include <cstdint>

namespace cdi_dvc
{

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

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_UTILS_H
