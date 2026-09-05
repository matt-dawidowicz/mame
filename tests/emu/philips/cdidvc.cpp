// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <vector>

#include "catch.hpp"

#include "cdidvc_save_state.h"
#include "cdidvc_utils.h"

#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"

TEST_CASE("CD-i DVC MPEG timestamp deltas wrap at 33 bits", "[emu][philips][dvc]")
{
	constexpr uint64_t wrap = uint64_t(1) << 33;
	constexpr uint64_t mask = wrap - 1;

	REQUIRE(cdi_dvc::mpeg_timestamp_delta(0, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(100, 40) == 60);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(40, 100) == -60);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(1, mask) == 2);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(mask, 1) == -2);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta((wrap >> 1) - 1, 0) == int64_t((wrap >> 1) - 1));
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(wrap >> 1, 0) == -int64_t(wrap >> 1));
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(wrap + 1, 1) == 0);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(1, wrap + 1) == 0);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(wrap + 5, wrap + 1) == 4);
	REQUIRE(cdi_dvc::mpeg_timestamp_delta(0, (wrap >> 1) + 1) == int64_t((wrap >> 1) - 1));
}

TEST_CASE("CD-i DVC MPEG timestamps convert from 90 kHz to 45 kHz deltas", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_dclk_delta(0, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(180, 90) == 45);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(90, 180) == -45);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(181, 90) == 45);

	constexpr uint64_t wrap = uint64_t(1) << 33;
	REQUIRE(cdi_dvc::mpeg_dclk_delta(wrap, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(0, wrap) == 0);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(wrap - 2, 0) == -1);
	REQUIRE(cdi_dvc::mpeg_dclk_delta(0, wrap - 2) == 1);
}

TEST_CASE("CD-i DVC FMA stream selection accepts matching MPEG audio streams", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_stream_selected(true, 0xc0, 0));
	REQUIRE(cdi_dvc::mpeg_stream_selected(true, 0xcf, 0x000f));
	REQUIRE(cdi_dvc::mpeg_stream_selected(true, 0xd3, 0x00f3));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(true, 0xc1, 0));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(true, 0xbf, 0x000f));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(true, 0xe0, 0));
}

TEST_CASE("CD-i DVC FMV stream selection accepts matching MPEG video streams", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_stream_selected(false, 0xe0, 0));
	REQUIRE(cdi_dvc::mpeg_stream_selected(false, 0xef, 0x000f));
	REQUIRE(cdi_dvc::mpeg_stream_selected(false, 0xe7, 0x00f7));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(false, 0xe1, 0));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(false, 0xdf, 0x000f));
	REQUIRE_FALSE(cdi_dvc::mpeg_stream_selected(false, 0xf0, 0));
}

TEST_CASE("CD-i DVC MPEG stream selection covers every stream ID and selector", "[emu][philips][dvc]")
{
	for (unsigned target = 0; target < 2; ++target)
	{
		bool const for_fma = target == 0;
		for (uint16_t selected_stream = 0; selected_stream < 16; ++selected_stream)
		{
			for (unsigned stream = 0; stream <= 0xff; ++stream)
			{
				uint8_t const stream_id = uint8_t(stream);
				bool const in_stream_range = for_fma
					? stream_id >= 0xc0 && stream_id <= 0xdf
					: stream_id >= 0xe0 && stream_id <= 0xef;
				bool const expected = in_stream_range && (stream_id & 0x0f) == selected_stream;

				INFO("for_fma=" << for_fma << " selected_stream=" << selected_stream << " stream_id=" << stream);
				REQUIRE(cdi_dvc::mpeg_stream_selected(for_fma, stream_id, selected_stream) == expected);
			}
		}
	}
}

