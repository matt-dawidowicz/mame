// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDICDIC_STATE_H
#define MAME_PHILIPS_CDICDIC_STATE_H

#pragma once

#include <array>
#include <cstdint>

namespace cdic_hle
{

enum class command : uint8_t
{
	unknown,
	reset_mode1,
	reset_mode2,
	fetch_toc,
	play_cdda,
	read_mode1,
	read_mode2,
	stop_cdda,
	seek,
	update
};

enum class disc_operation : uint8_t
{
	none,
	mode1,
	mode2,
	cdda,
	toc
};

constexpr bool validates_sector_header(disc_operation operation)
{
	return operation == disc_operation::mode1 ||
		operation == disc_operation::mode2 ||
		operation == disc_operation::toc;
}

constexpr bool discards_invalid_sector(disc_operation operation)
{
	// Mode 2 subheaders control routing and audio format.  Continuing after
	// both raw and descrambled validation fail would interpret untrusted bytes.
	return operation == disc_operation::mode2;
}

enum class disc_state : uint8_t
{
	idle,
	seeking,
	reading
};

struct command_descriptor
{
	command kind;
	disc_operation operation;
	bool starts_read;
	bool stops_immediately;
	bool stops_after_physical_sector;
};

constexpr command_descriptor describe_command(uint16_t value)
{
	switch (value)
	{
	case 0x23: return { command::reset_mode1, disc_operation::none, false, false, true };
	case 0x24: return { command::reset_mode2, disc_operation::none, false, false, true };
	case 0x27: return { command::fetch_toc, disc_operation::toc, true, false, false };
	case 0x28: return { command::play_cdda, disc_operation::cdda, true, false, false };
	case 0x29: return { command::read_mode1, disc_operation::mode1, true, false, false };
	case 0x2a: return { command::read_mode2, disc_operation::mode2, true, false, false };
	case 0x2b: return { command::stop_cdda, disc_operation::none, false, true, false };
	case 0x2c: return { command::seek, disc_operation::mode1, true, false, false };
	case 0x2e: return { command::update, disc_operation::none, false, false, false };
	default:   return { command::unknown, disc_operation::none, false, false, false };
	}
}

constexpr bool stops_after_physical_sector(uint16_t value)
{
	return describe_command(value).stops_after_physical_sector;
}

constexpr bool channel_selected(uint32_t mask, uint8_t channel)
{
	return channel < 32 && bool(mask & (uint32_t(1) << channel));
}

constexpr bool audio_channel_selected(uint16_t mask, uint8_t channel)
{
	return channel < 16 && bool(mask & (uint16_t(1) << channel));
}

enum class xa_coding_status : uint8_t
{
	valid,
	reserved_high_bit,
	reserved_sample_width,
	reserved_sample_rate,
	reserved_channel_mode
};

struct xa_coding
{
	xa_coding_status status;
	uint8_t channels;
	uint8_t bits_per_sample;
	uint16_t clock_divisor;
	uint8_t sector_periods;
	bool emphasis;

