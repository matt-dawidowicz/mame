// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDICDIC_STATE_H
#define MAME_PHILIPS_CDICDIC_STATE_H

#pragma once

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

enum class sector_target : uint8_t
{
	filtered,
	data,
	audio
};

struct mode2_sector
{
	uint8_t file;
	uint8_t channel;
	uint8_t submode;
};

struct sector_decision
{
	sector_target target;
	bool end_read;
};

constexpr uint8_t SUBMODE_EOF   = 0x80;
constexpr uint8_t SUBMODE_FORM  = 0x20;
constexpr uint8_t SUBMODE_TRIG  = 0x10;
constexpr uint8_t SUBMODE_DATA  = 0x08;
constexpr uint8_t SUBMODE_AUDIO = 0x04;
constexpr uint8_t SUBMODE_VIDEO = 0x02;
constexpr uint8_t SUBMODE_EOR   = 0x01;

constexpr sector_decision select_mode2_sector(
		uint16_t file_register,
		uint32_t channel_mask,
		uint16_t audio_channel_mask,
		mode2_sector sector)
{
	if (uint8_t(file_register >> 8) != sector.file)
		return { sector_target::filtered, false };

	const bool terminal = bool(sector.submode & (SUBMODE_EOF | SUBMODE_TRIG | SUBMODE_EOR));
	const bool applicable = bool(sector.submode & (SUBMODE_DATA | SUBMODE_AUDIO | SUBMODE_VIDEO));
	if (!terminal && (!applicable || !channel_selected(channel_mask, sector.channel)))
		return { sector_target::filtered, false };

	const bool audio =
		bool(sector.submode & SUBMODE_FORM) &&
		bool(sector.submode & SUBMODE_AUDIO) &&
		audio_channel_selected(audio_channel_mask, sector.channel);
	return { audio ? sector_target::audio : sector_target::data, bool(sector.submode & SUBMODE_EOF) };
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

constexpr int16_t clip_sample(int32_t sample)
{
	return int16_t(sample < -32768 ? -32768 : sample > 32767 ? 32767 : sample);
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
	constexpr int16_t FILTER[4][2] =
	{
		{ 0x000,  0x000 },
		{ 0x0f0,  0x000 },
		{ 0x1cc, -0x0d0 },
		{ 0x188, -0x0dc }
	};
	const uint8_t filter = (parameter >> 4) & 3;
	const uint8_t range = (parameter & 0x0f) > 12 ? 12 : parameter & 0x0f;
	const int32_t decoded =
		(int32_t(encoded) >> range) +
		((int32_t(FILTER[filter][0]) * recent + int32_t(FILTER[filter][1]) * older + 128) >> 8);
	const int16_t output = clip_sample(decoded);
	return { output, output, recent };
}

constexpr uint8_t xa_sector_count(uint8_t coding)
{
	const uint8_t channel_mode = coding & 0x03;
	const uint8_t sample_rate = coding & 0x0c;
	const uint8_t bits_per_sample = coding & 0x30;
	if (bool(coding & 0x80)
			|| channel_mode > 1
			|| (sample_rate != 0x00 && sample_rate != 0x04)
			|| (bits_per_sample != 0x00 && bits_per_sample != 0x10))
	{
		return 0;
	}

	uint8_t count = 2;
	if (bits_per_sample == 0x00)
		count *= 2;
	if (sample_rate == 0x04)
		count *= 2;
	if (channel_mode == 0x00)
		count *= 2;
	return count;
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
