// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstdint>

#include "catch.hpp"

#include "cdidvc_mpeg_format.h"

TEST_CASE("CD-i DVC MPEG-1 PES first-header-byte classifier covers all 256 values", "[emu][philips][dvc][mpeg]")
{
	for (unsigned value = 0; value <= 0xff; ++value)
	{
		uint8_t const data = uint8_t(value);
		cdi_dvc::mpeg1_pes_header_kind expected = cdi_dvc::mpeg1_pes_header_kind::payload_fallback;
		if (data == 0xff)
			expected = cdi_dvc::mpeg1_pes_header_kind::stuffing;
		else if ((data & 0xc0) == 0x40)
			expected = cdi_dvc::mpeg1_pes_header_kind::std_buffer;
		else if ((data & 0xf0) == 0x20)
			expected = cdi_dvc::mpeg1_pes_header_kind::pts;
		else if ((data & 0xf0) == 0x30)
			expected = cdi_dvc::mpeg1_pes_header_kind::pts_dts;
		else if (data == 0x0f)
			expected = cdi_dvc::mpeg1_pes_header_kind::no_timestamp;

		INFO("data=" << value);
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(data) == expected);
	}
}

TEST_CASE("CD-i DVC MPEG-1 PES classifier preserves parser precedence", "[emu][philips][dvc][mpeg]")
{
	using kind = cdi_dvc::mpeg1_pes_header_kind;

	REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(0xff) == kind::stuffing);
	for (unsigned value = 0x40; value <= 0x7f; ++value)
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(uint8_t(value)) == kind::std_buffer);
	for (unsigned value = 0x20; value <= 0x2f; ++value)
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(uint8_t(value)) == kind::pts);
	for (unsigned value = 0x30; value <= 0x3f; ++value)
		REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(uint8_t(value)) == kind::pts_dts);
	REQUIRE(cdi_dvc::classify_mpeg1_pes_header_byte(0x0f) == kind::no_timestamp);
}

TEST_CASE("CD-i DVC MPEG-1 PES headers cannot continue beyond the packet boundary", "[emu][philips][dvc][mpeg]")
{
	using kind = cdi_dvc::mpeg1_pes_header_kind;

	for (kind const value :
			{ kind::stuffing, kind::std_buffer, kind::pts, kind::pts_dts,
				kind::no_timestamp, kind::payload_fallback })
	{
		REQUIRE_FALSE(cdi_dvc::mpeg1_pes_header_can_continue(value, 0));
	}

	for (kind const value : { kind::stuffing, kind::std_buffer, kind::pts, kind::pts_dts })
	{
		REQUIRE(cdi_dvc::mpeg1_pes_header_can_continue(value, 1));
		REQUIRE(cdi_dvc::mpeg1_pes_header_can_continue(value, 0xffff));
	}

	for (kind const value : { kind::no_timestamp, kind::payload_fallback })
	{
		REQUIRE_FALSE(cdi_dvc::mpeg1_pes_header_can_continue(value, 1));
		REQUIRE_FALSE(cdi_dvc::mpeg1_pes_header_can_continue(value, 0xffff));
	}
}

TEST_CASE("CD-i DVC start-code routing covers every audio and video stream", "[emu][philips][dvc][mpeg][audio][exhaustive]")
{
	using route = cdi_dvc::mpeg1_start_code_route;

	for (bool const for_fma : { false, true })
	{
		unsigned const stream_count = for_fma ? 32 : 16;
		uint8_t const first_stream_id = for_fma ? 0xc0 : 0xe0;
		uint8_t const last_stream_id = for_fma ? 0xdf : 0xef;
		uint8_t const stream_mask = for_fma ? 0x1f : 0x0f;

		for (uint16_t selected_stream = 0; selected_stream < stream_count; ++selected_stream)
		{
			for (unsigned value = 0; value <= 0xff; ++value)
			{
				uint8_t const stream_id = uint8_t(value);
				route expected = route::skipped_packet;
				if (stream_id == 0xba)
					expected = route::pack_header;
				else if (stream_id == 0xb9)
					expected = route::program_end;
				else if (stream_id >= first_stream_id && stream_id <= last_stream_id
						&& (stream_id & stream_mask) == selected_stream)
					expected = route::selected_pes;

				INFO("for_fma=" << for_fma << " selected_stream=" << selected_stream
					<< " stream_id=" << value);
				REQUIRE(cdi_dvc::classify_mpeg1_start_code(
						for_fma, stream_id, selected_stream) == expected);
			}
		}
	}

	// The fifth audio stream-number bit is significant: Cx and Dx packets
	// must never alias one another.  High register bits remain reserved.
	for (uint16_t low = 0; low < 16; ++low)
	{
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xc0 | low), low)
			== route::selected_pes);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xd0 | low), low)
			== route::skipped_packet);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xc0 | low), 0x10 | low)
			== route::skipped_packet);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, uint8_t(0xd0 | low), 0x10 | low)
			== route::selected_pes);
	}

	for (uint32_t value = 0; value <= 0xffff; ++value)
	{
		REQUIRE(cdi_dvc::normalize_mpeg_stream_number(true, uint16_t(value)) == (value & 0x1f));
		REQUIRE(cdi_dvc::normalize_mpeg_stream_number(false, uint16_t(value)) == (value & 0x0f));
	}
}

