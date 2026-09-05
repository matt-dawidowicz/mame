// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdicdic_state.h"

namespace {

int32_t floor_divide_by_64(int32_t value)
{
	return value >= 0 ? value / 64 : -int32_t((uint32_t(-value) + 63) / 64);
}

cdic_hle::xa_sample reference_xa_sample(
		uint8_t parameter,
		int16_t encoded,
		int16_t recent,
		int16_t older)
{
	// FFmpeg's independent fixed-point XA decoder uses these exact Green Book
	// coefficients scaled by 64 and adds 32 before its arithmetic shift.
	static constexpr int16_t FILTER[4][2] =
	{
		{   0,   0 },
		{  60,   0 },
		{ 115, -52 },
		{  98, -55 }
	};
	const uint8_t filter = parameter >> 4;
	const uint8_t range = parameter & 0x0f;
	const int32_t predictor = floor_divide_by_64(
		int32_t(FILTER[filter][0]) * recent + int32_t(FILTER[filter][1]) * older + 32);
	const int32_t decoded = (int32_t(encoded) >> range) + predictor;
	const int16_t output = int16_t(decoded < -32768 ? -32768 : decoded > 32767 ? 32767 : decoded);
	return { output, output, recent };
}

void write_reference_group_parameters(
		std::array<uint8_t, 128> &group,
		uint8_t bits_per_sample,
		const std::array<uint8_t, 8> &parameters)
{
	const uint8_t units = bits_per_sample == 8 ? 4 : 8;
	const uint8_t copies = bits_per_sample == 8 ? 4 : 2;
	for (uint8_t unit = 0; unit < units; unit++)
	{
		const uint8_t base = bits_per_sample == 8
			? unit
			: uint8_t((unit < 4 ? 0 : 8) + (unit & 3));
		for (uint8_t copy = 0; copy < copies; copy++)
			group[base + copy * 4] = parameters[unit];
	}
}

uint16_t reference_decode_xa_group(
		uint8_t bits_per_sample,
		uint8_t channels,
		const std::array<uint8_t, 128> &group,
		std::array<int16_t, 4> &history,
		std::array<int16_t, 225> &left,
		std::array<int16_t, 225> &right)
{
	const uint8_t units = bits_per_sample == 8 ? 4 : 8;
	for (uint8_t unit = 0; unit < units; unit++)
	{
		const uint8_t channel = channels == 2 ? unit & 1 : 0;
		const uint16_t output_offset = uint16_t(unit / channels) * 28;
		const uint8_t parameter_offset = bits_per_sample == 8
			? uint8_t(12 + unit)
			: uint8_t((unit < 4 ? 4 : 12) + (unit & 3));
		const uint8_t parameter = group[parameter_offset];

		for (uint8_t sample = 0; sample < 28; sample++)
		{
			const uint8_t packed = bits_per_sample == 8
				? group[16 + unit + sample * 4]
				: group[16 + (unit / 2) + sample * 4];
			const uint8_t code = bits_per_sample == 8
				? packed
				: uint8_t((packed >> ((unit & 1) * 4)) & 0x0f);
			const int32_t signed_code = bits_per_sample == 8
				? (code >= 128 ? int32_t(code) - 256 : code)
				: (code >= 8 ? int32_t(code) - 16 : code);
			const int16_t encoded = int16_t(signed_code * (bits_per_sample == 8 ? 256 : 4096));
			const cdic_hle::xa_sample decoded = reference_xa_sample(
				parameter, encoded, history[channel * 2], history[channel * 2 + 1]);
			history[channel * 2] = decoded.recent;
			history[channel * 2 + 1] = decoded.older;
			(channel ? right : left)[output_offset + sample] = decoded.output;
		}
	}

	return uint16_t(units * 28 / channels);
}

cdic_hle::mode2_format_status reference_mode2_format(cdic_hle::mode2_sector sector)
{
	using cdic_hle::mode2_format_status;

	if (sector.channel >= 32)
		return mode2_format_status::reserved_channel;

	const uint8_t payload = sector.submode & 0x0e;
	if (payload != 0 && (payload & (payload - 1)) != 0)
		return mode2_format_status::conflicting_payload_types;

	if (payload == 0)
	{
		return (sector.channel == 0 && sector.coding == 0)
			? mode2_format_status::valid
			: mode2_format_status::invalid_empty_or_message;
	}

	const bool form2 = bool(sector.submode & 0x20);
	if (payload == 0x04)
	{
		if (sector.channel >= 16)
			return mode2_format_status::reserved_audio_channel;
		if (!form2)
			return mode2_format_status::invalid_payload_form;
		const bool valid_coding =
			!(sector.coding & 0x80) &&
			((sector.coding & 0x30) == 0x00 || (sector.coding & 0x30) == 0x10) &&
			((sector.coding & 0x0c) == 0x00 || (sector.coding & 0x0c) == 0x04) &&
			((sector.coding & 0x03) == 0x00 || (sector.coding & 0x03) == 0x01);
		if (!valid_coding)
			return mode2_format_status::invalid_audio_coding;
	}
	else if (payload == 0x08 && form2)
	{
		return mode2_format_status::invalid_payload_form;
	}
	else if (payload == 0x02 && !form2 && (sector.submode & 0x40))
	{
		return mode2_format_status::realtime_form1_video;
	}

	return mode2_format_status::valid;
}

struct reference_sector_decision
{
	cdic_hle::sector_target target;
	cdic_hle::mode2_format_status format;
	bool trigger;
	bool end_record;
	bool end_read;
};

reference_sector_decision reference_select_mode2_sector(
		uint16_t file_register,
		uint32_t channel_mask,
		uint16_t audio_channel_mask,
		cdic_hle::mode2_sector sector)
{
	using cdic_hle::mode2_format_status;
	using cdic_hle::sector_target;

	const mode2_format_status format = reference_mode2_format(sector);
	if (format != mode2_format_status::valid)
		return { sector_target::malformed, format, false, false, false };

	if (uint8_t(file_register >> 8) != sector.file)
		return { sector_target::filtered, format, false, false, false };

	const bool selected = sector.channel < 32 && bool(channel_mask & (uint32_t(1) << sector.channel));
	const bool trigger = bool(sector.submode & 0x10);
	const bool end_record = selected && bool(sector.submode & 0x01);
	const bool end_read = selected && bool(sector.submode & 0x80);
	const bool applicable = bool(sector.submode & 0x0e);
	if (!trigger && !end_record && !end_read && (!selected || !applicable))
		return { sector_target::filtered, format, false, false, false };

	const bool audio =
		selected &&
		bool(sector.submode & 0x04) &&
		sector.channel < 16 &&
		bool(audio_channel_mask & (uint16_t(1) << sector.channel));
	return { audio ? sector_target::audio : sector_target::data, format, trigger, end_record, end_read };
}

} // anonymous namespace