TEST_CASE("CD-i DVC video commands decode the VMPEG VIDCMD bit field", "[emu][philips][dvc]")
{
	auto require_effects = [](uint16_t command, uint8_t video_buffer, bool scroll, bool register_update,
			bool swap_buffer, bool video_on, bool video_off, bool hide, bool show_immediate, bool show_on_next)
	{
		cdi_dvc::video_command_effects const effects = cdi_dvc::decode_video_command(command);
		INFO("command=" << command);
		REQUIRE(effects.video_buffer == video_buffer);
		REQUIRE(effects.scroll == scroll);
		REQUIRE(effects.register_update == register_update);
		REQUIRE(effects.swap_buffer == swap_buffer);
		REQUIRE(effects.video_on == video_on);
		REQUIRE(effects.video_off == video_off);
		REQUIRE(effects.hide == hide);
		REQUIRE(effects.show_immediate == show_immediate);
		REQUIRE(effects.show_on_next == show_on_next);
	};

	require_effects(0x0000, 0, false, false, false, false, false, false, false, false);
	require_effects(0x0003, 3, false, false, false, false, false, false, false, false);
	require_effects(0x0008, 0, false, true, false, false, false, false, false, false);
	require_effects(0x000c, 0, true, true, false, false, false, false, false, false);
	require_effects(0x0010, 0, false, false, true, false, false, false, false, false);
	require_effects(0x0020, 0, false, false, false, true, false, false, false, false);
	require_effects(0x0040, 0, false, false, false, false, true, false, false, false);
	require_effects(0x0120, 0, false, false, false, true, false, true, false, false);
	require_effects(0x0220, 0, false, false, false, true, false, false, true, false);
	require_effects(0x0420, 0, false, false, false, true, false, false, false, true);
	require_effects(0x077f, 3, true, true, true, true, true, true, true, true);
}

TEST_CASE("CD-i DVC video command decoding covers every 16-bit command", "[emu][philips][dvc]")
{
	for (unsigned command = 0; command <= 0xffff; ++command)
	{
		cdi_dvc::video_command_effects const effects = cdi_dvc::decode_video_command(uint16_t(command));
		INFO("command=" << command);
		REQUIRE(effects.video_buffer == (command & 0x0003));
		REQUIRE(effects.scroll == bool(command & 0x0004));
		REQUIRE(effects.register_update == bool(command & 0x0008));
		REQUIRE(effects.swap_buffer == bool(command & 0x0010));
		REQUIRE(effects.video_on == bool(command & 0x0020));
		REQUIRE(effects.video_off == bool(command & 0x0040));
		REQUIRE(effects.hide == bool(command & 0x0100));
		REQUIRE(effects.show_immediate == bool(command & 0x0200));
		REQUIRE(effects.show_on_next == bool(command & 0x0400));
	}
}

TEST_CASE("CD-i DVC FMA interrupt masks match VMPEG status bits", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::FMA_IRQ_END_ISO == 0x0001);
	REQUIRE(cdi_dvc::FMA_IRQ_STREAM_CHANGE == 0x0002);
	REQUIRE(cdi_dvc::FMA_IRQ_FRAME_DECODED == 0x0004);
	REQUIRE(cdi_dvc::FMA_IRQ_UNDERFLOW == 0x0008);
	REQUIRE(cdi_dvc::FMA_IRQ_DECODING_STARTED == 0x0010);
	REQUIRE(cdi_dvc::FMA_IRQ_TIMER == 0x0100);
}

TEST_CASE("CD-i DVC FMV interrupt masks match VMPEG status bits", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::FMV_IRQ_SEQUENCE == 0x0001);
	REQUIRE(cdi_dvc::FMV_IRQ_GOP == 0x0002);
	REQUIRE(cdi_dvc::FMV_IRQ_PICTURE == 0x0004);
	REQUIRE(cdi_dvc::FMV_IRQ_END_OF_DATA == 0x0008);
	REQUIRE(cdi_dvc::FMV_IRQ_DCL == 0x0080);
	REQUIRE(cdi_dvc::FMV_IRQ_TIMER == 0x0100);
	REQUIRE(cdi_dvc::FMV_IRQ_END_SEQUENCE == 0x0200);
	REQUIRE(cdi_dvc::FMV_IRQ_END_ISO == 0x0400);
	REQUIRE(cdi_dvc::FMV_IRQ_VSYNC == 0x0800);
	REQUIRE(cdi_dvc::FMV_IRQ_PAUSE == 0x1000);
	REQUIRE(cdi_dvc::FMV_IRQ_CLIP_UPDATE == 0x2000);
	REQUIRE(cdi_dvc::FMV_IRQ_GEOMETRY_LATCH == 0x2080);
}

