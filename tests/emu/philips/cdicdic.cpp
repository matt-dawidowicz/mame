// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdicdic_state.h"

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
	REQUIRE(describe_command(0x29).operation == disc_operation::mode1);
	REQUIRE(describe_command(0x2a).operation == disc_operation::mode2);
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

	auto result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x13, 5, 0x08 });
	REQUIRE(result.target == sector_target::filtered);
	REQUIRE_FALSE(result.end_read);

	result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x12, 5, 0x00 });
	REQUIRE(result.target == sector_target::filtered);

	result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x12, 5, 0x08 });
	REQUIRE(result.target == sector_target::data);

	result = select_mode2_sector(file, 0, audio_channels, mode2_sector{ 0x12, 5, 0x08 });
	REQUIRE(result.target == sector_target::filtered);

	result = select_mode2_sector(file, channels, audio_channels, mode2_sector{ 0x12, 5, 0x24 });
	REQUIRE(result.target == sector_target::audio);

	result = select_mode2_sector(file, channels, 0, mode2_sector{ 0x12, 5, 0x24 });
	REQUIRE(result.target == sector_target::data);

	result = select_mode2_sector(file, 0, 0, mode2_sector{ 0x12, 31, 0x80 });
	REQUIRE(result.target == sector_target::data);
	REQUIRE(result.end_read);

	REQUIRE_FALSE(cdic_hle::channel_selected(0xffffffff, 32));
	REQUIRE_FALSE(cdic_hle::audio_channel_selected(0xffff, 16));
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

TEST_CASE("CDIC XA sector duration accepts only supported coding combinations", "[emu][philips][cdic][xa]")
{
	REQUIRE(cdic_hle::xa_sector_count(0x11) == 2);
	REQUIRE(cdic_hle::xa_sector_count(0x01) == 4);
	REQUIRE(cdic_hle::xa_sector_count(0x05) == 8);
	REQUIRE(cdic_hle::xa_sector_count(0x04) == 16);
	REQUIRE(cdic_hle::xa_sector_count(0x20) == 0);
	REQUIRE(cdic_hle::xa_sector_count(0x08) == 0);
	REQUIRE(cdic_hle::xa_sector_count(0x02) == 0);
	REQUIRE(cdic_hle::xa_sector_count(0x80) == 0);
}