TEST_CASE("CDIC command map separates accepted transport effects", "[emu][philips][cdic][command]")
{
	using cdic_hle::command;
	using cdic_hle::describe_command;
	using cdic_hle::disc_operation;

	REQUIRE(describe_command(0x23).kind == command::reset_mode1);
	REQUIRE(describe_command(0x23).stops_after_physical_sector);
	REQUIRE_FALSE(describe_command(0x23).starts_read);
	REQUIRE(describe_command(0x24).kind == command::reset_mode2);
	REQUIRE(describe_command(0x27).operation == disc_operation::toc);
	REQUIRE(describe_command(0x28).operation == disc_operation::cdda);
	REQUIRE_FALSE(cdic_hle::validates_sector_header(describe_command(0x28).operation));
	REQUIRE_FALSE(cdic_hle::discards_invalid_sector(describe_command(0x28).operation));
	REQUIRE(describe_command(0x29).operation == disc_operation::mode1);
	REQUIRE(cdic_hle::validates_sector_header(describe_command(0x29).operation));
	REQUIRE_FALSE(cdic_hle::discards_invalid_sector(describe_command(0x29).operation));
	REQUIRE(describe_command(0x2a).operation == disc_operation::mode2);
	REQUIRE(cdic_hle::validates_sector_header(describe_command(0x2a).operation));
	REQUIRE(cdic_hle::discards_invalid_sector(describe_command(0x2a).operation));
	REQUIRE(describe_command(0x2b).stops_immediately);
	REQUIRE(describe_command(0x2c).operation == disc_operation::mode1);
	REQUIRE(describe_command(0x2e).kind == command::update);
	REQUIRE(describe_command(0xffff).kind == command::unknown);
	REQUIRE_FALSE(describe_command(0xffff).starts_read);
}

TEST_CASE("CDIC Mode 2 filtering keeps physical movement separate from visible delivery", "[emu][philips][cdic][sector][filter]")
{
	using cdic_hle::mode2_sector;
	using cdic_hle::sector_target;
	using cdic_hle::select_mode2_sector;

	constexpr uint16_t file = 0x1200;
	constexpr uint32_t channels = uint32_t(1) << 5;
	constexpr uint16_t audio_channels = uint16_t(1) << 5;

	auto result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x13, 5, 0x08, 0x00 });
	REQUIRE(result.target == sector_target::filtered);
	REQUIRE_FALSE(result.end_read);

	result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x12, 0, 0x00, 0x00 });
	REQUIRE(result.target == sector_target::filtered);

	result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x12, 5, 0x08, 0x00 });
	REQUIRE(result.target == sector_target::data);

	result = select_mode2_sector(file, 0, audio_channels, mode2_sector{ 0x12, 5, 0x08, 0x00 });
	REQUIRE(result.target == sector_target::filtered);

	result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x12, 5, 0x24, 0x00 });
	REQUIRE(result.target == sector_target::audio);

	result = select_mode2_sector(file, channels, 0, mode2_sector{ 0x12, 5, 0x24, 0x00 });
	REQUIRE(result.target == sector_target::data);

	result = select_mode2_sector(file, 0, 0, mode2_sector{ 0x12, 31, 0x88, 0x00 });
	REQUIRE(result.target == sector_target::filtered);
	REQUIRE_FALSE(result.end_read);

	result = select_mode2_sector(file, 0, 0, mode2_sector{ 0x12, 31, 0x98, 0x00 });
	REQUIRE(result.target == sector_target::data);
	REQUIRE(result.trigger);
	REQUIRE_FALSE(result.end_read);

	result = select_mode2_sector(file, channels, 0, mode2_sector{ 0x12, 5, 0x89, 0x00 });
	REQUIRE(result.target == sector_target::data);
	REQUIRE(result.end_record);
	REQUIRE(result.end_read);

	result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x12, 5, 0x04, 0x00 });
	REQUIRE(result.target == sector_target::malformed);
	REQUIRE(result.format == cdic_hle::mode2_format_status::invalid_payload_form);

	REQUIRE_FALSE(cdic_hle::channel_selected(0xffffffff, 32));
	REQUIRE_FALSE(cdic_hle::audio_channel_selected(0xffff, 16));
}

TEST_CASE("CDIC Mode 2 format validation classifies every Green Book sector boundary", "[emu][philips][cdic][sector][malformed]")
{
	using cdic_hle::mode2_format_status;
	using cdic_hle::mode2_sector;
	using cdic_hle::validate_mode2_sector;

	REQUIRE(validate_mode2_sector({ 0, 32, 0x08, 0x00 }) == mode2_format_status::reserved_channel);
	REQUIRE(validate_mode2_sector({ 0, 16, 0x24, 0x00 }) == mode2_format_status::reserved_audio_channel);
	REQUIRE(validate_mode2_sector({ 0, 0, 0x0c, 0x00 }) == mode2_format_status::conflicting_payload_types);
	REQUIRE(validate_mode2_sector({ 0, 0, 0x04, 0x00 }) == mode2_format_status::invalid_payload_form);
	REQUIRE(validate_mode2_sector({ 0, 0, 0x28, 0x00 }) == mode2_format_status::invalid_payload_form);
	REQUIRE(validate_mode2_sector({ 0, 0, 0x42, 0x00 }) == mode2_format_status::realtime_form1_video);
	REQUIRE(validate_mode2_sector({ 0, 1, 0x00, 0x00 }) == mode2_format_status::invalid_empty_or_message);
	REQUIRE(validate_mode2_sector({ 0, 0, 0x20, 0x01 }) == mode2_format_status::invalid_empty_or_message);
	REQUIRE(validate_mode2_sector({ 0, 0, 0x24, 0x80 }) == mode2_format_status::invalid_audio_coding);

	REQUIRE(validate_mode2_sector({ 0xff, 0, 0x00, 0x00 }) == mode2_format_status::valid);
	REQUIRE(validate_mode2_sector({ 0, 0, 0x20, 0x00 }) == mode2_format_status::valid);
	REQUIRE(validate_mode2_sector({ 0, 31, 0x02, 0xff }) == mode2_format_status::valid);
	REQUIRE(validate_mode2_sector({ 0, 31, 0x62, 0xff }) == mode2_format_status::valid);
	REQUIRE(validate_mode2_sector({ 0, 15, 0x24, 0x14 }) == mode2_format_status::valid);
}

TEST_CASE("CDIC Mode 2 routing exhausts file channel submode and mask combinations", "[emu][philips][cdic][sector][filter][exhaustive]")
{
	constexpr uint16_t FILE_REGISTER = 0x5a00;
	uint64_t combinations = 0;

	for (uint16_t file = 0; file <= 0xff; file++)
	{
		for (uint16_t channel = 0; channel <= 0xff; channel++)
		{
			const uint32_t selected_channel = channel < 32 ? uint32_t(1) << channel : 0;
			const uint16_t selected_audio = channel < 16 ? uint16_t(uint16_t(1) << channel) : 0;
			for (uint16_t submode = 0; submode <= 0xff; submode++)
			{
				const cdic_hle::mode2_sector sector =
					{ uint8_t(file), uint8_t(channel), uint8_t(submode), 0x00 };
				for (uint8_t selection = 0; selection < 4; selection++)
				{
					const uint32_t channel_mask = (selection & 1) ? selected_channel : 0;
					const uint16_t audio_mask = (selection & 2) ? selected_audio : 0;
					const cdic_hle::sector_decision actual =
						cdic_hle::select_mode2_sector(FILE_REGISTER, channel_mask, audio_mask, sector);
					const reference_sector_decision expected =
						reference_select_mode2_sector(FILE_REGISTER, channel_mask, audio_mask, sector);
					combinations++;

					if (actual.target != expected.target ||
						actual.format != expected.format ||
						actual.trigger != expected.trigger ||
						actual.end_record != expected.end_record ||
						actual.end_read != expected.end_read)
					{
						INFO("file " << file << ", channel " << channel << ", submode " << submode
							<< ", selection " << unsigned(selection) << ", channel mask " << channel_mask
							<< ", audio mask " << audio_mask);
						REQUIRE(actual.target == expected.target);
						REQUIRE(actual.format == expected.format);
						REQUIRE(actual.trigger == expected.trigger);
						REQUIRE(actual.end_record == expected.end_record);
						REQUIRE(actual.end_read == expected.end_read);
					}
				}
			}
		}
	}

	REQUIRE(combinations == uint64_t(256) * 256 * 256 * 4);
}

