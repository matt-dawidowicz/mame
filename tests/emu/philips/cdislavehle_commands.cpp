// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstddef>
#include <cstdint>

#include "catch.hpp"

#include "cdislavehle_state.h"

TEST_CASE(
	"CD-i SLAVE command map classifies every accepted opcode by channel",
	"[emu][philips][cdi][slave][command]")
{
	using cdi_slave_hle::classification;
	using cdi_slave_hle::command;
	using cdi_slave_hle::describe_command;

	std::array<std::size_t, cdi_slave_hle::CHANNEL_COUNT> known{};
	for (uint16_t channel = 0; channel < cdi_slave_hle::CHANNEL_COUNT; ++channel)
	{
		for (uint16_t opcode = 0; opcode < 0x100; ++opcode)
		{
			const auto descriptor = describe_command(uint8_t(channel), uint8_t(opcode));
			INFO("channel=" << channel << " opcode=" << opcode);
			if (descriptor.kind == command::unknown)
			{
				REQUIRE(descriptor.request_length == 0);
				REQUIRE(descriptor.state == classification::unknown);
				REQUIRE(descriptor.response_length == 0);
			}
			else
			{
				known[channel]++;
				REQUIRE(descriptor.request_length >= 1);
				REQUIRE(descriptor.request_length <= 17);
				REQUIRE(descriptor.state != classification::unknown);
				REQUIRE((descriptor.response_length == 0) ==
					(descriptor.response_channel == cdi_slave_hle::NO_CHANNEL));
			}
		}
	}

	REQUIRE((known == std::array<std::size_t, 4>{ 66, 0, 21, 11 }));

	REQUIRE(describe_command(0, 0x83).kind == command::pointer_enable);
	REQUIRE(describe_command(0, 0x84).kind == command::pointer_disable);
	REQUIRE(describe_command(0, 0xc0).kind == command::pointer_position);
	REQUIRE(describe_command(0, 0xff).kind == command::pointer_position);
	REQUIRE(describe_command(2, 0x8a).kind == command::cpu_reset);
	REQUIRE(describe_command(2, 0xc0).kind == command::audio_attenuation);
	REQUIRE(describe_command(2, 0xcf).kind == command::audio_attenuation);
	REQUIRE(describe_command(2, 0xf0).kind == command::lcd_write);
	REQUIRE(describe_command(3, 0xb0).kind == command::disc_status);
	REQUIRE(describe_command(3, 0xb1).kind == command::disc_base);
	REQUIRE(describe_command(3, 0xfa).kind == command::xbus_interrupt_enable);
}

TEST_CASE(
	"CD-i SLAVE revision response exposes both firmware release bytes",
	"[emu][philips][cdi][slave][command][response]")
{
	using cdi_slave_hle::classification;
	using cdi_slave_hle::command;
	using cdi_slave_hle::describe_command;
	using cdi_slave_hle::response_timing;

	const auto revision = describe_command(3, 0xf0);
	REQUIRE(revision.kind == command::revision);
	REQUIRE(revision.request_length == 1);
	REQUIRE(revision.state == classification::verified);
	REQUIRE(revision.response_channel == 2);
	REQUIRE(revision.response_length == 3);
	REQUIRE(revision.timing == response_timing::short_irq);

	const auto disc_status = describe_command(3, 0xb0);
	REQUIRE(disc_status.state == classification::compatibility);
	REQUIRE(disc_status.response_channel == 3);
	REQUIRE(disc_status.response_length == 4);
	REQUIRE(disc_status.timing == response_timing::disc_irq);

	const auto disc_base = describe_command(3, 0xb1);
	REQUIRE(disc_base.state == classification::unimplemented);
	REQUIRE(disc_base.response_channel == cdi_slave_hle::NO_CHANNEL);
	REQUIRE(disc_base.response_length == 0);

	const auto unknown = describe_command(3, 0xff);
	REQUIRE(unknown.kind == command::unknown);
	REQUIRE(unknown.response_length == 0);
}

