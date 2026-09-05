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

constexpr bool mpeg_audio_stream_id(uint8_t stream_id)
{
	return stream_id >= 0xc0 && stream_id <= 0xdf;
}

// An unselected MPEG-audio PES header is still parsed.  Green Book IX.5.4.3.1
// permits a stream change within one ISO 11172 stream and requires the new
// stream to begin at the first following audio-frame sync.  Retaining the PES
// boundary lets a selector write take effect during an in-flight packet without
// ever feeding bytes from the formerly selected stream.
constexpr bool mpeg1_packet_needs_pes_header(
		bool for_fma, uint8_t stream_id, bool selected)
{
	return selected || (for_fma && mpeg_audio_stream_id(stream_id));
}

constexpr uint16_t MPEG_AUDIO_NO_CURRENT_STREAM = 0xffffU;

struct mpeg_audio_control_state
{
	uint16_t requested_stream = 0;
	uint16_t current_stream = MPEG_AUDIO_NO_CURRENT_STREAM;
	bool stream_change_pending = false;
	bool program_ended = false;
};

struct mpeg_audio_stream_request_result
{
	mpeg_audio_control_state state;
	bool requested_changed;
	bool restart_decoder;
};

// MD_Stream changes immediately, whereas MAS_Stream continues to describe the
// stream actually being decoded until the requested stream is found.  A request
// after the ISO end is retained for descriptor readback but cannot reopen the
// ended input; a playback abort is required first.
constexpr mpeg_audio_stream_request_result request_mpeg_audio_stream(
		mpeg_audio_control_state state, uint16_t requested_stream)
{
	uint16_t const normalized = normalize_mpeg_stream_number(true, requested_stream);
	bool const changed = normalized != state.requested_stream;
	if (changed)
	{
		state.requested_stream = normalized;
		state.stream_change_pending = !state.program_ended
			&& state.current_stream != normalized;
	}

	return { state, changed, changed && !state.program_ended };
}

struct mpeg_audio_stream_commit_result
{
	mpeg_audio_control_state state;
	bool signal_stream_change;
};

// Commit MAS_Stream only once a header from the requested packet has actually
// been accepted by the decoder.  The initial stream establishes status without
// a CSU event; CSU belongs to a requested change during the active ISO stream.
constexpr mpeg_audio_stream_commit_result commit_mpeg_audio_stream(
		mpeg_audio_control_state state, uint8_t stream_id)
{
	if (state.program_ended
			|| !mpeg_stream_selected(true, stream_id, state.requested_stream))
		return { state, false };

	bool const signal = state.stream_change_pending
		&& state.current_stream != state.requested_stream;
	state.current_stream = state.requested_stream;
	state.stream_change_pending = false;
	return { state, signal };
}

constexpr mpeg_audio_control_state end_mpeg_audio_program(
		mpeg_audio_control_state state)
{
	state.program_ended = true;
	state.stream_change_pending = false;
	return state;
}

// An abort retains the requested descriptor stream but releases the actual
// decoder stream and is the only transition that admits a following ISO stream.
constexpr mpeg_audio_control_state abort_mpeg_audio_program(
		mpeg_audio_control_state state)
{
	state.current_stream = MPEG_AUDIO_NO_CURRENT_STREAM;
	state.stream_change_pending = false;
	state.program_ended = false;
	return state;
}

constexpr bool mpeg_audio_input_accepting(mpeg_audio_control_state const &state)
{
	return !state.program_ended;
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
