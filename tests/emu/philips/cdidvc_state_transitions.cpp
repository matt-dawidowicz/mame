// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "cdidvc_mpeg_format.h"
#include "cdidvc_utils.h"

#include <array>
#include <cstdint>

namespace
{

void require_same_control_state(
	cdi_dvc::mpeg_audio_control_state const &lhs,
	cdi_dvc::mpeg_audio_control_state const &rhs)
{
	REQUIRE(lhs.requested_stream == rhs.requested_stream);
	REQUIRE(lhs.current_stream == rhs.current_stream);
	REQUIRE(lhs.stream_change_pending == rhs.stream_change_pending);
	REQUIRE(lhs.program_ended == rhs.program_ended);
}

uint32_t hash_control_state(uint32_t hash, cdi_dvc::mpeg_audio_control_state const &state)
{
	auto mix = [&hash](uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
		{
			hash ^= uint8_t(value >> shift);
			hash *= 16777619U;
		}
	};
	mix(state.requested_stream);
	mix(state.current_stream);
	mix(state.stream_change_pending ? 1U : 0U);
	mix(state.program_ended ? 1U : 0U);
	return hash;
}

} // anonymous namespace

TEST_CASE(
	"CD-i DVC pending stream change snapshot resumes identically",
	"[emu][philips][dvc][audio][save][stream-change]")
{
	for (uint16_t initial = 0; initial < 32; ++initial)
	{
		for (uint16_t requested = 0; requested < 32; ++requested)
		{
			INFO("initial=" << initial << " requested=" << requested);
			cdi_dvc::mpeg_audio_control_state live;
			live = cdi_dvc::request_mpeg_audio_stream(live, initial).state;
			live = cdi_dvc::commit_mpeg_audio_stream(live, uint8_t(0xc0 | initial)).state;

			auto const request = cdi_dvc::request_mpeg_audio_stream(live, requested);
			live = request.state;

			// This structure is the guest-visible stream-control state registered by
			// the production device.  Copying it models the scalar portion of a save
			// taken before the requested stream reaches an accepted PES/audio header.
			cdi_dvc::mpeg_audio_control_state restored = live;

			for (uint16_t encountered = 0; encountered < 32; ++encountered)
			{
				auto live_commit = cdi_dvc::commit_mpeg_audio_stream(
					live, uint8_t(0xc0 | encountered));
				auto restored_commit = cdi_dvc::commit_mpeg_audio_stream(
					restored, uint8_t(0xc0 | encountered));
				require_same_control_state(live_commit.state, restored_commit.state);
				REQUIRE(live_commit.signal_stream_change == restored_commit.signal_stream_change);

				if (encountered == requested)
				{
					REQUIRE(live_commit.state.current_stream == requested);
					REQUIRE_FALSE(live_commit.state.stream_change_pending);
					REQUIRE(live_commit.signal_stream_change == (requested != initial));
					break;
				}
			}
		}
	}
}

TEST_CASE(
	"CD-i DVC program-end snapshot stays ended until explicit abort",
	"[emu][philips][dvc][audio][save][termination][restart]")
{
	for (uint16_t stream = 0; stream < 32; ++stream)
	{
		cdi_dvc::mpeg_audio_control_state live;
		live = cdi_dvc::request_mpeg_audio_stream(live, stream).state;
		live = cdi_dvc::commit_mpeg_audio_stream(live, uint8_t(0xc0 | stream)).state;
		live = cdi_dvc::end_mpeg_audio_program(live);
		cdi_dvc::mpeg_audio_control_state restored = live;

		REQUIRE_FALSE(cdi_dvc::mpeg_audio_input_accepting(live));
		REQUIRE_FALSE(cdi_dvc::mpeg_audio_input_accepting(restored));

		uint16_t const next = uint16_t((stream + 1) & 0x1f);
		auto live_request = cdi_dvc::request_mpeg_audio_stream(live, next);
		auto restored_request = cdi_dvc::request_mpeg_audio_stream(restored, next);
		require_same_control_state(live_request.state, restored_request.state);
		REQUIRE_FALSE(live_request.restart_decoder);
		REQUIRE_FALSE(restored_request.restart_decoder);
		REQUIRE_FALSE(live_request.state.stream_change_pending);

		live = cdi_dvc::abort_mpeg_audio_program(live_request.state);
		restored = cdi_dvc::abort_mpeg_audio_program(restored_request.state);
		require_same_control_state(live, restored);
		REQUIRE(cdi_dvc::mpeg_audio_input_accepting(live));
		REQUIRE(live.current_stream == cdi_dvc::MPEG_AUDIO_NO_CURRENT_STREAM);
		REQUIRE_FALSE(live.program_ended);

		auto live_commit = cdi_dvc::commit_mpeg_audio_stream(live, uint8_t(0xc0 | next));
		auto restored_commit = cdi_dvc::commit_mpeg_audio_stream(restored, uint8_t(0xc0 | next));
		require_same_control_state(live_commit.state, restored_commit.state);
		REQUIRE_FALSE(live_commit.signal_stream_change);
		REQUIRE(live_commit.state.current_stream == next);
	}
}

