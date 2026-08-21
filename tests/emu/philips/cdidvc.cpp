// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <vector>

#include "catch.hpp"

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
	REQUIRE(cdi_dvc::FMV_IRQ_CLIP_UPDATE == 0x2000);
	REQUIRE(cdi_dvc::FMV_IRQ_GEOMETRY_LATCH == 0x2080);
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

TEST_CASE("CD-i DVC audio queue compaction discards only consumed samples", "[emu][philips][dvc]")
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