	constexpr bool valid() const { return status == xa_coding_status::valid; }
};

constexpr xa_coding decode_xa_coding(uint8_t coding)
{
	const bool emphasis = bool(coding & 0x40);
	if (coding & 0x80)
		return { xa_coding_status::reserved_high_bit, 0, 0, 0, 0, emphasis };

	const uint8_t sample_width = coding & 0x30;
	if (sample_width != 0x00 && sample_width != 0x10)
		return { xa_coding_status::reserved_sample_width, 0, 0, 0, 0, emphasis };

	const uint8_t sample_rate = coding & 0x0c;
	if (sample_rate != 0x00 && sample_rate != 0x04)
		return { xa_coding_status::reserved_sample_rate, 0, 0, 0, 0, emphasis };

	const uint8_t channel_mode = coding & 0x03;
	if (channel_mode != 0x00 && channel_mode != 0x01)
		return { xa_coding_status::reserved_channel_mode, 0, 0, 0, 0, emphasis };

	const uint8_t channels = channel_mode == 0x01 ? 2 : 1;
	const uint8_t bits_per_sample = sample_width == 0x10 ? 8 : 4;
	const uint16_t clock_divisor = sample_rate == 0x04 ? 1024 : 512;
	uint8_t sector_periods = 2;
	if (bits_per_sample == 4)
		sector_periods *= 2;
	if (clock_divisor == 1024)
		sector_periods *= 2;
	if (channels == 1)
		sector_periods *= 2;

	return
	{
		xa_coding_status::valid,
		channels,
		bits_per_sample,
		clock_divisor,
		sector_periods,
		emphasis
	};
}

constexpr uint8_t xa_sector_count(uint8_t coding)
{
	return decode_xa_coding(coding).sector_periods;
}

enum class sector_target : uint8_t
{
	filtered,
	data,
	audio,
	malformed
};

enum class mode2_format_status : uint8_t
{
	valid,
	reserved_channel,
	reserved_audio_channel,
	conflicting_payload_types,
	invalid_payload_form,
	realtime_form1_video,
	invalid_empty_or_message,
	invalid_audio_coding
};

struct mode2_sector
{
	uint8_t file;
	uint8_t channel;
	uint8_t submode;
	uint8_t coding;
};

struct sector_decision
{
	sector_target target;
	mode2_format_status format;
	bool trigger;
	bool end_record;
	bool end_read;
};

constexpr uint8_t SUBMODE_EOF   = 0x80;
constexpr uint8_t SUBMODE_RT    = 0x40;
constexpr uint8_t SUBMODE_FORM  = 0x20;
constexpr uint8_t SUBMODE_TRIG  = 0x10;
constexpr uint8_t SUBMODE_DATA  = 0x08;
constexpr uint8_t SUBMODE_AUDIO = 0x04;
constexpr uint8_t SUBMODE_VIDEO = 0x02;
constexpr uint8_t SUBMODE_EOR   = 0x01;

constexpr mode2_format_status validate_mode2_sector(mode2_sector sector)
{
	if (sector.channel >= 32)
		return mode2_format_status::reserved_channel;

	const uint8_t payload = sector.submode & (SUBMODE_DATA | SUBMODE_AUDIO | SUBMODE_VIDEO);
	if (payload && (payload & (payload - 1)))
		return mode2_format_status::conflicting_payload_types;

	if (!payload)
	{
		// Empty and message sectors have channel and coding information zero.
		return (sector.channel == 0 && sector.coding == 0)
			? mode2_format_status::valid
			: mode2_format_status::invalid_empty_or_message;
	}

	const bool form2 = bool(sector.submode & SUBMODE_FORM);
	if (payload == SUBMODE_AUDIO)
	{
		if (sector.channel >= 16)
			return mode2_format_status::reserved_audio_channel;
		if (!form2)
			return mode2_format_status::invalid_payload_form;
		if (!decode_xa_coding(sector.coding).valid())
			return mode2_format_status::invalid_audio_coding;
	}
	else if (payload == SUBMODE_DATA && form2)
	{
		return mode2_format_status::invalid_payload_form;
	}
	else if (payload == SUBMODE_VIDEO && !form2 && (sector.submode & SUBMODE_RT))
	{
		return mode2_format_status::realtime_form1_video;
	}

	return mode2_format_status::valid;
}

constexpr uint8_t SUBHEADER_MISMATCH_FILE    = 0x01;
constexpr uint8_t SUBHEADER_MISMATCH_CHANNEL = 0x02;
constexpr uint8_t SUBHEADER_MISMATCH_SUBMODE = 0x04;
constexpr uint8_t SUBHEADER_MISMATCH_CODING  = 0x08;

constexpr uint8_t subheader_mismatch(mode2_sector first, mode2_sector second)
{
	return
		(first.file != second.file ? SUBHEADER_MISMATCH_FILE : 0) |
		(first.channel != second.channel ? SUBHEADER_MISMATCH_CHANNEL : 0) |
		(first.submode != second.submode ? SUBHEADER_MISMATCH_SUBMODE : 0) |
		(first.coding != second.coding ? SUBHEADER_MISMATCH_CODING : 0);
}

constexpr sector_decision select_mode2_sector(
		uint16_t file_register,
		uint32_t channel_mask,
		uint16_t audio_channel_mask,
		mode2_sector sector)
{
	const mode2_format_status format = validate_mode2_sector(sector);
	if (format != mode2_format_status::valid)
		return { sector_target::malformed, format, false, false, false };

	if (uint8_t(file_register >> 8) != sector.file)
		return { sector_target::filtered, format, false, false, false };

	const bool selected = channel_selected(channel_mask, sector.channel);
	const bool trigger = bool(sector.submode & SUBMODE_TRIG);
	const bool end_record = selected && bool(sector.submode & SUBMODE_EOR);
	const bool end_read = selected && bool(sector.submode & SUBMODE_EOF);
	const bool applicable = bool(sector.submode & (SUBMODE_DATA | SUBMODE_AUDIO | SUBMODE_VIDEO));
	if (!trigger && !end_record && !end_read && (!selected || !applicable))
		return { sector_target::filtered, format, false, false, false };

	const bool audio =
		selected &&
		bool(sector.submode & SUBMODE_AUDIO) &&
		audio_channel_selected(audio_channel_mask, sector.channel);
	return { audio ? sector_target::audio : sector_target::data, format, trigger, end_record, end_read };
}

struct buffer_completion
{
	uint16_t data_buffer;
	uint16_t byte_offset;
	uint8_t next_data_buffer;
	uint8_t next_audio_buffer;
};

constexpr uint8_t RESET_NEXT_DATA_BUFFER = 1;
constexpr uint8_t RESET_NEXT_AUDIO_BUFFER = 0;

constexpr buffer_completion complete_buffer(
		uint16_t data_buffer,
		bool audio,
		uint8_t next_data_buffer,
		uint8_t next_audio_buffer)
{
	const uint8_t index = (audio ? next_audio_buffer : next_data_buffer) & 1;
	const uint8_t selector = index | (audio ? 4 : 0);
	return
	{
		uint16_t((data_buffer & ~uint16_t(0x0005)) | 0x4000 | selector),
		uint16_t(selector * 0x0a00),
		uint8_t(audio ? next_data_buffer : index ^ 1),
		uint8_t(audio ? index ^ 1 : next_audio_buffer)
	};
}

constexpr bool interrupt_asserted(
		uint16_t x_buffer,
		uint16_t data_buffer,
		uint16_t audio_buffer,
		uint16_t audio_control)
{
	return
		(bool(x_buffer & 0x8000) && bool(data_buffer & 0x4000)) ||
		(bool(audio_buffer & 0x8000) && bool(audio_control & 0x2000));
}

constexpr uint16_t acknowledge_interrupt_source(uint16_t value)
{
	return value & 0x7fff;
}

constexpr uint16_t acknowledge_audio_termination(uint16_t value)
{
	return value & ~uint16_t(0x0001);
}

constexpr uint8_t XA_GROUP_PARAMETER_BYTES = 16;
constexpr uint8_t XA_GROUP_SAMPLES_PER_UNIT = 28;
constexpr uint8_t XA_GROUP_MAX_SOUND_UNITS = 8;

struct xa_group_parameters
{
	std::array<uint8_t, XA_GROUP_MAX_SOUND_UNITS> value{};
	uint8_t unit_count = 0;
	uint8_t copy_mismatch = 0;
	uint8_t reserved_filter = 0;
	uint8_t reserved_range = 0;
	bool supported_sample_width = false;