TEST_CASE("CDIC Mode 2 duplicated subheaders reject every contradictory byte", "[emu][philips][cdic][sector][malformed][exhaustive]")
{
	using cdic_hle::mode2_sector;
	using cdic_hle::subheader_mismatch;

	for (uint16_t base = 0; base <= 0xff; base++)
	{
		const uint8_t value = uint8_t(base);
		const mode2_sector first = { value, value, value, value };
		REQUIRE(subheader_mismatch(first, first) == 0);

		for (uint16_t alternate = 0; alternate <= 0xff; alternate++)
		{
			if (alternate == base)
				continue;
			const uint8_t changed = uint8_t(alternate);
			INFO("base " << base << ", alternate " << alternate);
			REQUIRE(subheader_mismatch(first, { changed, value, value, value }) == cdic_hle::SUBHEADER_MISMATCH_FILE);
			REQUIRE(subheader_mismatch(first, { value, changed, value, value }) == cdic_hle::SUBHEADER_MISMATCH_CHANNEL);
			REQUIRE(subheader_mismatch(first, { value, value, changed, value }) == cdic_hle::SUBHEADER_MISMATCH_SUBMODE);
			REQUIRE(subheader_mismatch(first, { value, value, value, changed }) == cdic_hle::SUBHEADER_MISMATCH_CODING);
		}
	}

	REQUIRE(subheader_mismatch({ 0, 0, 0, 0 }, { 1, 1, 1, 1 }) == 0x0f);
}

TEST_CASE("CDIC data and audio delivery use independent double-buffer sequences", "[emu][philips][cdic][buffer]")
{
	uint8_t next_data = 1;
	uint8_t next_audio = 0;
	uint16_t dbuf = 0;

	auto completion = cdic_hle::complete_buffer(dbuf, false, next_data, next_audio);
	REQUIRE(completion.byte_offset == 0x0a00);
	REQUIRE((completion.data_buffer & 0x0005) == 0x0001);
	REQUIRE(bool(completion.data_buffer & 0x4000));
	next_data = completion.next_data_buffer;
	next_audio = completion.next_audio_buffer;
	dbuf = completion.data_buffer;

	completion = cdic_hle::complete_buffer(dbuf, true, next_data, next_audio);
	REQUIRE(completion.byte_offset == 0x2800);
	REQUIRE((completion.data_buffer & 0x0005) == 0x0004);
	next_data = completion.next_data_buffer;
	next_audio = completion.next_audio_buffer;
	dbuf = completion.data_buffer;

	completion = cdic_hle::complete_buffer(dbuf, false, next_data, next_audio);
	REQUIRE(completion.byte_offset == 0x0000);
	REQUIRE((completion.data_buffer & 0x0005) == 0x0000);
	next_data = completion.next_data_buffer;
	next_audio = completion.next_audio_buffer;
	dbuf = completion.data_buffer;

	completion = cdic_hle::complete_buffer(dbuf, true, next_data, next_audio);
	REQUIRE(completion.byte_offset == 0x3200);
	REQUIRE((completion.data_buffer & 0x0005) == 0x0005);
}

TEST_CASE("CDIC Mode 2 filters latch and replacement reads restart audio at buffer 2800", "[emu][philips][cdic][audio][buffer][filter][restart][hardware][exhaustive]")
{
	cdic_hle::mode2_filter_state filters;
	cdic_hle::latch_mode2_filters(filters, 0x1200, uint32_t(1) << 3, uint16_t(1) << 3);
	REQUIRE(cdic_hle::select_mode2_sector(
		filters.file, filters.channels, filters.audio_channels,
		{ 0x12, 3, 0x24, 0x00 }).target == cdic_hle::sector_target::audio);

	// Programming-register writes are not a routing boundary.  The active
	// filter remains unchanged until command $2e (or a new Mode-2 read) latches it.
	const uint16_t programmed_file = 0x3400;
	const uint32_t programmed_channels = uint32_t(1) << 7;
	const uint16_t programmed_audio_channels = uint16_t(1) << 7;
	REQUIRE(cdic_hle::select_mode2_sector(
		filters.file, filters.channels, filters.audio_channels,
		{ 0x34, 7, 0x24, 0x00 }).target == cdic_hle::sector_target::filtered);

	cdic_hle::latch_mode2_filters(
		filters, programmed_file, programmed_channels, programmed_audio_channels);
	REQUIRE(cdic_hle::select_mode2_sector(
		filters.file, filters.channels, filters.audio_channels,
		{ 0x12, 3, 0x24, 0x00 }).target == cdic_hle::sector_target::filtered);
	REQUIRE(cdic_hle::select_mode2_sector(
		filters.file, filters.channels, filters.audio_channels,
		{ 0x34, 7, 0x24, 0x00 }).target == cdic_hle::sector_target::audio);

	cdic_hle::realtime_audio_state update_audio;
	update_audio.ready = { true, false };
	update_audio.next_play = 1;
	update_audio.periods_remaining = 7;
	update_audio.enabled = true;
	uint8_t update_delivery = 1;
	cdic_hle::apply_mode2_filter_boundary(
		filters, update_audio, update_delivery,
		cdic_hle::mode2_filter_boundary::update,
		0x5600, uint32_t(1) << 9, uint16_t(1) << 9);
	REQUIRE(filters.file == 0x5600);
	REQUIRE(filters.channels == uint32_t(1) << 9);
	REQUIRE(filters.audio_channels == uint16_t(1) << 9);
	REQUIRE(update_audio.ready[0]);
	REQUIRE_FALSE(update_audio.ready[1]);
	REQUIRE(update_audio.next_play == 1);
	REQUIRE(update_audio.periods_remaining == 7);
	REQUIRE(update_audio.enabled);
	REQUIRE(update_delivery == 1);

	for (const bool enabled : { false, true })
	{
		for (uint8_t ready = 0; ready < 4; ready++)
		{
			for (uint8_t next_play = 0; next_play < 2; next_play++)
			{
				for (uint8_t periods = 0; periods <= 16; periods++)
				{
					for (uint8_t initial_delivery = 0; initial_delivery < 2; initial_delivery++)
					{
						cdic_hle::realtime_audio_state audio;
						audio.enabled = enabled;
						audio.ready = { bool(ready & 1), bool(ready & 2) };
						audio.next_play = next_play;
						audio.periods_remaining = periods;
						uint8_t next_delivery = initial_delivery;
						cdic_hle::apply_mode2_filter_boundary(
							filters, audio, next_delivery,
							cdic_hle::mode2_filter_boundary::new_read,
							0x7800, uint32_t(1) << 11, uint16_t(1) << 11);

						INFO("enabled=" << enabled << " ready=" << unsigned(ready)
							<< " next_play=" << unsigned(next_play)
							<< " periods=" << unsigned(periods));
						REQUIRE(audio.enabled == enabled);
						REQUIRE_FALSE(audio.ready[0]);
						REQUIRE_FALSE(audio.ready[1]);
						REQUIRE(audio.next_play == 0);
						REQUIRE(audio.periods_remaining == 0);
						REQUIRE(next_delivery == cdic_hle::RESET_NEXT_AUDIO_BUFFER);
						REQUIRE(filters.file == 0x7800);
						REQUIRE(filters.channels == uint32_t(1) << 11);
						REQUIRE(filters.audio_channels == uint16_t(1) << 11);
					}
				}
			}
		}
	}
}

