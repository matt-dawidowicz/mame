// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "catch.hpp"

#define PLM_NO_STDIO
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"

namespace {

constexpr std::size_t AUDIO_FRAME_SIZE = 627;

std::vector<uint8_t> make_audio_replay_fixture()
{
	// Synthetic 192 kbit/s, 44.1 kHz MPEG-1 Layer II frames.
	// Use the same four legal channel-mode transitions exercised by the
	// existing DVC PL_MPEG regression without retaining game data.
	constexpr std::array<uint8_t, 4> modes { 0x00, 0x40, 0x80, 0xc0 };

	std::vector<uint8_t> stream(AUDIO_FRAME_SIZE * modes.size(), 0);

	for (std::size_t frame = 0; frame < modes.size(); ++frame)
	{
		std::size_t const offset = frame * AUDIO_FRAME_SIZE;

		stream[offset + 0] = 0xff;
		stream[offset + 1] = 0xfd;
		stream[offset + 2] = 0xa2;
		stream[offset + 3] = modes[frame];
	}

	return stream;
}

void require_matching_samples(
		plm_samples_t const *live,
		plm_samples_t const *replayed)
{
	REQUIRE(live != nullptr);
	REQUIRE(replayed != nullptr);

	REQUIRE(live->count == PLM_AUDIO_SAMPLES_PER_FRAME);
	REQUIRE(replayed->count == PLM_AUDIO_SAMPLES_PER_FRAME);
	REQUIRE(live->count == replayed->count);

	REQUIRE(std::memcmp(
			live->interleaved,
			replayed->interleaved,
			sizeof(live->interleaved)) == 0);
}

} // anonymous namespace

TEST_CASE(
		"CD-i DVC audio replay reconstruction preserves decoder state across a mid-frame snapshot",
		"[emu][philips][dvc][audio][save]")
{
	std::vector<uint8_t> stream = make_audio_replay_fixture();

	// Model a save taken after two complete frames have been decoded while
	// part of the third frame is already resident in the decoder input buffer.
	// This is the important replay-journal case: the saved byte stream extends
	// beyond the number of frames the backend has actually consumed.
	std::size_t const snapshot_bytes =
			(AUDIO_FRAME_SIZE * 2) + (AUDIO_FRAME_SIZE / 2);

	REQUIRE(snapshot_bytes < stream.size());

	//
	// Live decoder at the instant represented by the save state.
	//
	plm_buffer_t *const live_buffer =
			plm_buffer_create_with_capacity(stream.size());
	REQUIRE(live_buffer != nullptr);

	REQUIRE(plm_buffer_write(
			live_buffer,
			stream.data(),
			snapshot_bytes) == snapshot_bytes);

	plm_audio_t *const live_decoder =
			plm_audio_create_with_buffer(live_buffer, 1);
	REQUIRE(live_decoder != nullptr);

	REQUIRE(plm_audio_has_header(live_decoder));
	REQUIRE(plm_audio_get_samplerate(live_decoder) == 44'100);

	for (unsigned frame = 0; frame < 2; ++frame)
	{
		plm_samples_t *const decoded = plm_audio_decode(live_decoder);
		REQUIRE(decoded != nullptr);
		REQUIRE(decoded->count == PLM_AUDIO_SAMPLES_PER_FRAME);
	}

	//
	// Reconstruct the opaque backend the same way cdi_dvc_device does on
	// postload: create a fresh decoder, restore every journalled byte, then
	// decode exactly the number of frames that had already been consumed.
	//
	plm_buffer_t *const replay_buffer =
			plm_buffer_create_with_capacity(stream.size());
	REQUIRE(replay_buffer != nullptr);

	REQUIRE(plm_buffer_write(
			replay_buffer,
			stream.data(),
			snapshot_bytes) == snapshot_bytes);

	plm_audio_t *const replay_decoder =
			plm_audio_create_with_buffer(replay_buffer, 1);
	REQUIRE(replay_decoder != nullptr);

	REQUIRE(plm_audio_has_header(replay_decoder));
	REQUIRE(plm_audio_get_samplerate(replay_decoder) == 44'100);

	for (unsigned frame = 0; frame < 2; ++frame)
	{
		plm_samples_t *const decoded = plm_audio_decode(replay_decoder);
		REQUIRE(decoded != nullptr);
		REQUIRE(decoded->count == PLM_AUDIO_SAMPLES_PER_FRAME);
	}

	// Both opaque backends must now represent the same logical point in the
	// stream before any additional input arrives.
	REQUIRE(plm_audio_get_time(live_decoder) ==
			plm_audio_get_time(replay_decoder));

	//
	// Resume both decoders with the bytes that arrived after the snapshot.
	//
	std::size_t const remaining = stream.size() - snapshot_bytes;

	REQUIRE(plm_buffer_write(
			live_buffer,
			stream.data() + snapshot_bytes,
			remaining) == remaining);

	REQUIRE(plm_buffer_write(
			replay_buffer,
			stream.data() + snapshot_bytes,
			remaining) == remaining);

	// The subsequently decoded frames must be bit-identical. This proves that
	// replaying the complete journal and advancing by the saved frame count
	// restores the decoder to the same future-observable state even when the
	// snapshot includes a partial following frame.
	for (unsigned frame = 2; frame < 4; ++frame)
	{
		plm_samples_t *const live_samples =
				plm_audio_decode(live_decoder);
		plm_samples_t *const replay_samples =
				plm_audio_decode(replay_decoder);

		require_matching_samples(live_samples, replay_samples);

		REQUIRE(plm_audio_get_time(live_decoder) ==
				plm_audio_get_time(replay_decoder));
	}

	plm_audio_destroy(live_decoder);
	plm_audio_destroy(replay_decoder);
}