	constexpr bool valid() const
	{
		return supported_sample_width && !copy_mismatch && !reserved_filter && !reserved_range;
	}
};

constexpr uint8_t xa_parameter_copy_count(uint8_t bits_per_sample)
{
	return bits_per_sample == 8 ? 4 : bits_per_sample == 4 ? 2 : 0;
}

constexpr uint8_t xa_sound_unit_count(uint8_t bits_per_sample)
{
	return bits_per_sample == 8 ? 4 : bits_per_sample == 4 ? 8 : 0;
}

constexpr uint8_t xa_parameter_copy_offset(uint8_t bits_per_sample, uint8_t unit, uint8_t copy)
{
	if (bits_per_sample == 8)
		return unit + copy * 4;
	if (bits_per_sample == 4)
		return (unit & 3) + (unit & 4 ? 8 : 0) + copy * 4;
	return 0;
}

constexpr uint8_t xa_maximum_range(uint8_t bits_per_sample)
{
	return bits_per_sample == 8 ? 8 : bits_per_sample == 4 ? 12 : 0;
}

constexpr xa_group_parameters inspect_xa_group_parameters(const uint8_t *data, uint8_t bits_per_sample)
{
	xa_group_parameters result;
	result.unit_count = xa_sound_unit_count(bits_per_sample);
	const uint8_t copy_count = xa_parameter_copy_count(bits_per_sample);
	result.supported_sample_width = bool(result.unit_count && copy_count);
	if (!result.supported_sample_width)
		return result;

	const uint8_t maximum_range = xa_maximum_range(bits_per_sample);
	for (uint8_t unit = 0; unit < result.unit_count; unit++)
	{
		const uint8_t mask = uint8_t(1U << unit);
		const uint8_t first = data[xa_parameter_copy_offset(bits_per_sample, unit, 0)];
		result.value[unit] = first;
		for (uint8_t copy = 0; copy < copy_count; copy++)
		{
			const uint8_t parameter = data[xa_parameter_copy_offset(bits_per_sample, unit, copy)];
			if (parameter != first)
				result.copy_mismatch |= mask;
			if ((parameter >> 4) > 3)
				result.reserved_filter |= mask;
			if ((parameter & 0x0f) > maximum_range)
				result.reserved_range |= mask;
		}
	}

	return result;
}

constexpr int16_t clip_sample(int32_t sample)
{
	return int16_t(sample < -32768 ? -32768 : sample > 32767 ? 32767 : sample);
}

constexpr int16_t expand_xa_code(uint8_t code, uint8_t bits_per_sample)
{
	if (bits_per_sample != 4 && bits_per_sample != 8)
		return 0;
	const uint16_t code_count = uint16_t(1U << bits_per_sample);
	const uint8_t value = code & uint8_t(code_count - 1);
	const int32_t signed_value = value & (code_count >> 1) ? int32_t(value) - code_count : value;
	return int16_t(signed_value * (uint32_t(1) << (16 - bits_per_sample)));
}

struct xa_sample
{
	int16_t output;
	int16_t recent;
	int16_t older;
};

constexpr xa_sample decode_xa_sample(
		uint8_t parameter,
		int16_t encoded,
		int16_t recent,
		int16_t older)
{
	// Callers validate the sound parameter before reaching this arithmetic.
	// These coefficients are the Green Book values scaled by 256.  The final
	// +128 and shift are an independently corroborated compatibility rounding
	// model; exact CDIC intermediate widths and rounding remain unmeasured.
	constexpr int16_t FILTER[4][2] =
	{
		{ 0x000,  0x000 },
		{ 0x0f0,  0x000 },
		{ 0x1cc, -0x0d0 },
		{ 0x188, -0x0dc }
	};
	const uint8_t filter = (parameter >> 4) & 3;
	const uint8_t range = parameter & 0x0f;
	const int32_t decoded =
		(int32_t(encoded) >> range) +
		((int32_t(FILTER[filter][0]) * recent + int32_t(FILTER[filter][1]) * older + 128) >> 8);
	const int16_t output = clip_sample(decoded);
	return { output, output, recent };
}

struct xa_group_decode_result
{
	xa_group_parameters parameters;
	uint16_t samples_per_channel;