TEST_CASE("CD-i DVC selected PES routing preserves every first-header-byte path", "[emu][philips][dvc][mpeg][audio][exhaustive]")
{
	using route = cdi_dvc::mpeg1_start_code_route;
	using kind = cdi_dvc::mpeg1_pes_header_kind;

	for (uint16_t selected_stream = 0; selected_stream < 32; ++selected_stream)
	{
		uint8_t const selected_id = uint8_t(0xc0 | selected_stream);
		uint8_t const other_id = uint8_t(0xc0 | ((selected_stream + 1) & 0x1f));
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, selected_id, selected_stream)
			== route::selected_pes);
		REQUIRE(cdi_dvc::classify_mpeg1_start_code(true, other_id, selected_stream)
			== route::skipped_packet);

		for (unsigned value = 0; value <= 0xff; ++value)
		{
			uint8_t const data = uint8_t(value);
			kind const header = cdi_dvc::classify_mpeg1_pes_header_byte(data);
			bool const continuation = cdi_dvc::mpeg1_pes_header_can_continue(header, 1);
			bool const expected_continuation = header == kind::stuffing
				|| header == kind::std_buffer || header == kind::pts || header == kind::pts_dts;
			INFO("selected_stream=" << selected_stream << " first_header_byte=" << value);
			REQUIRE(continuation == expected_continuation);
		}
	}
}

TEST_CASE("CD-i DVC stream switching retains every audio PES boundary", "[emu][philips][dvc][mpeg][audio][switch][exhaustive]")
{
	for (bool const for_fma : { false, true })
	{
		for (unsigned value = 0; value <= 0xff; ++value)
		{
			uint8_t const stream_id = uint8_t(value);
			for (bool const selected : { false, true })
			{
				bool const expected = selected
					|| (for_fma && stream_id >= 0xc0 && stream_id <= 0xdf);
				INFO("for_fma=" << for_fma << " stream_id=" << value
					<< " selected=" << selected);
				REQUIRE(cdi_dvc::mpeg1_packet_needs_pes_header(
						for_fma, stream_id, selected) == expected);
			}
		}
	}
}

TEST_CASE("CD-i DVC requested and current audio streams switch at decoder acceptance", "[emu][philips][dvc][mpeg][audio][switch]")
{
	using cdi_dvc::MPEG_AUDIO_NO_CURRENT_STREAM;
	using cdi_dvc::mpeg_audio_control_state;

	mpeg_audio_control_state state;
	REQUIRE(state.requested_stream == 0);
	REQUIRE(state.current_stream == MPEG_AUDIO_NO_CURRENT_STREAM);
	REQUIRE_FALSE(state.stream_change_pending);
	REQUIRE(cdi_dvc::mpeg_audio_input_accepting(state));

	// Initial decode establishes the actual stream without fabricating a CSU.
	auto committed = cdi_dvc::commit_mpeg_audio_stream(state, 0xc0);
	state = committed.state;
	REQUIRE(state.current_stream == 0);
	REQUIRE_FALSE(committed.signal_stream_change);

	// MD_Stream changes immediately.  MAS_Stream remains on the old stream
	// until a valid header from the requested packet reaches the decoder.
	auto requested = cdi_dvc::request_mpeg_audio_stream(state, 0xffe1);
	state = requested.state;
	REQUIRE(requested.requested_changed);
	REQUIRE(requested.restart_decoder);
	REQUIRE(state.requested_stream == 1);
	REQUIRE(state.current_stream == 0);
	REQUIRE(state.stream_change_pending);

	committed = cdi_dvc::commit_mpeg_audio_stream(state, 0xc2);
	state = committed.state;
	REQUIRE(state.current_stream == 0);
	REQUIRE(state.stream_change_pending);
	REQUIRE_FALSE(committed.signal_stream_change);

	committed = cdi_dvc::commit_mpeg_audio_stream(state, 0xc1);
	state = committed.state;
	REQUIRE(state.current_stream == 1);
	REQUIRE_FALSE(state.stream_change_pending);
	REQUIRE(committed.signal_stream_change);

	requested = cdi_dvc::request_mpeg_audio_stream(state, 1);
	REQUIRE_FALSE(requested.requested_changed);
	REQUIRE_FALSE(requested.restart_decoder);
}