TEST_CASE("CDIC interrupt sources are gated by their documented enable state", "[emu][philips][cdic][irq]")
{
	using cdic_hle::interrupt_asserted;

	REQUIRE_FALSE(interrupt_asserted(0, 0, 0, 0));
	REQUIRE_FALSE(interrupt_asserted(0x8000, 0, 0, 0));
	REQUIRE(interrupt_asserted(0x8000, 0x4000, 0, 0));
	REQUIRE_FALSE(interrupt_asserted(0, 0, 0x8000, 0));
	REQUIRE(interrupt_asserted(0, 0, 0x8000, 0x2000));
	REQUIRE(interrupt_asserted(0x8000, 0x4000, 0x8000, 0x2000));
}

TEST_CASE("CDIC reset and status acknowledgements return to an idle non-IRQ state", "[emu][philips][cdic][reset][irq]")
{
	REQUIRE(cdic_hle::RESET_NEXT_DATA_BUFFER == 1);
	REQUIRE(cdic_hle::RESET_NEXT_AUDIO_BUFFER == 0);
	REQUIRE(cdic_hle::acknowledge_interrupt_source(0xffff) == 0x7fff);
	REQUIRE(cdic_hle::acknowledge_interrupt_source(0x1234) == 0x1234);
	REQUIRE(cdic_hle::acknowledge_audio_termination(0xffff) == 0xfffe);
	REQUIRE(cdic_hle::acknowledge_audio_termination(0x1234) == 0x1234);
	REQUIRE_FALSE(cdic_hle::interrupt_asserted(0, 0, 0, 0));
}

TEST_CASE("CDIC AUDCTL write and source selection exhaust the register space", "[emu][philips][cdic][audio][audctl][hardware][exhaustive]")
{
	using cdic_hle::audio_control_action;

	for (const uint16_t latched : { uint16_t(0), cdic_hle::AUDCTL_TERMINATED })
	{
		const uint16_t current = uint16_t(0xa5a0 | latched);
		for (uint32_t raw = 0; raw <= 0xffff; raw++)
		{
			const uint16_t data = uint16_t(raw);
			const uint16_t merged = cdic_hle::merge_audio_control(current, data, 0xffff);
			INFO("latched=" << latched << " data=" << data);
			REQUIRE(merged == uint16_t(cdic_hle::AUDCTL_WRITTEN_READBACK |
				(data & cdic_hle::AUDCTL_WRITABLE) | latched));

			for (const bool active : { false, true })
			{
				audio_control_action expected;
				if (!(data & cdic_hle::AUDCTL_PLAY))
					expected = audio_control_action::stop;
				else if (active)
					expected = audio_control_action::none;
				else if (data & cdic_hle::AUDCTL_AUDIO_IRQ)
					expected = audio_control_action::start_sound_map;
				else
					expected = audio_control_action::start_realtime;
				REQUIRE(cdic_hle::classify_audio_control(data, active) == expected);
				REQUIRE(cdic_hle::classify_audio_control(merged, active) == expected);
			}
		}
	}

	REQUIRE(cdic_hle::AUDCTL_RESET_READBACK == 0xc7fe);
	REQUIRE(cdic_hle::merge_audio_control(0, 0x0000, 0xffff) == 0xd7fe);
	REQUIRE(cdic_hle::merge_audio_control(0, 0x0800, 0xffff) == 0xdffe);
	REQUIRE(cdic_hle::merge_audio_control(0, 0x2800, 0xffff) == 0xfffe);
	REQUIRE(cdic_hle::merge_audio_control(0xd7ff, 0x2800, 0xffff) == 0xffff);
	REQUIRE(cdic_hle::merge_audio_control(0xa5a1, 0x0000, 0x00ff) == 0xf7ff);
	REQUIRE(cdic_hle::merge_audio_control(0xa5a0, 0x5a00, 0xff00) == 0xdffe);
	REQUIRE(cdic_hle::classify_audio_control(0x2000, false) == audio_control_action::stop);
	REQUIRE(cdic_hle::classify_audio_control(0x0800, false) == audio_control_action::start_realtime);
	REQUIRE(cdic_hle::classify_audio_control(0x2800, false) == audio_control_action::start_sound_map);
}

TEST_CASE("CDIC sound-map timing reproduces start completion abort and FF edges", "[emu][philips][cdic][audio][audctl][buffer][timing][hardware]")
{
	using cdic_hle::audio_map_tick_action;
	constexpr uint8_t CODING = 0x04;
	const uint8_t periods = cdic_hle::xa_sector_count(CODING);
	REQUIRE(periods == 16);

	cdic_hle::audio_map_state state;
	REQUIRE_FALSE(cdic_hle::start_audio_map(state, 0x2000));
	REQUIRE(cdic_hle::start_audio_map(state, 0x2800));
	cdic_hle::request_audio_map_stop(state);
	REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::abort_before_buffer);
	REQUIRE_FALSE(state.active);

	state = {};
	// The hardware's post-$2800-write readback is $fffe.  Fixed read-one bits
	// must not move the first map away from its measured $2800 buffer.
	REQUIRE(cdic_hle::start_audio_map(state, 0xfffe));
	REQUIRE(state.active);
	REQUIRE(state.next_address == cdic_hle::AUDIO_MAP_FIRST_ADDRESS);
	REQUIRE(state.next_address == 0x2800);
	REQUIRE(state.periods_remaining == 1);
	REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::consume_buffer);

	auto consumed = cdic_hle::consume_audio_map_buffer(state, CODING);
	REQUIRE_FALSE(consumed.previous_buffer_complete);
	REQUIRE_FALSE(consumed.terminated);
	REQUIRE(consumed.coding_valid);
	REQUIRE(state.next_address == 0x3200);
	for (uint8_t tick = 1; tick < periods; tick++)
		REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::none);
	REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::consume_buffer);

	consumed = cdic_hle::consume_audio_map_buffer(state, CODING);
	REQUIRE(consumed.previous_buffer_complete);
	REQUIRE(state.next_address == 0x2800);
	cdic_hle::request_audio_map_stop(state);
	REQUIRE(state.active);
	REQUIRE(state.stop_requested);
	for (uint8_t tick = 1; tick < periods; tick++)
		REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::none);
	REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::abort_complete);
	REQUIRE_FALSE(state.active);
	REQUIRE_FALSE(state.stop_requested);
	REQUIRE(state.next_address == 0xffff);

	state = {};
	REQUIRE(cdic_hle::start_audio_map(state, 0x2800));
	REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::consume_buffer);
	consumed = cdic_hle::consume_audio_map_buffer(state, CODING);
	for (uint8_t tick = 0; tick < periods; tick++)
	{
		const audio_map_tick_action expected = tick + 1 == periods
			? audio_map_tick_action::consume_buffer
			: audio_map_tick_action::none;
		REQUIRE(cdic_hle::advance_audio_map(state) == expected);
	}
	consumed = cdic_hle::consume_audio_map_buffer(state, 0xff);
	REQUIRE(consumed.previous_buffer_complete);
	REQUIRE(consumed.terminated);
	REQUIRE_FALSE(state.active);
	REQUIRE(state.periods_remaining == 0);

	// A replacement map starts from its one-tick priming interval; the ended
	// map does not leave a fictitious full-sector delay behind.
	REQUIRE(cdic_hle::start_audio_map(state, 0x2800));
	REQUIRE(state.periods_remaining == 1);
	REQUIRE(cdic_hle::advance_audio_map(state) == audio_map_tick_action::consume_buffer);
	consumed = cdic_hle::consume_audio_map_buffer(state, 0xff);
	REQUIRE_FALSE(consumed.previous_buffer_complete);
	REQUIRE(consumed.terminated);
}