TEST_CASE(
	"CD-i DVC rapid stop restart and stream changes have deterministic long continuation",
	"[emu][philips][dvc][audio][save][transition][longrun]")
{
	cdi_dvc::mpeg_audio_control_state live;
	cdi_dvc::mpeg_audio_control_state restored;
	uint32_t live_hash = 2166136261U;
	uint32_t restored_hash = 2166136261U;

	// Run far beyond the number of control transitions a normal interactive FMV
	// sequence would perform.  Snapshot once while a change is pending, then
	// prove every future transition and the final hash remain identical.
	constexpr unsigned transitions = 100'000;
	constexpr unsigned snapshot_at = 37'777;
	bool have_snapshot = false;

	for (unsigned index = 0; index < transitions; ++index)
	{
		uint16_t const requested = uint16_t((index * 13U + 7U) & 0x1fU);
		live = cdi_dvc::request_mpeg_audio_stream(live, requested).state;
		if (have_snapshot)
			restored = cdi_dvc::request_mpeg_audio_stream(restored, requested).state;

		if (index == snapshot_at)
		{
			restored = live;
			restored_hash = live_hash;
			have_snapshot = true;
		}

		// Periodically end the ISO stream.  A descriptor change while ended must
		// not reopen input; an explicit abort then admits the next stream.
		if ((index % 97U) == 96U)
		{
			live = cdi_dvc::end_mpeg_audio_program(live);
			if (have_snapshot)
				restored = cdi_dvc::end_mpeg_audio_program(restored);

			uint16_t const after_end = uint16_t((requested + 5U) & 0x1fU);
			live = cdi_dvc::request_mpeg_audio_stream(live, after_end).state;
			if (have_snapshot)
				restored = cdi_dvc::request_mpeg_audio_stream(restored, after_end).state;

			live = cdi_dvc::abort_mpeg_audio_program(live);
			if (have_snapshot)
				restored = cdi_dvc::abort_mpeg_audio_program(restored);
		}

		uint8_t const selected_id = uint8_t(0xc0 | live.requested_stream);
		live = cdi_dvc::commit_mpeg_audio_stream(live, selected_id).state;
		live_hash = hash_control_state(live_hash, live);

		if (have_snapshot)
		{
			uint8_t const restored_id = uint8_t(0xc0 | restored.requested_stream);
			restored = cdi_dvc::commit_mpeg_audio_stream(restored, restored_id).state;
			restored_hash = hash_control_state(restored_hash, restored);
			require_same_control_state(live, restored);
		}
	}

	REQUIRE(have_snapshot);
	REQUIRE(live_hash == restored_hash);
}

TEST_CASE(
	"CD-i DVC system transition command bits remain independent",
	"[emu][philips][dvc][transition][command]")
{
	for (unsigned command = 0; command <= 0xffff; ++command)
	{
		auto const effects = cdi_dvc::decode_system_command(uint16_t(command));
		INFO("command=" << command);
		REQUIRE(effects.play == bool(command & 0x0008));
		REQUIRE(effects.pause == bool(command & 0x0010));
		REQUIRE(effects.continue_playback == bool(command & 0x0020));
		REQUIRE(effects.step == bool(command & 0x0040));
		REQUIRE(effects.stop == bool(command & 0x0080));
		REQUIRE(effects.clear_fifo == bool(command & 0x0100));
		REQUIRE(effects.decoder_on == bool(command & 0x1000));
		REQUIRE(effects.decoder_off == bool(command & 0x2000));
		REQUIRE(effects.dma == bool(command & 0x8000));
	}
}