TEST_CASE(
	"CD-i SLAVE shared parser accepts the established LCD mailbox path",
	"[emu][philips][cdi][slave][command][transport]")
{
	using cdi_slave_hle::idle_parser;
	using cdi_slave_hle::parse_byte;
	using cdi_slave_hle::parse_result;
	using cdi_slave_hle::parser_idle;

	auto state = idle_parser();
	auto transition = parse_byte(state, 2, 0xf0, 17);
	REQUIRE(transition.result == parse_result::collecting);
	REQUIRE(transition.write_index == 0);
	REQUIRE(transition.next.origin_channel == 2);
	REQUIRE(transition.next.index == 1);
	REQUIRE(transition.next.count == 17);
	state = transition.next;

	for (uint8_t index = 1; index < 16; ++index)
	{
		const uint8_t channel = index & 1 ? 1 : 2;
		transition = parse_byte(state, channel, index, 17);
		INFO("index=" << unsigned(index) << " channel=" << unsigned(channel));
		REQUIRE(transition.result == parse_result::collecting);
		REQUIRE(transition.write_index == index);
		state = transition.next;
	}

	transition = parse_byte(state, 1, 0x10, 17);
	REQUIRE(transition.result == parse_result::complete);
	REQUIRE(transition.write_index == 16);
	REQUIRE(parser_idle(transition.next));
}

TEST_CASE(
	"CD-i SLAVE parser rejects wrong-channel and malformed command streams",
	"[emu][philips][cdi][slave][command][transport][hardening][malformed]")
{
	using cdi_slave_hle::idle_parser;
	using cdi_slave_hle::parse_byte;
	using cdi_slave_hle::parse_result;
	using cdi_slave_hle::parser_idle;
	using cdi_slave_hle::parser_state;

	auto transition = parse_byte(idle_parser(), 1, 0xf0, 17);
	REQUIRE(transition.result == parse_result::rejected);
	REQUIRE(parser_idle(transition.next));

	transition = parse_byte(idle_parser(), 4, 0xf0, 17);
	REQUIRE(transition.result == parse_result::rejected);

	transition = parse_byte(idle_parser(), 2, 0xf0, 16);
	REQUIRE(transition.result == parse_result::rejected);

	transition = parse_byte(idle_parser(), 2, 0xc0, 17);
	REQUIRE(transition.result == parse_result::collecting);
	transition = parse_byte(transition.next, 1, 0x00, 17);
	REQUIRE(transition.result == parse_result::rejected);
	REQUIRE(parser_idle(transition.next));

	transition = parse_byte(parser_state{ 2, 0xc0, 17, 17 }, 2, 0x00, 17);
	REQUIRE(transition.result == parse_result::rejected);
	REQUIRE(parser_idle(transition.next));

	transition = parse_byte(parser_state{ 3, 0xb0, 2, 5 }, 3, 0x00, 17);
	REQUIRE(transition.result == parse_result::rejected);
	REQUIRE(parser_idle(transition.next));
}

TEST_CASE(
	"CD-i SLAVE parser completes fixed-length commands at exact bounds",
	"[emu][philips][cdi][slave][command][transport]")
{
	using cdi_slave_hle::command;
	using cdi_slave_hle::idle_parser;
	using cdi_slave_hle::parse_byte;
	using cdi_slave_hle::parse_result;

	auto transition = parse_byte(idle_parser(), 2, 0xc0, 17);
	REQUIRE(transition.result == parse_result::collecting);
	for (uint8_t index = 1; index < 5; ++index)
	{
		transition = parse_byte(transition.next, 2, index, 17);
		REQUIRE(transition.write_index == index);
		REQUIRE(transition.result == (index == 4 ? parse_result::complete : parse_result::collecting));
	}
	REQUIRE(transition.descriptor.kind == command::audio_attenuation);

	transition = parse_byte(idle_parser(), 3, 0xb0, 17);
	REQUIRE(transition.result == parse_result::collecting);
	for (uint8_t index = 1; index < 4; ++index)
		transition = parse_byte(transition.next, 3, index, 17);
	REQUIRE(transition.result == parse_result::complete);
	REQUIRE(transition.descriptor.kind == command::disc_status);

	transition = parse_byte(idle_parser(), 3, 0xf0, 17);
	REQUIRE(transition.result == parse_result::complete);
	REQUIRE(transition.write_index == 0);
	REQUIRE(transition.descriptor.kind == command::revision);
}
