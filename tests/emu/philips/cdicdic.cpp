// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdicdic_state.h"

namespace {

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

TEST_CASE("CDIC command time converts BCD MSF and whole-second seeks to logical LBA", "[emu][philips][cdic][command][sector]")
{
	REQUIRE(cdic_hle::lba_from_time(0x00020000) == 0);
	REQUIRE(cdic_hle::lba_from_time(0x00020100) == 1);
	REQUIRE(cdic_hle::lba_from_time(0x00027400) == 74);
	REQUIRE(cdic_hle::lba_from_time(0x00028000) == 0);
	REQUIRE(cdic_hle::lba_from_time(0x01000000) == 4350);
	REQUIRE(cdic_hle::lba_from_time(0x01012300) == 4448);
}

TEST_CASE("CDIC XA predictor vectors preserve history, clipping, and reserved-range handling", "[emu][philips][cdic][xa]")
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

	decoded = cdic_hle::decode_xa_sample(0x0f, int16_t(0x8000), 0, 0);
	REQUIRE(decoded.output == -8);
	REQUIRE(cdic_hle::decode_xa_sample(0x0c, int16_t(0x8000), 0, 0).output == -8);
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
