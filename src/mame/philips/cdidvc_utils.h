// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_UTILS_H
#define MAME_PHILIPS_CDIDVC_UTILS_H

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cdi_dvc
{

constexpr uint64_t MPEG_TIMESTAMP_MODULUS = uint64_t(1) << 33;
constexpr uint64_t MPEG_TIMESTAMP_MASK = MPEG_TIMESTAMP_MODULUS - 1;
constexpr uint32_t MPEG_SYSTEM_CLOCK_HZ = 90'000;
constexpr uint32_t DCLK_HZ = 45'000;

struct video_command_effects
{
	uint8_t video_buffer;
	bool scroll;
	bool register_update;
	bool swap_buffer;
	bool video_on;
	bool video_off;
	bool hide;
	bool show_immediate;
	bool show_on_next;
};

struct system_command_effects
{
	bool play;
	bool pause;
	bool continue_playback;
	bool step;
	bool stop;
	bool clear_fifo;
	bool decoder_on;
	bool decoder_off;
	bool dma;
};

struct picture_event_reorder_state
{
	uint16_t reference_interrupts;
	bool reference_valid;
};

struct picture_event_reorder_result
{
	picture_event_reorder_state state;
	uint16_t output_interrupts;
	bool output_valid;
};

struct full_motion_picture_rate
{
	uint32_t numerator;
	uint32_t denominator;
};

enum class mpeg1_layer2_audio_header_status : uint8_t
{
	supported,
	accepted_reserved_emphasis,
	unsupported_free_format,
	invalid_sync,
	invalid_version,
	invalid_layer,
	invalid_bitrate,
	invalid_sample_rate
};

struct mpeg1_layer2_audio_header
{
	uint16_t bitrate_kbps;
	uint16_t sample_rate_hz;
	uint16_t frame_size_bytes;
	uint8_t bitrate_index;
	uint8_t sample_rate_index;
	uint8_t channel_mode;
	uint8_t mode_extension;
	uint8_t emphasis;
	bool private_bit;
	bool has_crc;
	bool padding;
	mpeg1_layer2_audio_header_status status;
	bool valid;
};

enum : uint8_t
{
	CDI_LAYER2_PROFILE_INVALID_HEADER = 0x01,
	CDI_LAYER2_PROFILE_BITRATE_CHANNEL = 0x02,
	CDI_LAYER2_PROFILE_SAMPLE_RATE = 0x04,
	CDI_LAYER2_PROFILE_PRIVATE_BIT = 0x08,
	CDI_LAYER2_PROFILE_EMPHASIS = 0x10
};

enum class audio_output_kind : uint8_t
{
	scheduled_silence,
	pcm,
	starvation
};

struct audio_output_frame
{
	int16_t left;
	int16_t right;
	std::size_t pending_frames_before;
	audio_output_kind kind;
	bool drained;
};

// Current scheduler implementation model: when multiple timestamped decoded
// frames are already due at one display opportunity, present the newest due
// frame and consume the older due frames it supersedes.  This helper describes
// emulator queue policy; it is not a claim about an exposed VMPEG FIFO format.
struct presentation_selection
{
	std::size_t selected_index;
	std::size_t consume_count;
	bool valid;
};

enum : uint16_t
{
	FMA_IRQ_END_ISO = 0x0001,
	FMA_IRQ_STREAM_CHANGE = 0x0002,
	FMA_IRQ_FRAME_DECODED = 0x0004,
	FMA_IRQ_UNDERFLOW = 0x0008,
	FMA_IRQ_DECODING_STARTED = 0x0010,
	FMA_IRQ_TIMER = 0x0100
};

enum : uint16_t
{
	FMV_IRQ_SEQUENCE = 0x0001,
	FMV_IRQ_GOP = 0x0002,
	FMV_IRQ_PICTURE = 0x0004,
	FMV_IRQ_END_OF_DATA = 0x0008,
	FMV_IRQ_DCL = 0x0080,
	FMV_IRQ_TIMER = 0x0100,
	FMV_IRQ_END_SEQUENCE = 0x0200,
	FMV_IRQ_END_ISO = 0x0400,
	FMV_IRQ_VSYNC = 0x0800,
	FMV_IRQ_PAUSE = 0x1000,
	FMV_IRQ_CLIP_UPDATE = 0x2000,
	FMV_IRQ_GEOMETRY_LATCH = FMV_IRQ_DCL | FMV_IRQ_CLIP_UPDATE
};

enum : uint16_t
{
	FMV_VDI_DECODING_TIMESTAMP_UPDATED = 0x4000
};

constexpr std::size_t FMV_INPUT_FIFO_HIGH_WATER_BYTES = 28'000;
constexpr std::size_t FMV_OUTPUT_FIFO_PICTURES = 3;
constexpr uint16_t FMV_STATUS_INPUT_READY = 0x2000;

struct presentation_picture_event
{
	uint16_t interrupts;
	bool end_of_data;
};

// Guest-visible picture events belong to the picture that reaches the display
// boundary, not the newest picture decoded ahead of it.
constexpr presentation_picture_event make_presentation_picture_event(
		uint16_t marker_interrupts, bool last_picture_pending,
		uint32_t selected_generation, uint32_t last_picture_generation)
{
	bool const end_of_data = last_picture_pending
		&& selected_generation == last_picture_generation;
	return {
		uint16_t(FMV_IRQ_PICTURE | marker_interrupts
				| (end_of_data ? FMV_IRQ_END_OF_DATA : 0)),
		end_of_data
	};
}

// GEN_PICTURES_IN_FIFO exposes a seven-bit count on VMPEG.  The current MAME
// implementation has decoded-picture queue visibility, so clamp that queue to
// the width of the guest-visible register.
constexpr uint16_t fmv_pictures_in_fifo(std::size_t queued_pictures)
{
	return uint16_t(queued_pictures < 0x7fU ? queued_pictures : 0x7fU);
}

// GEN_DTS exposes bits 21:7 of the 90 kHz MPEG decoding timestamp.  The DVC
// firmware expands this 703.125 Hz view back into its 45 kHz clock domain.
constexpr uint16_t fmv_reduced_decoding_timestamp(uint64_t timestamp90)
{
	return uint16_t((timestamp90 >> 7) & 0x7fffU);
}

// VMPEG stops requesting compressed video input only after the high-water
// mark is exceeded.  This mirrors the strict comparison used by the reference
// implementation and leaves the boundary byte ready for another DMA slice.
constexpr uint16_t fmv_input_status(std::size_t buffered_bytes)
{
	return buffered_bytes > FMV_INPUT_FIFO_HIGH_WATER_BYTES
		? uint16_t(0) : FMV_STATUS_INPUT_READY;
}

// Backend adaptation, not a hardware FIFO rule.
//
// Stock PL_MPEG retains the bytes of an incomplete picture until the next
// picture start code arrives.  A streaming MPEG decoder can consume those
// bytes progressively, so PL_MPEG's retained-byte count cannot be exposed
// directly as guest-visible VMPEG FIFO occupancy while the decoder is
// explicitly waiting for more picture data.
constexpr uint16_t fmv_input_status_from_backend(
		std::size_t retained_bytes, bool decoder_waiting_for_input)
{
	return decoder_waiting_for_input
		? FMV_STATUS_INPUT_READY
		: fmv_input_status(retained_bytes);
}

// GEN_FRAME_PERIOD is expressed in 90 kHz ticks.  PL_MPEG exposes the parsed
// frame rate in millihertz, so round the reciprocal to the nearest tick.
constexpr uint16_t fmv_frame_period_90khz(uint32_t framerate_millihz)
{
	if (!framerate_millihz)
		return 0;

	uint64_t const rounded =
		(90'000'000ULL + framerate_millihz / 2U) / framerate_millihz;
	return uint16_t(rounded < 0xffffU ? rounded : 0xffffU);
}

constexpr uint64_t mpeg_timestamp_normalize(uint64_t value)
{
	return value & MPEG_TIMESTAMP_MASK;
}

// SCR, PTS and DTS use the same five-byte 33-bit timestamp packing pattern in
// MPEG-1 system streams. The high-order tag bits in byte 0 identify the field;
// this helper intentionally decodes only the shared timestamp payload.
constexpr uint64_t decode_mpeg1_timestamp_field(
		uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4)
{
	return (uint64_t(byte0 & 0x0e) << 29)
		| (uint64_t(byte1) << 22)
		| (uint64_t(byte2 & 0xfe) << 14)
		| (uint64_t(byte3) << 7)
		| uint64_t(byte4 >> 1);
}

constexpr uint64_t decode_mpeg1_timestamp_field(std::array<uint8_t, 5> const &field)
{
	return decode_mpeg1_timestamp_field(field[0], field[1], field[2], field[3], field[4]);
}

constexpr bool mpeg1_timestamp_marker_bits_valid(uint8_t byte0, uint8_t byte2, uint8_t byte4)
{
	return (byte0 & 0x01) && (byte2 & 0x01) && (byte4 & 0x01);
}

// Format-validation overload used by measurement-only parser telemetry.  It
// checks the MPEG-1 field tag and marker bits, but callers remain free to decode
// the payload even when this reports false.
constexpr bool mpeg1_timestamp_marker_bits_valid(
		std::array<uint8_t, 5> const &field, uint8_t expected_prefix)
{
	return (field[0] >> 4) == (expected_prefix & 0x0f)
		&& mpeg1_timestamp_marker_bits_valid(field[0], field[2], field[4]);
}

constexpr int64_t mpeg_timestamp_delta(uint64_t lhs, uint64_t rhs)
{
	uint64_t const raw = (lhs - rhs) & MPEG_TIMESTAMP_MASK;
	return (raw & (uint64_t(1) << 32))
		? int64_t(raw) - int64_t(MPEG_TIMESTAMP_MODULUS)
		: int64_t(raw);
}

// Convert a modulo-2^32 subtraction to its signed representative without
// relying on implementation-defined unsigned-to-signed narrowing.
constexpr int32_t signed_wrap_delta32(uint32_t lhs, uint32_t rhs)
{
	uint32_t const raw = lhs - rhs;
	return raw < 0x80000000U
		? int32_t(raw)
		: int32_t(int64_t(raw) - 0x1'0000'0000LL);
}

constexpr int32_t mpeg_dclk_delta(uint64_t timestamp, uint64_t scr)
{
	uint32_t const target45 = uint32_t((timestamp >> 1) & 0xffffffffU);
	uint32_t const scr45 = uint32_t((scr >> 1) & 0xffffffffU);
	return signed_wrap_delta32(target45, scr45);
}

// Anchor an ISO/IEC 11172 90 kHz system clock to the VMPEG 45 kHz DCLK.
// Unsigned DCLK subtraction intentionally preserves the 32-bit wrap behavior.
constexpr uint64_t mpeg_clock_from_dclk(uint64_t anchor90, uint32_t anchor45, uint32_t current45)
{
	uint32_t const elapsed45 = current45 - anchor45;
	return mpeg_timestamp_normalize(anchor90 + uint64_t(elapsed45) * 2U);
}

constexpr bool mpeg_presentation_due(uint64_t timestamp90, uint64_t clock90, int64_t early_tolerance90 = 0)
{
	return mpeg_timestamp_delta(timestamp90, clock90) <= early_tolerance90;
}

constexpr presentation_selection select_latest_due_presentation(
		uint64_t const *timestamps90, std::size_t count, uint64_t clock90,
		int64_t early_tolerance90 = 0)
{
	presentation_selection result { 0, 0, false };
	if (!timestamps90)
		return result;

	for (std::size_t index = 0; index < count; ++index)
	{
		if (!mpeg_presentation_due(timestamps90[index], clock90, early_tolerance90))
			break;

		result.selected_index = index;
		result.consume_count = index + 1;
		result.valid = true;
	}
	return result;
}

constexpr uint64_t dclk_delay_to_samples(int32_t delta45, uint32_t sample_rate)
{
	if (delta45 <= 0 || sample_rate == 0)
		return 0;

	return (uint64_t(uint32_t(delta45)) * sample_rate + (DCLK_HZ - 1U)) / DCLK_HZ;
}

// PL_MPEG emits normalized floating-point samples.  Keep the host-backend
// conversion in one place so its symmetric nearest-integer rounding and
// clipping behavior can be regression-tested.  This is not a claim about the
// VMPEG DSP's internal accumulator or DAC quantization.
constexpr int16_t quantize_plm_audio_sample(float sample)
{
	if (sample != sample) // A decoder NaN must not reach an integer conversion.
		return 0;

	float const scaled = sample * 32767.0F;
	if (scaled <= -32768.0F)
		return -32768;
	if (scaled >= 32767.0F)
		return 32767;

	return int16_t(scaled >= 0.0F
		? int32_t(scaled + 0.5F)
		: int32_t(scaled - 0.5F));
}

// Byte-order-independent FNV-1a over signed 16-bit PCM represented in little
// endian order.  This is diagnostic hashing, not part of the audio signal path.
constexpr uint32_t hash_pcm16_sample(uint32_t hash, int16_t sample)
{
	uint16_t const value = uint16_t(sample);
	hash ^= uint8_t(value & 0xff);
	hash *= 16777619U;
	hash ^= uint8_t(value >> 8);
	hash *= 16777619U;
	return hash;
}

// Decode a complete MPEG-1 Layer II frame header.  This is elementary-stream
// format parsing, not a VMPEG register/hardware claim.  Free-format headers are
// syntactically distinct from the reserved bitrate index, but remain unsupported
// because their frame length cannot be derived from a single header and PL_MPEG
// does not implement the required following-header search.
constexpr mpeg1_layer2_audio_header decode_mpeg1_layer2_audio_header(uint32_t header)
{
	constexpr uint16_t bitrate_kbps[16] =
		{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 };
	constexpr uint16_t sample_rate_hz[4] = { 44'100, 48'000, 32'000, 0 };

	unsigned const sync = (header >> 21) & 0x7ff;
	unsigned const version = (header >> 19) & 0x03;
	unsigned const layer = (header >> 17) & 0x03;
	unsigned const bitrate_index = (header >> 12) & 0x0f;
	unsigned const sample_rate_index = (header >> 10) & 0x03;

	mpeg1_layer2_audio_header result {
		bitrate_kbps[bitrate_index],
		sample_rate_hz[sample_rate_index],
		0,
		uint8_t(bitrate_index),
		uint8_t(sample_rate_index),
		uint8_t((header >> 6) & 0x03),
		uint8_t((header >> 4) & 0x03),
		uint8_t(header & 0x03),
		((header >> 8) & 1U) != 0,
		((header >> 16) & 1U) == 0,
		((header >> 9) & 1U) != 0,
		mpeg1_layer2_audio_header_status::supported,
		false
	};

	if (sync != 0x7ff)
		result.status = mpeg1_layer2_audio_header_status::invalid_sync;
	else if (version != 3)
		result.status = mpeg1_layer2_audio_header_status::invalid_version;
	else if (layer != 2)
		result.status = mpeg1_layer2_audio_header_status::invalid_layer;
	else if (bitrate_index == 0)
		result.status = mpeg1_layer2_audio_header_status::unsupported_free_format;
	else if (bitrate_index == 15)
		result.status = mpeg1_layer2_audio_header_status::invalid_bitrate;
	else if (sample_rate_index == 3)
		result.status = mpeg1_layer2_audio_header_status::invalid_sample_rate;
	else
	{
		result.frame_size_bytes = uint16_t(
			(144'000U * result.bitrate_kbps) / result.sample_rate_hz
			+ (result.padding ? 1U : 0U));
		if (result.emphasis == 2)
			result.status = mpeg1_layer2_audio_header_status::accepted_reserved_emphasis;
		result.valid = true;
	}

	return result;
}

// Green Book IX.5.3.2 constrains conventional MPEG-1 Layer II syntax to the
// CD-i Full Motion profile: a mode-dependent bitrate table, 44.1 kHz only,
// private bit zero, and either no emphasis or 50/15 microsecond emphasis.
// Stereo includes independent, intensity (joint), and dual-channel modes.
// Return independent flags so malformed fixtures cannot hide one violation
// behind another.  This remains separate from syntax parsing because the
// VMPEG response to out-of-profile input is not specified by available evidence.
constexpr uint8_t cdi_full_motion_layer2_profile_violations(
		mpeg1_layer2_audio_header const &header)
{
	if (!header.valid)
		return CDI_LAYER2_PROFILE_INVALID_HEADER;

	bool const mono = header.channel_mode == 3;
	bool const bitrate_permitted = mono
		? header.bitrate_index >= 1 && header.bitrate_index <= 10
		: (header.bitrate_index >= 4 && header.bitrate_index <= 14
			&& header.bitrate_index != 5);
	uint8_t violations = 0;
	if (!bitrate_permitted)
		violations |= CDI_LAYER2_PROFILE_BITRATE_CHANNEL;
	if (header.sample_rate_index != 0)
		violations |= CDI_LAYER2_PROFILE_SAMPLE_RATE;
	if (header.private_bit)
		violations |= CDI_LAYER2_PROFILE_PRIVATE_BIT;
	if (header.emphasis > 1)
		violations |= CDI_LAYER2_PROFILE_EMPHASIS;
	return violations;
}

// ISO/IEC 11172 picture-rate codes permitted by the CD-i Full Motion profile.
// Codes outside the Full Motion set are returned as 0/1 rather than silently
// accepting a rate that the CD-i profile does not require.
constexpr full_motion_picture_rate full_motion_picture_rate_from_code(uint8_t code)
{
	switch (code)
	{
	case 1: return { 24'000, 1'001 }; // 23.976 Hz
	case 2: return { 24, 1 };
	case 3: return { 25, 1 };
	case 4: return { 30'000, 1'001 }; // 29.97 Hz
	case 5: return { 30, 1 };
	default: return { 0, 1 };
	}
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

inline void compact_consumed_audio_samples(std::vector<int16_t> &samples, std::size_t &read)
{
	if (!read)
		return;

	if (read >= samples.size())
	{
		samples.clear();
	}
	else
	{
		samples.erase(samples.begin(), samples.begin() + read);
	}
	read = 0;
}

// Consume exactly one stereo output frame from the current MAME queue model.
// Timestamp-scheduled silence takes priority, a complete stereo pair is never
// split, and an empty/incomplete queue deterministically emits zero without
// consuming data.  This deliberately does not claim VMPEG DAC-edge behavior.
inline audio_output_frame take_audio_output_frame(
		std::vector<int16_t> &samples, std::size_t &read, uint64_t &wait_samples)
{
	std::size_t const pending = read < samples.size()
		? (samples.size() - read) / 2U
		: 0;

	if (wait_samples)
	{
		--wait_samples;
		return { 0, 0, pending, audio_output_kind::scheduled_silence, false };
	}

	if (read >= samples.size() || samples.size() - read < 2U)
		return { 0, 0, pending, audio_output_kind::starvation, false };

	int16_t const left = samples[read++];
	int16_t const right = samples[read++];
	bool const drained = read >= samples.size();
	if (drained)
	{
		samples.clear();
		read = 0;
	}
	return { left, right, pending, audio_output_kind::pcm, drained };
}

constexpr video_command_effects decode_video_command(uint16_t command)
{
	return {
		uint8_t(command & 0x0003),
		bool(command & 0x0004),
		bool(command & 0x0008),
		bool(command & 0x0010),
		bool(command & 0x0020),
		bool(command & 0x0040),
		bool(command & 0x0100),
		bool(command & 0x0200),
		bool(command & 0x0400)
	};
}

constexpr system_command_effects decode_system_command(uint16_t command)
{
	return {
		bool(command & 0x0008),
		bool(command & 0x0010),
		bool(command & 0x0020),
		bool(command & 0x0040),
		bool(command & 0x0080),
		bool(command & 0x0100),
		bool(command & 0x1000),
		bool(command & 0x2000),
		bool(command & 0x8000)
	};
}

constexpr picture_event_reorder_result reorder_picture_events(
		picture_event_reorder_state state, uint8_t picture_type, uint16_t picture_interrupts)
{
	picture_event_reorder_result result { state, 0, false };
	switch (picture_type)
	{
	case 1: // Intra-coded picture.
	case 2: // Predictive-coded picture.
		result.output_interrupts = state.reference_interrupts;
		result.output_valid = state.reference_valid;
		result.state.reference_interrupts = picture_interrupts;
		result.state.reference_valid = true;
		break;

	case 3: // Bidirectionally predictive-coded picture.
		result.output_interrupts = picture_interrupts;
		result.output_valid = true;
		break;

	default:
		break;
	}
	return result;
}

constexpr picture_event_reorder_result flush_picture_events(picture_event_reorder_state state)
{
	return { { 0, false }, state.reference_interrupts, state.reference_valid };
}

} // namespace cdi_dvc

#endif // MAME_PHILIPS_CDIDVC_UTILS_H