TEST_CASE("CD-i DVC audio stream commit state is exhaustive", "[emu][philips][dvc][mpeg][audio][switch][exhaustive]")
{
	for (uint16_t requested = 0; requested < 32; ++requested)
	{
		for (unsigned current_index = 0; current_index <= 32; ++current_index)
		{
			uint16_t const current = current_index == 32
				? cdi_dvc::MPEG_AUDIO_NO_CURRENT_STREAM
				: uint16_t(current_index);
			for (bool const pending : { false, true })
			{
				for (bool const ended : { false, true })
				{
					for (unsigned value = 0; value <= 0xff; ++value)
					{
						uint8_t const stream_id = uint8_t(value);
						cdi_dvc::mpeg_audio_control_state const before {
							requested, current, pending, ended
						};
						auto const result = cdi_dvc::commit_mpeg_audio_stream(
								before, stream_id);
						bool const accepted = !ended
							&& stream_id >= 0xc0 && stream_id <= 0xdf
							&& (stream_id & 0x1f) == requested;
						bool const expected_signal = accepted && pending
							&& current != requested;

						INFO("requested=" << requested << " current=" << current
							<< " pending=" << pending << " ended=" << ended
							<< " stream_id=" << value);
						REQUIRE(result.signal_stream_change == expected_signal);
						REQUIRE(result.state.requested_stream == requested);
						REQUIRE(result.state.current_stream
							== (accepted ? requested : current));
						REQUIRE(result.state.stream_change_pending
							== (accepted ? false : pending));
						REQUIRE(result.state.program_ended == ended);
					}
				}
			}
		}
	}
}

TEST_CASE("CD-i DVC rapid stream requests cancel or commit exactly once", "[emu][philips][dvc][mpeg][audio][switch][save]")
{
	cdi_dvc::mpeg_audio_control_state state { 5, 5, false, false };

	for (uint16_t const requested_stream : { uint16_t(6), uint16_t(7) })
	{
		auto const request = cdi_dvc::request_mpeg_audio_stream(state, requested_stream);
		REQUIRE(request.requested_changed);
		REQUIRE(request.restart_decoder);
		state = request.state;
		REQUIRE(state.current_stream == 5);
		REQUIRE(state.stream_change_pending);
	}

	// Returning to the actual stream before another decoder accepts a header
	// cancels CSU; no physical stream change occurred.
	auto request = cdi_dvc::request_mpeg_audio_stream(state, 5);
	state = request.state;
	REQUIRE(request.requested_changed);
	REQUIRE(request.restart_decoder);
	REQUIRE_FALSE(state.stream_change_pending);
	auto commit = cdi_dvc::commit_mpeg_audio_stream(state, 0xc5);
	REQUIRE_FALSE(commit.signal_stream_change);

	// A save/load copy at a pending transition has identical future behavior.
	request = cdi_dvc::request_mpeg_audio_stream(commit.state, 31);
	cdi_dvc::mpeg_audio_control_state restored = request.state;
	auto const live_commit = cdi_dvc::commit_mpeg_audio_stream(request.state, 0xdf);
	auto const restored_commit = cdi_dvc::commit_mpeg_audio_stream(restored, 0xdf);
	REQUIRE(live_commit.state.requested_stream == restored_commit.state.requested_stream);
	REQUIRE(live_commit.state.current_stream == restored_commit.state.current_stream);
	REQUIRE(live_commit.state.stream_change_pending == restored_commit.state.stream_change_pending);
	REQUIRE(live_commit.signal_stream_change == restored_commit.signal_stream_change);
	REQUIRE(live_commit.signal_stream_change);
}

TEST_CASE("CD-i DVC ISO end stays closed until playback abort", "[emu][philips][dvc][mpeg][audio][termination][save]")
{
	cdi_dvc::mpeg_audio_control_state state { 3, 3, false, false };
	state = cdi_dvc::end_mpeg_audio_program(state);
	REQUIRE(state.program_ended);
	REQUIRE_FALSE(state.stream_change_pending);
	REQUIRE_FALSE(cdi_dvc::mpeg_audio_input_accepting(state));

	// The requested descriptor may still change, but the ended decoder cannot
	// silently reopen on bytes belonging to another ISO stream.
	auto const request = cdi_dvc::request_mpeg_audio_stream(state, 4);
	state = request.state;
	REQUIRE(request.requested_changed);
	REQUIRE_FALSE(request.restart_decoder);
	REQUIRE(state.requested_stream == 4);
	REQUIRE(state.current_stream == 3);
	REQUIRE_FALSE(state.stream_change_pending);
	REQUIRE_FALSE(cdi_dvc::commit_mpeg_audio_stream(state, 0xc4).signal_stream_change);

	// Abort is the documented boundary that releases the old ISO stream.
	state = cdi_dvc::abort_mpeg_audio_program(state);
	REQUIRE(cdi_dvc::mpeg_audio_input_accepting(state));
	REQUIRE(state.requested_stream == 4);
	REQUIRE(state.current_stream == cdi_dvc::MPEG_AUDIO_NO_CURRENT_STREAM);
	REQUIRE_FALSE(state.stream_change_pending);
	REQUIRE_FALSE(state.program_ended);
	auto const first = cdi_dvc::commit_mpeg_audio_stream(state, 0xc4);
	REQUIRE(first.state.current_stream == 4);
	REQUIRE_FALSE(first.signal_stream_change);
}