	constexpr bool valid() const { return samples_per_channel && parameters.valid(); }
};

constexpr void reset_xa_history(int16_t *history)
{
	for (uint8_t i = 0; i < 4; i++)
		history[i] = 0;
}

inline xa_group_decode_result decode_xa_group(
		uint8_t bits_per_sample,
		uint8_t channels,
		const uint8_t *data,
		int16_t *history,
		int16_t *left,
		int16_t *right)
{
	const xa_group_parameters parameters = inspect_xa_group_parameters(data, bits_per_sample);
	const uint16_t samples_per_channel =
		(channels == 1 || channels == 2)
			? uint16_t(parameters.unit_count * XA_GROUP_SAMPLES_PER_UNIT / channels)
			: 0;
	const xa_group_decode_result result{ parameters, samples_per_channel };
	if (!samples_per_channel)
		return result;

	// Raw-image reads do not expose the CIRC reliability needed to choose a
	// contradictory copy.  Preserve the group duration without allowing an
	// untrusted parameter to contaminate predictor history.  This is MAME's
	// explicit concealment policy, not a claim about physical CDIC silicon.
	if (!parameters.valid())
	{
		for (uint16_t sample = 0; sample < samples_per_channel; sample++)
		{
			left[sample] = 0;
			if (channels == 2)
				right[sample] = 0;
		}
		return result;
	}

	for (uint8_t unit = 0; unit < parameters.unit_count; unit++)
	{
		const uint8_t channel = channels == 2 ? unit & 1 : 0;
		int16_t *const output = channel ? right : left;
		const uint16_t output_offset = uint16_t(unit / channels) * XA_GROUP_SAMPLES_PER_UNIT;
		int16_t &recent = history[channel * 2];
		int16_t &older = history[channel * 2 + 1];

		for (uint8_t sample = 0; sample < XA_GROUP_SAMPLES_PER_UNIT; sample++)
		{
			int16_t encoded;
			if (bits_per_sample == 8)
			{
				const uint8_t value = data[XA_GROUP_PARAMETER_BYTES + unit + sample * 4];
				encoded = expand_xa_code(value, 8);
			}
			else
			{
				const uint8_t value = data[XA_GROUP_PARAMETER_BYTES + (unit >> 1) + sample * 4];
				const uint8_t nibble = (value >> ((unit & 1) * 4)) & 0x0f;
				encoded = expand_xa_code(nibble, 4);
			}

			const xa_sample decoded = decode_xa_sample(parameters.value[unit], encoded, recent, older);
			recent = decoded.recent;
			older = decoded.older;
			output[output_offset + sample] = decoded.output;
		}
	}

	return result;
}

constexpr uint32_t lba_from_time(uint32_t time)
{
	const uint8_t bcd_mins = uint8_t(time >> 24);
	const uint8_t mins = uint8_t((bcd_mins >> 4) * 10 + (bcd_mins & 0x0f));
	const uint8_t bcd_secs = uint8_t(time >> 16);
	const uint8_t secs = uint8_t((bcd_secs >> 4) * 10 + (bcd_secs & 0x0f));
	const uint8_t bcd_frac = uint8_t(time >> 8);

	uint32_t lba = (uint32_t(mins) * 60 + secs) * 75;
	// Firmware uses fraction bit 7 to request a whole-second seek.  This is
	// strongly inferred from software and retained as an HLE command rule.
	if (!(bcd_frac & 0x80))
		lba += uint32_t(bcd_frac >> 4) * 10 + (bcd_frac & 0x0f);

	return lba >= 150 ? lba - 150 : lba;
}

} // namespace cdic_hle

#endif // MAME_PHILIPS_CDICDIC_STATE_H