TEST_CASE("CDIC realtime audio double buffer waits refills and resumes deterministically", "[emu][philips][cdic][audio][audctl][buffer][starvation][hardware]")
{
	cdic_hle::realtime_audio_state state;
	cdic_hle::mark_realtime_audio_ready(state, 0);
	REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == cdic_hle::NO_AUDIO_BUFFER);

	cdic_hle::start_realtime_audio(state);
	REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == 0);
	cdic_hle::begin_realtime_audio_buffer(state, 4);
	cdic_hle::mark_realtime_audio_ready(state, 1);
	for (uint8_t tick = 0; tick < 3; tick++)
	{
		REQUIRE_FALSE(cdic_hle::advance_realtime_audio(state));
		REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == cdic_hle::NO_AUDIO_BUFFER);
	}
	REQUIRE(cdic_hle::advance_realtime_audio(state));
	REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == 1);

	// Starvation holds the expected half without duplicating the prior one;
	// a later refill becomes consumable immediately.
	REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == cdic_hle::NO_AUDIO_BUFFER);
	cdic_hle::mark_realtime_audio_ready(state, 0);
	REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == 0);
	cdic_hle::begin_realtime_audio_buffer(state, 8);
	cdic_hle::mark_realtime_audio_ready(state, 1);
	cdic_hle::stop_realtime_audio(state);
	REQUIRE_FALSE(state.enabled);
	REQUIRE(state.periods_remaining == 0);
	REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == cdic_hle::NO_AUDIO_BUFFER);
	cdic_hle::start_realtime_audio(state);
	REQUIRE(cdic_hle::take_realtime_audio_buffer(state) == 1);

	cdic_hle::reset_realtime_audio_buffers(state);
	REQUIRE(state.enabled);
	REQUIRE(state.next_play == 0);
	REQUIRE_FALSE(state.ready[0]);
	REQUIRE_FALSE(state.ready[1]);
}

TEST_CASE("CDIC CDDA receive gating models first pre-start sector retention", "[emu][philips][cdic][audio][cdda][audctl][buffer][model][exhaustive]")
{
	using cdic_hle::cdda_receive_action;

	for (const bool enabled : { false, true })
	{
		for (const bool sound_map_active : { false, true })
		{
			for (const bool pending : { false, true })
			{
				cdic_hle::realtime_audio_state state;
				state.enabled = enabled;
				const cdda_receive_action expected = enabled && !sound_map_active
					? cdda_receive_action::play
					: (pending ? cdda_receive_action::retain_buffered : cdda_receive_action::buffer);
				INFO("enabled=" << enabled << " map=" << sound_map_active << " pending=" << pending);
				REQUIRE(cdic_hle::classify_cdda_receive(state, sound_map_active, pending) == expected);
			}
		}
	}
}

TEST_CASE("CDIC command time converts BCD MSF and whole-second seeks to logical LBA", "[emu][philips][cdic][command][sector]")
{
	REQUIRE(cdic_hle::lba_from_time(0x00020000) == 0);
	REQUIRE(cdic_hle::lba_from_time(0x00020100) == 1);
	REQUIRE(cdic_hle::lba_from_time(0x00027400) == 74);
	REQUIRE(cdic_hle::lba_from_time(0x00028000) == 0);
	REQUIRE(cdic_hle::lba_from_time(0x01000000) == 4350);
	REQUIRE(cdic_hle::lba_from_time(0x01012300) == 4448);
}

TEST_CASE("CDIC XA predictor vectors preserve history and clip final PCM", "[emu][philips][cdic][xa][decode]")
{
	auto decoded = cdic_hle::decode_xa_sample(0x00, 0x1000, 0, 0);
	REQUIRE(decoded.output == 0x1000);
	REQUIRE(decoded.recent == 0x1000);
	REQUIRE(decoded.older == 0);

	decoded = cdic_hle::decode_xa_sample(0x10, 0, 1000, -500);
	REQUIRE(decoded.output == 938);
	REQUIRE(decoded.recent == 938);
	REQUIRE(decoded.older == 1000);

	decoded = cdic_hle::decode_xa_sample(0x20, 0x7fff, 30000, -30000);
	REQUIRE(decoded.output == 32767);

	decoded = cdic_hle::decode_xa_sample(0x30, int16_t(0x8000), -32768, 32767);
	REQUIRE(decoded.output == -32768);

	std::array<int16_t, 4> history{ 1, -1, 32767, -32768 };
	cdic_hle::reset_xa_history(history.data());
	const std::array<int16_t, 4> zero_history{};
	REQUIRE(history == zero_history);
}

TEST_CASE("CDIC XA predictor exhausts every legal filter range and code", "[emu][philips][cdic][xa][decode][exhaustive]")
{
	static constexpr std::array<int16_t, 9> HISTORY =
	{
		-32768, -32767, -30000, -1, 0, 1, 30000, 32766, 32767
	};
	for (uint16_t raw = 0; raw <= 0xff; raw++)
	{
		const uint8_t code = uint8_t(raw);
		const int16_t expected_8bit = int16_t((raw >= 128 ? int32_t(raw) - 256 : raw) * 256);
		const uint8_t nibble = code & 0x0f;
		const int16_t expected_4bit = int16_t((nibble >= 8 ? int32_t(nibble) - 16 : nibble) * 4096);
		REQUIRE(cdic_hle::expand_xa_code(code, 8) == expected_8bit);
		REQUIRE(cdic_hle::expand_xa_code(code, 4) == expected_4bit);
	}
	REQUIRE(cdic_hle::expand_xa_code(0xff, 16) == 0);

	for (const uint8_t bits_per_sample : { uint8_t(4), uint8_t(8) })
	{
		const uint8_t maximum_range = bits_per_sample == 8 ? 8 : 12;
		const uint16_t code_count = bits_per_sample == 8 ? 256 : 16;
		for (uint8_t filter = 0; filter < 4; filter++)
		{
			for (uint8_t range = 0; range <= maximum_range; range++)
			{
				const uint8_t parameter = uint8_t((filter << 4) | range);
				for (uint16_t code = 0; code < code_count; code++)
				{
					const int32_t signed_code = code >= (code_count / 2) ? int32_t(code) - code_count : code;
					const int16_t encoded = int16_t(signed_code * (bits_per_sample == 8 ? 256 : 4096));
					for (const int16_t recent : HISTORY)
					{
						for (const int16_t older : HISTORY)
						{
							const cdic_hle::xa_sample expected = reference_xa_sample(parameter, encoded, recent, older);
							const cdic_hle::xa_sample actual = cdic_hle::decode_xa_sample(parameter, encoded, recent, older);
							INFO("width " << unsigned(bits_per_sample) << ", filter " << unsigned(filter)
								<< ", range " << unsigned(range) << ", code " << code
								<< ", history " << recent << ", " << older);
							REQUIRE(actual.output == expected.output);
							REQUIRE(actual.recent == expected.recent);
							REQUIRE(actual.older == expected.older);
						}
					}
				}
			}
		}
	}
}

