// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_MPEG_FORMAT_H
#define MAME_PHILIPS_CDIDVC_MPEG_FORMAT_H

#pragma once

#include <cstdint>

namespace cdi_dvc
{

// The first non-stuffing MPEG-1 PES header byte selects one of these parser
// paths. This describes ISO/IEC 11172 stream syntax consumed by the emulator;
// it is not a VMPEG register/hardware-semantic claim.
enum class mpeg1_pes_header_kind : uint8_t
{
	stuffing,
	std_buffer,
	pts,
	pts_dts,
	no_timestamp,
	payload_fallback
};

constexpr mpeg1_pes_header_kind classify_mpeg1_pes_header_byte(uint8_t data)
{
	if (data == 0xff)
		return mpeg1_pes_header_kind::stuffing;
	if ((data & 0xc0) == 0x40)
		return mpeg1_pes_header_kind::std_buffer;
	if ((data & 0xf0) == 0x20)
		return mpeg1_pes_header_kind::pts;
	if ((data & 0xf0) == 0x30)
		return mpeg1_pes_header_kind::pts_dts;
	if (data == 0x0f)
		return mpeg1_pes_header_kind::no_timestamp;
	return mpeg1_pes_header_kind::payload_fallback;
}

constexpr bool mpeg1_pes_header_can_continue(
		mpeg1_pes_header_kind kind, uint16_t packet_bytes_remaining)
{
	if (packet_bytes_remaining == 0)
		return false;

	return kind == mpeg1_pes_header_kind::stuffing
		|| kind == mpeg1_pes_header_kind::std_buffer
		|| kind == mpeg1_pes_header_kind::pts
		|| kind == mpeg1_pes_header_kind::pts_dts;
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_MPEG_FORMAT_H