TEST_CASE("CD-i DVC system command bits expose playback and decoder effects", "[emu][philips][dvc]")
{
	for (uint32_t command = 0; command <= 0xffff; ++command)
	{
		cdi_dvc::system_command_effects const effects =
			cdi_dvc::decode_system_command(uint16_t(command));
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

TEST_CASE("CD-i DVC compressed input status honors the VMPEG high-water boundary", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::FMV_INPUT_FIFO_HIGH_WATER_BYTES == 28'000);
	REQUIRE(cdi_dvc::FMV_OUTPUT_FIFO_PICTURES == 3);
	REQUIRE(cdi_dvc::fmv_input_status(0) == cdi_dvc::FMV_STATUS_INPUT_READY);
	REQUIRE(cdi_dvc::fmv_input_status(27'999) == cdi_dvc::FMV_STATUS_INPUT_READY);
	REQUIRE(cdi_dvc::fmv_input_status(28'000) == cdi_dvc::FMV_STATUS_INPUT_READY);
	REQUIRE(cdi_dvc::fmv_input_status(28'001) == 0);
}

TEST_CASE("CD-i DVC incomplete MPEG pictures cannot deadlock compressed input", "[emu][philips][dvc]")
{
	// Dragon's Lair reached this exact state: stock PL_MPEG retained
	// 28,380 bytes while waiting for the next picture start code.
	// Treating those retained bytes as physical FIFO occupancy prevents
	// the firmware from supplying the data PL_MPEG itself requires.
	REQUIRE(cdi_dvc::fmv_input_status_from_backend(28'380, false) == 0);
	REQUIRE(cdi_dvc::fmv_input_status_from_backend(
			28'380, true) == cdi_dvc::FMV_STATUS_INPUT_READY);

	// The underlying hardware high-water rule remains unchanged.
	REQUIRE(cdi_dvc::fmv_input_status_from_backend(
			28'000, false) == cdi_dvc::FMV_STATUS_INPUT_READY);
	REQUIRE(cdi_dvc::fmv_input_status_from_backend(28'001, false) == 0);
}

TEST_CASE("CD-i DVC frame-period register converts millihertz to 90 kHz ticks", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::fmv_frame_period_90khz(0) == 0);
	REQUIRE(cdi_dvc::fmv_frame_period_90khz(25'000) == 3'600);
	REQUIRE(cdi_dvc::fmv_frame_period_90khz(29'970) == 3'003);
	REQUIRE(cdi_dvc::fmv_frame_period_90khz(30'000) == 3'000);
}

TEST_CASE("CD-i DVC video replay pump events preserve offset flush and frame target", "[emu][philips][dvc]")
{
	uint64_t const event = cdi_dvc::save_video_replay_pump_event(0x0123456, true, 0x12345678);
	REQUIRE(cdi_dvc::save_video_replay_pump_offset(event) == 0x0123456);
	REQUIRE(cdi_dvc::save_video_replay_pump_flush(event));
	REQUIRE(cdi_dvc::save_video_replay_pump_frames(event) == 0x12345678);

	uint64_t const plain = cdi_dvc::save_video_replay_pump_event(
			cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY, false, 3);
	REQUIRE(cdi_dvc::save_video_replay_pump_offset(plain)
			== cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY);
	REQUIRE_FALSE(cdi_dvc::save_video_replay_pump_flush(plain));
	REQUIRE(cdi_dvc::save_video_replay_pump_frames(plain) == 3);
}

TEST_CASE("CD-i DVC presentation storage rejects unsafe restored states", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::video_present_frame_fits(false, 0, 0, 0));
	REQUIRE(cdi_dvc::video_present_frame_fits(true, 1, 1, 1));
	REQUIRE(cdi_dvc::video_present_frame_fits(
			true, cdi_dvc::SAVE_VIDEO_MAX_WIDTH, cdi_dvc::SAVE_VIDEO_MAX_HEIGHT,
			cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME));

	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(true, 0, 0, 0));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(true, 320, 240, 0));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(true, 320, 240, 320 * 240 - 1));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(false, 320, 240, 0));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(false, 0, 0, 1));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(
			true, cdi_dvc::SAVE_VIDEO_MAX_WIDTH + 1, 1,
			cdi_dvc::SAVE_VIDEO_MAX_WIDTH + 1));
}

TEST_CASE("CD-i DVC picture interrupts follow the presented generation", "[emu][philips][dvc]")
{
	cdi_dvc::presentation_picture_event event = cdi_dvc::make_presentation_picture_event(
			cdi_dvc::FMV_IRQ_SEQUENCE | cdi_dvc::FMV_IRQ_GOP,
			true, 41, 42);
	REQUIRE(event.interrupts ==
			(cdi_dvc::FMV_IRQ_PICTURE | cdi_dvc::FMV_IRQ_SEQUENCE | cdi_dvc::FMV_IRQ_GOP));
	REQUIRE_FALSE(event.end_of_data);

	event = cdi_dvc::make_presentation_picture_event(
			cdi_dvc::FMV_IRQ_SEQUENCE | cdi_dvc::FMV_IRQ_GOP,
			true, 42, 42);
	REQUIRE(event.interrupts ==
			(cdi_dvc::FMV_IRQ_PICTURE | cdi_dvc::FMV_IRQ_SEQUENCE
				| cdi_dvc::FMV_IRQ_GOP | cdi_dvc::FMV_IRQ_END_OF_DATA));
	REQUIRE(event.end_of_data);
}

TEST_CASE("CD-i DVC picture FIFO count saturates to the VMPEG register width", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::fmv_pictures_in_fifo(0) == 0);
	REQUIRE(cdi_dvc::fmv_pictures_in_fifo(1) == 1);
	REQUIRE(cdi_dvc::fmv_pictures_in_fifo(0x7e) == 0x7e);
	REQUIRE(cdi_dvc::fmv_pictures_in_fifo(0x7f) == 0x7f);
	REQUIRE(cdi_dvc::fmv_pictures_in_fifo(0x10000) == 0x7f);
}

TEST_CASE("CD-i DVC decoding timestamp status exposes the VMPEG reduced view", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::FMV_VDI_DECODING_TIMESTAMP_UPDATED == 0x4000);
	REQUIRE(cdi_dvc::fmv_reduced_decoding_timestamp(0) == 0);
	REQUIRE(cdi_dvc::fmv_reduced_decoding_timestamp(0x7f) == 0);
	REQUIRE(cdi_dvc::fmv_reduced_decoding_timestamp(0x80) == 1);
	REQUIRE(cdi_dvc::fmv_reduced_decoding_timestamp(uint64_t(0x7fff) << 7) == 0x7fff);
	REQUIRE(cdi_dvc::fmv_reduced_decoding_timestamp(uint64_t(0x8000) << 7) == 0);
}

TEST_CASE("CD-i DVC picture events follow MPEG reference-frame reordering", "[emu][philips][dvc]")
{
	using state = cdi_dvc::picture_event_reorder_state;
	using result = cdi_dvc::picture_event_reorder_result;

	state current { 0, false };
	result step = cdi_dvc::reorder_picture_events(current, 1,
			cdi_dvc::FMV_IRQ_SEQUENCE | cdi_dvc::FMV_IRQ_GOP);
	REQUIRE_FALSE(step.output_valid);
	REQUIRE(step.state.reference_valid);
	REQUIRE(step.state.reference_interrupts ==
			(cdi_dvc::FMV_IRQ_SEQUENCE | cdi_dvc::FMV_IRQ_GOP));
	current = step.state;

	step = cdi_dvc::reorder_picture_events(current, 2, 0);
	REQUIRE(step.output_valid);
	REQUIRE(step.output_interrupts ==
			(cdi_dvc::FMV_IRQ_SEQUENCE | cdi_dvc::FMV_IRQ_GOP));
	REQUIRE(step.state.reference_valid);
	REQUIRE(step.state.reference_interrupts == 0);
	current = step.state;

	step = cdi_dvc::reorder_picture_events(current, 3, 0);
	REQUIRE(step.output_valid);
	REQUIRE(step.output_interrupts == 0);
	REQUIRE(step.state.reference_valid);
	REQUIRE(step.state.reference_interrupts == 0);

	step = cdi_dvc::flush_picture_events(step.state);
	REQUIRE(step.output_valid);
	REQUIRE(step.output_interrupts == 0);
	REQUIRE_FALSE(step.state.reference_valid);
	REQUIRE(step.state.reference_interrupts == 0);
}

TEST_CASE("CD-i DVC picture-event reordering covers every picture type and marker combination", "[emu][philips][dvc]")
{
	for (unsigned reference_valid = 0; reference_valid <= 1; ++reference_valid)
	{
		for (unsigned reference_interrupts = 0; reference_interrupts <= 3; ++reference_interrupts)
		{
			for (unsigned picture_type = 0; picture_type <= 7; ++picture_type)
			{
				for (unsigned picture_interrupts = 0; picture_interrupts <= 3; ++picture_interrupts)
				{
					cdi_dvc::picture_event_reorder_state const initial {
						uint16_t(reference_interrupts), bool(reference_valid)
					};
					cdi_dvc::picture_event_reorder_result const actual = cdi_dvc::reorder_picture_events(
							initial, uint8_t(picture_type), uint16_t(picture_interrupts));
					INFO("reference_valid=" << reference_valid
							<< " reference_interrupts=" << reference_interrupts
							<< " picture_type=" << picture_type
							<< " picture_interrupts=" << picture_interrupts);

					if (picture_type == 1 || picture_type == 2)
					{
						REQUIRE(actual.output_valid == bool(reference_valid));
						REQUIRE(actual.output_interrupts == reference_interrupts);
						REQUIRE(actual.state.reference_valid);
						REQUIRE(actual.state.reference_interrupts == picture_interrupts);
					}
					else if (picture_type == 3)
					{
						REQUIRE(actual.output_valid);
						REQUIRE(actual.output_interrupts == picture_interrupts);
						REQUIRE(actual.state.reference_valid == bool(reference_valid));
						REQUIRE(actual.state.reference_interrupts == reference_interrupts);
					}
					else
					{
						REQUIRE_FALSE(actual.output_valid);
						REQUIRE(actual.output_interrupts == 0);
						REQUIRE(actual.state.reference_valid == bool(reference_valid));
						REQUIRE(actual.state.reference_interrupts == reference_interrupts);
					}
				}
			}
		}
	}
}

TEST_CASE("CD-i DVC audio queue compaction discards only consumed samples", "[emu][philips][dvc][audio]")
{
	std::vector<int16_t> samples { 10, 11, 20, 21, 30, 31 };
	std::size_t read = 2;
	cdi_dvc::compact_consumed_audio_samples(samples, read);
	REQUIRE(read == 0);
	REQUIRE((samples == std::vector<int16_t> { 20, 21, 30, 31 }));

	read = 0;
	cdi_dvc::compact_consumed_audio_samples(samples, read);
	REQUIRE(read == 0);
	REQUIRE((samples == std::vector<int16_t> { 20, 21, 30, 31 }));

	read = samples.size();
	cdi_dvc::compact_consumed_audio_samples(samples, read);
	REQUIRE(read == 0);
	REQUIRE(samples.empty());

	samples = { 40, 41 };
	read = samples.size() + 2;
	cdi_dvc::compact_consumed_audio_samples(samples, read);
	REQUIRE(read == 0);
	REQUIRE(samples.empty());
}

TEST_CASE("CD-i DVC PCM output preserves order across scheduled silence starvation and refill", "[emu][philips][dvc][audio]")
{
	using kind = cdi_dvc::audio_output_kind;
	std::vector<int16_t> samples { 10, 11, 20, 21 };
	std::size_t read = 0;
	uint64_t wait = 2;
	uint32_t hash = 2166136261U;

	auto take = [&]()
	{
		return cdi_dvc::take_audio_output_frame(samples, read, wait);
	};
	auto hash_frame = [&hash](cdi_dvc::audio_output_frame const &frame)
	{
		hash = cdi_dvc::hash_pcm16_sample(hash, frame.left);
		hash = cdi_dvc::hash_pcm16_sample(hash, frame.right);
	};

	auto output = take();
	REQUIRE(output.kind == kind::scheduled_silence);
	REQUIRE(output.left == 0);
	REQUIRE(output.right == 0);
	REQUIRE(output.pending_frames_before == 2);
	REQUIRE_FALSE(output.drained);
	REQUIRE(wait == 1);
	REQUIRE(read == 0);

	output = take();
	REQUIRE(output.kind == kind::scheduled_silence);
	REQUIRE(wait == 0);
	REQUIRE(read == 0);

	output = take();
	REQUIRE(output.kind == kind::pcm);
	REQUIRE(output.left == 10);
	REQUIRE(output.right == 11);
	REQUIRE(output.pending_frames_before == 2);
	REQUIRE_FALSE(output.drained);
	hash_frame(output);

	output = take();
	REQUIRE(output.kind == kind::pcm);
	REQUIRE(output.left == 20);
	REQUIRE(output.right == 21);
	REQUIRE(output.pending_frames_before == 1);
	REQUIRE(output.drained);
	REQUIRE(samples.empty());
	REQUIRE(read == 0);
	hash_frame(output);

	output = take();
	REQUIRE(output.kind == kind::starvation);
	REQUIRE(output.left == 0);
	REQUIRE(output.right == 0);
	REQUIRE_FALSE(output.drained);
	REQUIRE(samples.empty());

	// A refill starts at its first sample: no already-consumed frame is replayed
	// and no incomplete stereo pair can leak a single-channel value.
	samples.push_back(30);
	output = take();
	REQUIRE(output.kind == kind::starvation);
	REQUIRE(samples.size() == 1);
	REQUIRE(read == 0);
	samples.push_back(31);
	output = take();
	REQUIRE(output.kind == kind::pcm);
	REQUIRE(output.left == 30);
	REQUIRE(output.right == 31);
	REQUIRE(output.drained);
	hash_frame(output);

	REQUIRE(hash == 0xfdc1c8bcU);
}

TEST_CASE("CD-i DVC MPEG audio accepts legal per-frame channel-mode changes", "[emu][philips][dvc]")
{
	// Synthetic 192 kbit/s, 44.1 kHz MPEG-1 Layer II frames cover all four
	// channel modes.  The first two reproduce the transition observed
	// at the beginning of The 7th Guest stream without retaining game data.
	constexpr size_t frame_size = 627;
	constexpr std::array<uint8_t, 4> modes { 0x00, 0x40, 0x80, 0xc0 };
	std::vector<uint8_t> stream(frame_size * modes.size(), 0);
	auto write_header = [&stream](size_t offset, uint8_t mode)
	{
		stream[offset + 0] = 0xff;
		stream[offset + 1] = 0xfd;
		stream[offset + 2] = 0xa2;
		stream[offset + 3] = mode;
	};
	for (size_t i = 0; i < modes.size(); ++i)
		write_header(frame_size * i, modes[i]);

	plm_buffer_t *const buffer = plm_buffer_create_with_capacity(stream.size());
	REQUIRE(buffer != nullptr);
	REQUIRE(plm_buffer_write(buffer, stream.data(), stream.size()) == stream.size());
	plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
	REQUIRE(decoder != nullptr);
	REQUIRE(plm_audio_has_header(decoder));
	REQUIRE(decoder->mode == PLM_AUDIO_MODE_STEREO);

	constexpr std::array<int, 4> expected_modes {
		PLM_AUDIO_MODE_STEREO,
		PLM_AUDIO_MODE_JOINT_STEREO,
		PLM_AUDIO_MODE_DUAL_CHANNEL,
		PLM_AUDIO_MODE_MONO
	};
	for (int const expected_mode : expected_modes)
	{
		plm_samples_t *const decoded = plm_audio_decode(decoder);
		REQUIRE(decoded != nullptr);
		REQUIRE(decoded->count == PLM_AUDIO_SAMPLES_PER_FRAME);
		REQUIRE(decoder->mode == expected_mode);
	}

	plm_audio_destroy(decoder);
}

TEST_CASE("CD-i DVC MPEG audio rejects unsupported bitrate indices", "[emu][philips][dvc]")
{
	auto header_is_accepted = [](uint8_t bitrate_index)
	{
		std::array<uint8_t, 6> stream {
			0xff,
			0xfd,
			uint8_t(bitrate_index << 4),
			0x00,
			0x00,
			0x00
		};

		plm_buffer_t *const buffer = plm_buffer_create_with_capacity(stream.size());
		REQUIRE(buffer != nullptr);
		REQUIRE(plm_buffer_write(buffer, stream.data(), stream.size()) == stream.size());
		plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
		REQUIRE(decoder != nullptr);
		bool const accepted = plm_audio_has_header(decoder);
		plm_audio_destroy(decoder);
		return accepted;
	};

	REQUIRE_FALSE(header_is_accepted(0));
	REQUIRE_FALSE(header_is_accepted(15));
}

namespace {

std::vector<uint8_t> make_silent_layer2_frame(
		uint8_t bitrate_index,
		uint8_t sample_rate_index = 0,
		uint8_t mode = PLM_AUDIO_MODE_STEREO,
		bool padding = false)
{
	uint32_t const header =
		(0x7ffU << 21) |
		(3U << 19) |
		(2U << 17) |
		(1U << 16) |
		(uint32_t(bitrate_index) << 12) |
		(uint32_t(sample_rate_index) << 10) |
		(uint32_t(padding) << 9) |
		(uint32_t(mode) << 6);
	auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
	REQUIRE(decoded.valid);

	std::vector<uint8_t> frame(decoded.frame_size_bytes, 0);
	frame[0] = uint8_t(header >> 24);
	frame[1] = uint8_t(header >> 16);
	frame[2] = uint8_t(header >> 8);
	frame[3] = uint8_t(header);
	return frame;
}

void append_frame(std::vector<uint8_t> &stream, std::vector<uint8_t> const &frame)
{
	stream.insert(stream.end(), frame.begin(), frame.end());
}

} // anonymous namespace

TEST_CASE("CD-i DVC PL_MPEG backend accepts per-frame Layer II bitrate changes", "[emu][philips][dvc][audio]")
{
	// This isolates conventional decoder capability.  Green Book IX.5.3.2.1
	// requires a bitrate change to begin a new CD-i audio sequence, identified
	// by its presentation-time gap; raw elementary-stream data has no PTS.
	struct frame_spec
	{
		uint8_t bitrate_index;
		uint8_t mode;
		bool padding;
	};

	constexpr std::array<frame_spec, 4> frames {{
		{ 10, PLM_AUDIO_MODE_STEREO, false },
		{ 11, PLM_AUDIO_MODE_JOINT_STEREO, true },
		{ 8, PLM_AUDIO_MODE_DUAL_CHANNEL, false },
		{ 9, PLM_AUDIO_MODE_MONO, true }
	}};

	std::vector<uint8_t> stream;
	for (frame_spec const &frame : frames)
		append_frame(stream, make_silent_layer2_frame(frame.bitrate_index, 0, frame.mode, frame.padding));

	plm_buffer_t *const buffer = plm_buffer_create_with_capacity(stream.size());
	REQUIRE(buffer != nullptr);
	REQUIRE(plm_buffer_write(buffer, stream.data(), stream.size()) == stream.size());
	plm_buffer_signal_end(buffer);

	plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
	REQUIRE(decoder != nullptr);
	REQUIRE(plm_audio_has_header(decoder));

	for (frame_spec const &frame : frames)
	{
		plm_samples_t *const decoded = plm_audio_decode(decoder);
		REQUIRE(decoded != nullptr);
		REQUIRE(decoded->count == PLM_AUDIO_SAMPLES_PER_FRAME);
		REQUIRE(decoder->bitrate_index == int(frame.bitrate_index) - 1);
		REQUIRE(decoder->mode == frame.mode);
		for (float const sample : decoded->interleaved)
			REQUIRE(sample == 0.0F);
	}

	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(plm_audio_has_ended(decoder));
	plm_audio_destroy(decoder);
}

TEST_CASE("CD-i DVC MPEG audio resynchronizes after malformed Layer II candidates", "[emu][philips][dvc][audio]")
{
	std::vector<uint8_t> stream {
		0x31, 0x41, 0x59,
		0xff, 0xfd, 0xf0, 0x00, 0x00, 0x00,
		0x26, 0x53, 0x58
	};
	append_frame(stream, make_silent_layer2_frame(10));

	plm_buffer_t *const buffer = plm_buffer_create_with_capacity(stream.size());
	REQUIRE(buffer != nullptr);
	REQUIRE(plm_buffer_write(buffer, stream.data(), stream.size()) == stream.size());
	plm_buffer_signal_end(buffer);

	plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
	REQUIRE(decoder != nullptr);
	REQUIRE(plm_audio_has_header(decoder));
	REQUIRE(plm_audio_decode(decoder) != nullptr);
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(plm_audio_has_ended(decoder));
	plm_audio_destroy(decoder);
}

TEST_CASE("CD-i DVC MPEG audio waits for an exact frame then refills without duplication", "[emu][philips][dvc][audio]")
{
	std::vector<uint8_t> frame = make_silent_layer2_frame(10);
	plm_buffer_t *const buffer = plm_buffer_create_with_capacity(frame.size());
	REQUIRE(buffer != nullptr);
	REQUIRE(plm_buffer_write(buffer, frame.data(), 3) == 3);

	plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
	REQUIRE(decoder != nullptr);
	REQUIRE_FALSE(plm_audio_has_header(decoder));
	REQUIRE(plm_buffer_write(buffer, frame.data() + 3, 3) == 3);
	REQUIRE(plm_audio_has_header(decoder));
	REQUIRE(plm_audio_decode(decoder) == nullptr);

	std::size_t const penultimate = frame.size() - 7;
	REQUIRE(plm_buffer_write(buffer, frame.data() + 6, penultimate) == penultimate);
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(plm_buffer_write(buffer, frame.data() + frame.size() - 1, 1) == 1);

	plm_samples_t *const decoded = plm_audio_decode(decoder);
	REQUIRE(decoded != nullptr);
	REQUIRE(decoded->count == PLM_AUDIO_SAMPLES_PER_FRAME);
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(buffer->bit_index == buffer->length << 3);

	// Refill after the exact-boundary miss.  Before the cursor fix, the write
	// could see a byte position beyond length and underflow while discarding the
	// consumed prefix.
	std::vector<uint8_t> next_frame = make_silent_layer2_frame(
		11, 0, PLM_AUDIO_MODE_JOINT_STEREO, true);
	std::size_t const split = next_frame.size() / 2;
	REQUIRE(plm_buffer_write(buffer, next_frame.data(), split) == split);
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(plm_buffer_write(
		buffer,
		next_frame.data() + split,
		next_frame.size() - split) == next_frame.size() - split);
	plm_samples_t *const refilled = plm_audio_decode(decoder);
	REQUIRE(refilled != nullptr);
	REQUIRE(refilled->count == PLM_AUDIO_SAMPLES_PER_FRAME);
	REQUIRE(decoder->samples_decoded == 2 * PLM_AUDIO_SAMPLES_PER_FRAME);
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(buffer->bit_index == buffer->length << 3);

	plm_buffer_signal_end(buffer);
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(plm_audio_has_ended(decoder));
	plm_audio_destroy(decoder);
}

TEST_CASE("CD-i DVC MPEG audio never emits a truncated final frame", "[emu][philips][dvc][audio]")
{
	std::vector<uint8_t> frame = make_silent_layer2_frame(10);
	plm_buffer_t *const buffer = plm_buffer_create_with_capacity(frame.size());
	REQUIRE(buffer != nullptr);
	REQUIRE(plm_buffer_write(buffer, frame.data(), frame.size() - 1) == frame.size() - 1);
	plm_buffer_signal_end(buffer);

	plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
	REQUIRE(decoder != nullptr);
	REQUIRE(plm_audio_has_header(decoder));
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(plm_audio_has_ended(decoder));
	plm_audio_destroy(decoder);
}
