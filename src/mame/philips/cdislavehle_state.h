// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#ifndef MAME_PHILIPS_CDISLAVEHLE_STATE_H
#define MAME_PHILIPS_CDISLAVEHLE_STATE_H

#pragma once

#include <cstddef>
#include <cstdint>

namespace cdi_slave_hle
{

constexpr uint8_t CHANNEL_COUNT = 4;
constexpr uint8_t NO_CHANNEL = 0xff;

enum class command : uint8_t
{
	unknown,
	pointer_enable,
	pointer_disable,
	pointer_position,
	keyboard_enable,
	audio_mute,
	audio_unmute,
	cpu_reset,
	audio_attenuation,
	lcd_write,
	memory_set,
	memory_clear,
	disc_status,
	disc_base,
	revision,
	pointer_type,
	test_plug,
	video_standard,
	developer_enable,
	xbus_interrupt_enable,
	developer_disable
};

// These labels describe the best evidence for the complete modeled command,
// not merely the fact that software writes its opcode.
enum class classification : uint8_t
{
	verified,
	strongly_inferred,
	compatibility,
	unimplemented,
	unknown
};

enum class response_timing : uint8_t
{
	none,
	immediate_irq,
	short_irq,
	disc_irq,
	immediate_no_irq
};

struct command_descriptor
{
	command kind;
	uint8_t request_length;
	classification state;
	uint8_t response_channel;
	uint8_t response_length;
	response_timing timing;
};

constexpr command_descriptor NO_COMMAND =
	{ command::unknown, 0, classification::unknown, NO_CHANNEL, 0, response_timing::none };

constexpr command_descriptor describe_command(uint8_t channel, uint8_t opcode)
{
	switch (channel)
	{
	case 0:
		if (opcode == 0x83)
			return { command::pointer_enable, 1, classification::strongly_inferred, 0, 4, response_timing::immediate_irq };
		if (opcode == 0x84)
			return { command::pointer_disable, 1, classification::strongly_inferred, NO_CHANNEL, 0, response_timing::none };
		if (opcode >= 0xc0)
			return { command::pointer_position, 3, classification::strongly_inferred, NO_CHANNEL, 0, response_timing::none };
		return NO_COMMAND;

	case 1:
		// Channel 1 has no known initiating command.  Existing firmware paths
		// may use it to continue a channel-2 LCD transfer.
		return NO_COMMAND;

	case 2:
		if (opcode == 0x80)
			return { command::keyboard_enable, 1, classification::unimplemented, NO_CHANNEL, 0, response_timing::none };
		if (opcode == 0x82)
			return { command::audio_mute, 1, classification::strongly_inferred, NO_CHANNEL, 0, response_timing::none };
		if (opcode == 0x83)
			return { command::audio_unmute, 1, classification::strongly_inferred, NO_CHANNEL, 0, response_timing::none };
		if (opcode == 0x8a)
			return { command::cpu_reset, 1, classification::strongly_inferred, NO_CHANNEL, 0, response_timing::none };
		if (opcode >= 0xc0 && opcode <= 0xcf)
			return { command::audio_attenuation, 5, classification::strongly_inferred, NO_CHANNEL, 0, response_timing::none };
		if (opcode == 0xf0)
			return { command::lcd_write, 17, classification::strongly_inferred, NO_CHANNEL, 0, response_timing::none };
		return NO_COMMAND;

	case 3:
		switch (opcode)
		{
		case 0x80: return { command::memory_set, 4, classification::unimplemented, NO_CHANNEL, 0, response_timing::none };
		case 0x81: return { command::memory_clear, 4, classification::unimplemented, NO_CHANNEL, 0, response_timing::none };
		case 0xb0: return { command::disc_status, 4, classification::compatibility, 3, 4, response_timing::disc_irq };
		case 0xb1: return { command::disc_base, 4, classification::unimplemented, NO_CHANNEL, 0, response_timing::none };
		case 0xf0: return { command::revision, 1, classification::verified, 2, 3, response_timing::short_irq };
		case 0xf3: return { command::pointer_type, 1, classification::compatibility, 2, 2, response_timing::short_irq };
		case 0xf4: return { command::test_plug, 1, classification::strongly_inferred, 2, 2, response_timing::short_irq };
		case 0xf6: return { command::video_standard, 1, classification::strongly_inferred, 2, 2, response_timing::immediate_no_irq };
		case 0xf7: return { command::developer_enable, 1, classification::unimplemented, NO_CHANNEL, 0, response_timing::none };
		case 0xfa: return { command::xbus_interrupt_enable, 1, classification::unimplemented, NO_CHANNEL, 0, response_timing::none };
		case 0xfe: return { command::developer_disable, 1, classification::unimplemented, NO_CHANNEL, 0, response_timing::none };
		default:   return NO_COMMAND;
		}

	default:
		return NO_COMMAND;
	}
}

constexpr bool continuation_channel_allowed(
		command_descriptor descriptor,
		uint8_t origin_channel,
		uint8_t write_channel)
{
	if (descriptor.kind == command::lcd_write && origin_channel == 2)
		return write_channel == 1 || write_channel == 2;

	return write_channel == origin_channel;
}

struct parser_state
{
	uint8_t origin_channel;
	uint8_t opcode;
	uint8_t index;
	uint8_t count;
};

constexpr parser_state idle_parser()
{
	return { NO_CHANNEL, 0, 0, 0 };
}

constexpr bool parser_idle(parser_state state)
{
	return state.origin_channel == NO_CHANNEL && state.index == 0 && state.count == 0;
}

enum class parse_result : uint8_t
{
	rejected,
	collecting,
	complete
};

struct parser_transition
{
	parse_result result;
	command_descriptor descriptor;
	parser_state next;
	uint8_t write_index;
};

constexpr parser_transition reject_byte()
{
	return { parse_result::rejected, NO_COMMAND, idle_parser(), 0 };
}

constexpr parser_transition parse_byte(
		parser_state state,
		uint8_t write_channel,
		uint8_t value,
		std::size_t capacity)
{
	if (write_channel >= CHANNEL_COUNT || capacity == 0)
		return reject_byte();

	if (state.index == 0)
	{
		if (!parser_idle(state))
			return reject_byte();

		const command_descriptor descriptor = describe_command(write_channel, value);
		if (descriptor.kind == command::unknown
				|| descriptor.request_length == 0
				|| descriptor.request_length > capacity)
		{
			return reject_byte();
		}

		if (descriptor.request_length == 1)
			return { parse_result::complete, descriptor, idle_parser(), 0 };

		return
		{
			parse_result::collecting,
			descriptor,
			{ write_channel, value, 1, descriptor.request_length },
			0
		};
	}

	const command_descriptor descriptor = describe_command(state.origin_channel, state.opcode);
	if (descriptor.kind == command::unknown
			|| state.origin_channel >= CHANNEL_COUNT
			|| state.count != descriptor.request_length
			|| state.index >= state.count
			|| state.index >= capacity
			|| !continuation_channel_allowed(descriptor, state.origin_channel, write_channel))
	{
		return reject_byte();
	}

	const uint8_t write_index = state.index;
	state.index++;
	if (state.index == state.count)
		return { parse_result::complete, descriptor, idle_parser(), write_index };

	return { parse_result::collecting, descriptor, state, write_index };
}

} // namespace cdi_slave_hle

#endif // MAME_PHILIPS_CDISLAVEHLE_STATE_H