TEST_CASE("CDIC XA sound-group parameter selection matches Mono-I captures", "[emu][philips][cdic][xa][group][malformed][hardware][exhaustive]")
{
	for (const uint8_t bits_per_sample : { uint8_t(4), uint8_t(8) })
	{
		const uint8_t units = bits_per_sample == 8 ? 4 : 8;
		const uint8_t copies = bits_per_sample == 8 ? 4 : 2;
		const uint8_t valid_mask = uint8_t((1U << units) - 1);
		const uint8_t maximum_range = bits_per_sample == 8 ? 8 : 12;
		std::array<uint8_t, 128> group{};
		std::array<uint8_t, 8> parameters{};
		for (uint8_t unit = 0; unit < units; unit++)
			parameters[unit] = uint8_t(((unit & 3) << 4) | (unit % (maximum_range + 1)));
		write_reference_group_parameters(group, bits_per_sample, parameters);

		const cdic_hle::xa_group_parameters valid = cdic_hle::inspect_xa_group_parameters(group.data(), bits_per_sample);
		REQUIRE(valid.valid());
		REQUIRE(valid.supported_sample_width);
		REQUIRE(valid.unit_count == units);
		REQUIRE(valid.copy_mismatch == 0);
		REQUIRE(valid.reserved_filter == 0);
		REQUIRE(valid.reserved_range == 0);
		REQUIRE(valid.redundant_reserved_filter == 0);
		REQUIRE(valid.redundant_reserved_range == 0);
		for (uint8_t unit = 0; unit < units; unit++)
		{
			REQUIRE(valid.value[unit] == parameters[unit]);
			const uint8_t expected_offset = bits_per_sample == 8
				? uint8_t(12 + unit)
				: uint8_t((unit < 4 ? 4 : 12) + (unit & 3));
			REQUIRE(cdic_hle::xa_parameter_copy_offset(bits_per_sample, unit, copies - 1) == expected_offset);
		}

		for (uint8_t unit = 0; unit < units; unit++)
		{
			for (uint8_t copy = 0; copy < copies; copy++)
			{
				std::array<uint8_t, 128> contradictory = group;
				const uint8_t offset = bits_per_sample == 8
					? uint8_t(unit + copy * 4)
					: uint8_t((unit < 4 ? 0 : 8) + (unit & 3) + copy * 4);
				contradictory[offset] ^= 1;
				const cdic_hle::xa_group_parameters inspected =
					cdic_hle::inspect_xa_group_parameters(contradictory.data(), bits_per_sample);
				INFO("width " << unsigned(bits_per_sample) << ", unit " << unsigned(unit)
					<< ", copy " << unsigned(copy));
				REQUIRE(inspected.valid());
				REQUIRE(inspected.copy_mismatch == uint8_t(1U << unit));
				REQUIRE(inspected.value[unit] == (copy == copies - 1
					? uint8_t(parameters[unit] ^ 1)
					: parameters[unit]));
			}
		}

		for (uint16_t raw = 0; raw <= 0xff; raw++)
		{
			parameters.fill(uint8_t(raw));
			write_reference_group_parameters(group, bits_per_sample, parameters);
			const cdic_hle::xa_group_parameters inspected =
				cdic_hle::inspect_xa_group_parameters(group.data(), bits_per_sample);
			INFO("width " << unsigned(bits_per_sample) << ", parameter " << raw);
			REQUIRE(inspected.copy_mismatch == 0);
			REQUIRE(inspected.reserved_filter == ((raw >> 4) > 3 ? valid_mask : 0));
			REQUIRE(inspected.reserved_range == ((raw & 0x0f) > maximum_range ? valid_mask : 0));
			REQUIRE(inspected.redundant_reserved_filter == ((raw >> 4) > 3 ? valid_mask : 0));
			REQUIRE(inspected.redundant_reserved_range == ((raw & 0x0f) > maximum_range ? valid_mask : 0));
			REQUIRE(inspected.valid() == ((raw >> 4) <= 3 && (raw & 0x0f) <= maximum_range));
		}

		// Exercise every byte value independently in every redundant position.
		parameters.fill(0);
		for (uint8_t unit = 0; unit < units; unit++)
		{
			for (uint8_t copy = 0; copy < copies; copy++)
			{
				for (uint16_t raw = 0; raw <= 0xff; raw++)
				{
					write_reference_group_parameters(group, bits_per_sample, parameters);
					group[cdic_hle::xa_parameter_copy_offset(bits_per_sample, unit, copy)] = uint8_t(raw);
					const cdic_hle::xa_group_parameters inspected =
						cdic_hle::inspect_xa_group_parameters(group.data(), bits_per_sample);
					const uint8_t mask = uint8_t(1U << unit);
					const bool selected = copy == copies - 1;
					const bool bad_filter = (raw >> 4) > 3;
					const bool bad_range = (raw & 0x0f) > maximum_range;

					INFO("width " << unsigned(bits_per_sample) << ", unit " << unsigned(unit)
						<< ", copy " << unsigned(copy) << ", value " << raw);
					REQUIRE(inspected.copy_mismatch == (raw ? mask : 0));
					REQUIRE(inspected.value[unit] == (selected ? raw : 0));
					REQUIRE(inspected.reserved_filter == (selected && bad_filter ? mask : 0));
					REQUIRE(inspected.reserved_range == (selected && bad_range ? mask : 0));
					REQUIRE(inspected.redundant_reserved_filter == (!selected && bad_filter ? mask : 0));
					REQUIRE(inspected.redundant_reserved_range == (!selected && bad_range ? mask : 0));
					REQUIRE(inspected.valid() == (!selected || (!bad_filter && !bad_range)));
				}
			}
		}
	}

	std::array<uint8_t, 128> compound_group{};
	std::array<uint8_t, 8> compound_parameters{};
	write_reference_group_parameters(compound_group, 4, compound_parameters);
	compound_group[6] = 0x4d;
	compound_parameters[5] = 0x3d;
	compound_parameters[7] = 0x40;
	write_reference_group_parameters(compound_group, 4, compound_parameters);
	compound_group[6] = 0x4d;
	const cdic_hle::xa_group_parameters compound =
		cdic_hle::inspect_xa_group_parameters(compound_group.data(), 4);
	REQUIRE_FALSE(compound.valid());
	REQUIRE(compound.copy_mismatch == 0x04);
	REQUIRE(compound.reserved_filter == 0x84);
	REQUIRE(compound.reserved_range == 0x24);
	REQUIRE(compound.redundant_reserved_filter == 0x80);
	REQUIRE(compound.redundant_reserved_range == 0x20);

	std::array<uint8_t, 128> group{};
	for (uint8_t i = 16; i < group.size(); i++)
		group[i] = uint8_t(i * 37 + 11);
	std::array<int16_t, 225> clean_left{};
	std::array<int16_t, 225> clean_right{};
	std::array<int16_t, 225> mismatch_left{};
	std::array<int16_t, 225> mismatch_right{};
	std::array<int16_t, 4> clean_history{ 1234, -2345, -3000, 4000 };
	std::array<int16_t, 4> mismatch_history = clean_history;
	const cdic_hle::xa_group_decode_result clean = cdic_hle::decode_xa_group(
		8, 2, group.data(), clean_history.data(), clean_left.data(), clean_right.data());
	group[0] = 0x31;
	const cdic_hle::xa_group_decode_result mismatch = cdic_hle::decode_xa_group(
		8, 2, group.data(), mismatch_history.data(), mismatch_left.data(), mismatch_right.data());
	REQUIRE(clean.valid());
	REQUIRE(mismatch.valid());
	REQUIRE(mismatch.parameters.copy_mismatch == 0x01);
	REQUIRE(mismatch_left == clean_left);
	REQUIRE(mismatch_right == clean_right);
	REQUIRE(mismatch_history == clean_history);

	group.fill(0);
	std::array<int16_t, 225> left;
	std::array<int16_t, 225> right;
	left.fill(0x5555);
	right.fill(0x5555);
	std::array<int16_t, 4> history{ 1234, -2345, -3000, 4000 };
	const std::array<int16_t, 4> original_history = history;
	group[12] = 0x40;
	const cdic_hle::xa_group_decode_result malformed = cdic_hle::decode_xa_group(
		8, 2, group.data(), history.data(), left.data(), right.data());
	REQUIRE_FALSE(malformed.valid());
	REQUIRE(malformed.parameters.copy_mismatch == 0x01);
	REQUIRE(malformed.parameters.reserved_filter == 0x01);
	REQUIRE(malformed.samples_per_channel == 56);
	REQUIRE(history == original_history);
	for (uint16_t sample = 0; sample < malformed.samples_per_channel; sample++)
	{
		REQUIRE(left[sample] == 0);
		REQUIRE(right[sample] == 0);
	}
	REQUIRE(left[malformed.samples_per_channel] == 0x5555);
	REQUIRE(right[malformed.samples_per_channel] == 0x5555);

	const cdic_hle::xa_group_parameters unsupported = cdic_hle::inspect_xa_group_parameters(group.data(), 16);
	REQUIRE_FALSE(unsupported.valid());
	REQUIRE_FALSE(unsupported.supported_sample_width);
	REQUIRE(unsupported.unit_count == 0);
}

