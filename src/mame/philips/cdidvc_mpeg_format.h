// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_MPEG_FORMAT_H
#define MAME_PHILIPS_CDIDVC_MPEG_FORMAT_H

#pragma once

#include <cstdint>

namespace cdi_dvc
{

// ISO/IEC 11172 reserves five stream-number bits for MPEG audio (C0-DF)
// and four for MPEG video (E0-EF).  Green Book IX.5.3.1.5 permits every
// MPEG audio Stream ID, while IX.8.2.4 defines the selected audio stream as
// 0..31.  Keep the width in one helper so register writes and packet routing
// cannot accidentally alias Cx and Dx streams.
constexpr uint16_t mpeg_stream_number_mask(bool for_fma)
{
	return for_fma ? 0x001fU : 0x000fU;
}

constexpr uint16_t normalize_mpeg_stream_number(bool for_fma, uint16_t selected_stream)
{
	return selected_stream & mpeg_stream_number_mask(for_fma);
}

constexpr bool mpeg_stream_selected(bool for_fma, uint8_t stream_id, uint16_t selected_stream)
{
	if (for_fma)
	{
		return stream_id >= 0xc0 && stream_id <= 0xdf
			&& (stream_id & 0x1f) == normalize_mpeg_stream_number(true, selected_stream);
	}

	return stream_id >= 0xe0 && stream_id <= 0xef
		&& (stream_id & 0x0f) == normalize_mpeg_stream_number(false, selected_stream);
}

enum class mpeg1_start_code_route : uint8_t
{
	pack_header,
	program_end,
	selected_pes,
	skipped_packet
};

// Classify the byte following 00 00 01.  For all bounded packet types, the
// selected/skipped result owns whether the subsequent PES header is parsed or
// the complete packet is discarded.  This is standards syntax and routing,
// not a claim about VMPEG's private register implementation.
constexpr mpeg1_start_code_route classify_mpeg1_start_code(
		bool for_fma, uint8_t stream_id, uint16_t selected_stream)
{
	if (stream_id == 0xba)
		return mpeg1_start_code_route::pack_header;
	if (stream_id == 0xb9)
		return mpeg1_start_code_route::program_end;
	return mpeg_stream_selected(for_fma, stream_id, selected_stream)
		? mpeg1_start_code_route::selected_pes
		: mpeg1_start_code_route::skipped_packet;
}

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