TEST_CASE("CDIC XA sound groups preserve channel order and history in every PCM mode", "[emu][philips][cdic][xa][group][decode]")
{
	static constexpr std::array<uint8_t, 8> CODINGS =
	{
		0x00, 0x04, 0x01, 0x05, 0x10, 0x14, 0x11, 0x15
	};

	for (const uint8_t coding : CODINGS)
	{
		const cdic_hle::xa_coding format = cdic_hle::decode_xa_coding(coding);
		REQUIRE(format.valid());
		std::array<uint8_t, 128> group{};
		for (uint16_t i = 16; i < group.size(); i++)
			group[i] = uint8_t(i * 73 + 41);

		std::array<uint8_t, 8> parameters{};
		const uint8_t units = format.bits_per_sample == 8 ? 4 : 8;
		const uint8_t maximum_range = format.bits_per_sample == 8 ? 8 : 12;
		for (uint8_t unit = 0; unit < units; unit++)
			parameters[unit] = uint8_t(((unit & 3) << 4) | ((unit * 3 + 1) % (maximum_range + 1)));
		write_reference_group_parameters(group, format.bits_per_sample, parameters);

		std::array<int16_t, 4> actual_history{ 1234, -2345, -3000, 4000 };
		std::array<int16_t, 4> expected_history = actual_history;
		for (uint8_t pass = 0; pass < 2; pass++)
		{
			std::array<int16_t, 225> actual_left;
			std::array<int16_t, 225> actual_right;
			std::array<int16_t, 225> expected_left;
			std::array<int16_t, 225> expected_right;
			actual_left.fill(0x5555);
			actual_right.fill(0x5555);
			expected_left.fill(0x5555);
			expected_right.fill(0x5555);

			const uint16_t expected_samples = reference_decode_xa_group(
				format.bits_per_sample, format.channels, group,
				expected_history, expected_left, expected_right);
			const cdic_hle::xa_group_decode_result actual = cdic_hle::decode_xa_group(
				format.bits_per_sample, format.channels, group.data(), actual_history.data(),
				actual_left.data(), actual_right.data());
			INFO("coding " << unsigned(coding) << ", pass " << unsigned(pass));
			REQUIRE(actual.valid());
			REQUIRE(actual.samples_per_channel == expected_samples);
			REQUIRE(actual.parameters.unit_count == units);
			for (uint16_t sample = 0; sample < expected_samples; sample++)
			{
				REQUIRE(actual_left[sample] == expected_left[sample]);
				if (format.channels == 2)
					REQUIRE(actual_right[sample] == expected_right[sample]);
			}
			REQUIRE(actual_left[expected_samples] == 0x5555);
			REQUIRE(actual_right[expected_samples] == 0x5555);
			REQUIRE(actual_history == expected_history);
		}
	}
}

TEST_CASE("CDIC XA PCM is bit-identical to the retained FFmpeg reference", "[emu][philips][cdic][xa][decode][reference]")
{
	struct landmark
	{
		uint16_t index;
		int16_t sample;
	};
	static constexpr std::array<landmark, 15> FFMPEG_LANDMARKS =
	{{
		{    0, -14336 }, {    1, -1280 }, {    2, -6144 }, {    3, -1968 },
		{   31,  -3022 }, {  127,   353 }, {  511,  8912 }, { 1023,  -911 },
		{ 2015,     87 }, { 4031,  2048 }, { 4032, -22485 }, { 4095,  -768 },
		{ 6000, -18743 }, { 8062,    64 }, { 8063,   472 }
	}};

	std::array<int16_t, 4> history{};
	std::array<int16_t, 8064> pcm{};
	uint16_t pcm_index = 0;
	uint64_t fnv = UINT64_C(14695981039346656037);
	for (uint8_t sector = 0; sector < 2; sector++)
	{
		for (uint8_t group_index = 0; group_index < 18; group_index++)
		{
			std::array<uint8_t, 128> group{};
			std::array<uint8_t, 8> parameters{};
			for (uint8_t unit = 0; unit < 8; unit++)
			{
				parameters[unit] = uint8_t(
					(((unit + group_index + sector) & 3) << 4) |
					((unit * 3 + group_index * 5 + sector * 7 + 1) % 13));
			}
			write_reference_group_parameters(group, 4, parameters);
			for (uint8_t i = 16; i < 128; i++)
				group[i] = uint8_t(sector * 101 + group_index * 29 + i * 73 + 41);

			std::array<int16_t, 224> left{};
			std::array<int16_t, 224> right{};
			const cdic_hle::xa_group_decode_result decoded = cdic_hle::decode_xa_group(
				4, 2, group.data(), history.data(), left.data(), right.data());
			REQUIRE(decoded.valid());
			REQUIRE(decoded.samples_per_channel == 112);
			for (uint16_t sample = 0; sample < decoded.samples_per_channel; sample++)
			{
				for (const int16_t value : { left[sample], right[sample] })
				{
					pcm[pcm_index++] = value;
					for (uint8_t byte = 0; byte < 2; byte++)
					{
						fnv ^= uint8_t(uint16_t(value) >> (byte * 8));
						fnv *= UINT64_C(1099511628211);
					}
				}
			}
		}
	}

	REQUIRE(pcm_index == pcm.size());
	REQUIRE(fnv == UINT64_C(0x19f5a1a69dbe9187));
	for (const landmark &expected : FFMPEG_LANDMARKS)
	{
		INFO("interleaved sample " << expected.index);
		REQUIRE(pcm[expected.index] == expected.sample);
	}
}

TEST_CASE("CDIC XA coding exhausts all supported and reserved byte combinations", "[emu][philips][cdic][xa][coding][exhaustive]")
{
	using cdic_hle::xa_coding_status;

	uint16_t valid_count = 0;
	for (uint16_t value = 0; value <= 0xff; value++)
	{
		const uint8_t coding = uint8_t(value);
		const cdic_hle::xa_coding decoded = cdic_hle::decode_xa_coding(coding);
		const bool valid =
			!(coding & 0x80) &&
			((coding & 0x30) == 0x00 || (coding & 0x30) == 0x10) &&
			((coding & 0x0c) == 0x00 || (coding & 0x0c) == 0x04) &&
			((coding & 0x03) == 0x00 || (coding & 0x03) == 0x01);

		INFO("coding byte " << unsigned(coding));
		REQUIRE(decoded.valid() == valid);
		REQUIRE(decoded.emphasis == bool(coding & 0x40));

		if (valid)
		{
			valid_count++;
			const uint8_t channels = (coding & 0x03) == 0x01 ? 2 : 1;
			const uint8_t bits = (coding & 0x30) == 0x10 ? 8 : 4;
			const uint16_t divisor = (coding & 0x0c) == 0x04 ? 1024 : 512;
			uint8_t periods = 2;
			if (bits == 4)
				periods *= 2;
			if (divisor == 1024)
				periods *= 2;
			if (channels == 1)
				periods *= 2;

			REQUIRE(decoded.status == xa_coding_status::valid);
			REQUIRE(decoded.channels == channels);
			REQUIRE(decoded.bits_per_sample == bits);
			REQUIRE(decoded.clock_divisor == divisor);
			REQUIRE(decoded.sector_periods == periods);
			REQUIRE(cdic_hle::xa_sector_count(coding) == periods);
		}
		else
		{
			xa_coding_status expected_status = xa_coding_status::reserved_channel_mode;
			if (coding & 0x80)
				expected_status = xa_coding_status::reserved_high_bit;
			else if ((coding & 0x30) != 0x00 && (coding & 0x30) != 0x10)
				expected_status = xa_coding_status::reserved_sample_width;
			else if ((coding & 0x0c) != 0x00 && (coding & 0x0c) != 0x04)
				expected_status = xa_coding_status::reserved_sample_rate;

			REQUIRE(decoded.status == expected_status);
			REQUIRE(decoded.channels == 0);
			REQUIRE(decoded.bits_per_sample == 0);
			REQUIRE(decoded.clock_divisor == 0);
			REQUIRE(decoded.sector_periods == 0);
			REQUIRE(cdic_hle::xa_sector_count(coding) == 0);
		}
	}

	REQUIRE(valid_count == 16);
}

TEST_CASE("CDIC XA and CDDA sector durations are exact rational clock identities", "[emu][philips][cdic][audio][timing][exhaustive]")
{
	constexpr uint32_t CLOCK2 = 45'158'400U * 3 / 7;
	REQUIRE(CLOCK2 == 19'353'600);
	REQUIRE(CLOCK2 / 512 == 37'800);
	REQUIRE(CLOCK2 / 1024 == 18'900);

	uint16_t valid_count = 0;
	for (uint16_t raw = 0; raw <= 0xff; raw++)
	{
		const cdic_hle::xa_coding coding = cdic_hle::decode_xa_coding(uint8_t(raw));
		const uint16_t samples = cdic_hle::xa_samples_per_sector_per_channel(coding);
		const uint32_t rate = cdic_hle::xa_sample_rate(coding, CLOCK2);
		INFO("coding=" << raw);
		if (coding.valid())
		{
			valid_count++;
			REQUIRE(samples != 0);
			REQUIRE(rate != 0);
			REQUIRE(uint32_t(samples) * 75 == rate * coding.sector_periods);
		}
		else
		{
			REQUIRE(samples == 0);
			REQUIRE(rate == 0);
		}
	}

	REQUIRE(valid_count == 16);
	REQUIRE(588 * 75 == 44'100);
	REQUIRE(cdic_hle::CDIC_SUBCODE_BYTE_OFFSET == (2352 - 12));
	REQUIRE_FALSE(cdic_hle::stores_sector_payload_in_ram(cdic_hle::disc_operation::cdda));
	REQUIRE(cdic_hle::stores_sector_payload_in_ram(cdic_hle::disc_operation::mode1));
	REQUIRE(cdic_hle::stores_sector_payload_in_ram(cdic_hle::disc_operation::mode2));
}

TEST_CASE("CDIC XA sample clocks do not accumulate drift over thirty minutes", "[emu][philips][cdic][audio][timing][longrun][exhaustive]")
{
	constexpr uint32_t CLOCK2 = 45'158'400U * 3 / 7;
	constexpr uint64_t SECTOR_TICKS = 30ULL * 60 * 75;
	uint16_t valid_count = 0;

	for (uint16_t raw = 0; raw <= 0xff; raw++)
	{
		const cdic_hle::xa_coding coding = cdic_hle::decode_xa_coding(uint8_t(raw));
		if (!coding.valid())
			continue;

		valid_count++;
		const uint64_t sample_rate = cdic_hle::xa_sample_rate(coding, CLOCK2);
		const uint64_t samples_per_tick = sample_rate / 75;
		const uint64_t samples_per_sector = cdic_hle::xa_samples_per_sector_per_channel(coding);
		const uint64_t played_samples = samples_per_tick * SECTOR_TICKS;
		const uint64_t timestamp_numerator = sample_rate * SECTOR_TICKS;
		cdic_hle::realtime_audio_state audio;
		cdic_hle::start_realtime_audio(audio);
		uint8_t next_delivery = cdic_hle::RESET_NEXT_AUDIO_BUFFER;
		uint8_t expected_play = cdic_hle::RESET_NEXT_AUDIO_BUFFER;
		uint64_t delivered = 0;
		uint64_t consumed = 0;
		bool sequence_ok = true;
		auto take_ready = [&]()
		{
			const uint8_t index = cdic_hle::take_realtime_audio_buffer(audio);
			if (index == cdic_hle::NO_AUDIO_BUFFER)
				return;
			sequence_ok = sequence_ok && index == expected_play;
			expected_play ^= 1;
			consumed++;
			cdic_hle::begin_realtime_audio_buffer(audio, coding.sector_periods);
		};

		// The device creates its audio timer before its sector timer, so an
		// exact-boundary audio tick may observe starvation immediately before
		// sector delivery.  Delivery retries playback in the same scheduler time.
		for (uint64_t tick = 0; tick < SECTOR_TICKS; tick++)
		{
			if (tick && cdic_hle::advance_realtime_audio(audio))
				take_ready();
			if (!(tick % coding.sector_periods))
			{
				cdic_hle::mark_realtime_audio_ready(audio, next_delivery);
				next_delivery ^= 1;
				delivered++;
				take_ready();
			}
		}

		INFO("coding=" << raw << " rate=" << sample_rate
			<< " periods=" << unsigned(coding.sector_periods));
		REQUIRE(sample_rate % 75 == 0);
		REQUIRE(samples_per_sector == samples_per_tick * coding.sector_periods);
		REQUIRE(played_samples * 75 == timestamp_numerator);
		REQUIRE(played_samples == sample_rate * 30ULL * 60);
		REQUIRE(sequence_ok);
		REQUIRE(consumed == delivered);
		REQUIRE(consumed == (SECTOR_TICKS - 1) / coding.sector_periods + 1);
	}

	REQUIRE(valid_count == 16);
}
