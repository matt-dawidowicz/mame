// license:BSD-3-Clause
// copyright-holders:MAMEdev Team

#ifndef MAME_MACHINE_SCC68070_HELPERS_H
#define MAME_MACHINE_SCC68070_HELPERS_H

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace scc68070
{

constexpr std::uint32_t DMA_ADDRESS_MASK = 0x00ffffff;

// Philips documents the UART as eight byte-wide registers on odd addresses.
// Keep the canonical map here so register mapping tests cannot silently drift.
constexpr std::uint32_t UART_RHR_ADDRESS = 0x80002011;
constexpr std::uint32_t UART_THR_ADDRESS = 0x80002013;
constexpr std::uint32_t UART_USR_ADDRESS = 0x80002015;
constexpr std::uint32_t UART_UMR_ADDRESS = 0x80002019;
constexpr std::uint32_t UART_UCR_ADDRESS = 0x8000201d;
constexpr std::uint32_t UART_UCS_ADDRESS = 0x8000201f;

// SCC68070 DMA address counters are 24-bit.  Their high-word register only
// exposes bits 23-16 in the low byte; the upper byte is reserved.
constexpr std::uint32_t dma_address_high_write(std::uint32_t current, std::uint16_t data, std::uint16_t mem_mask)
{
	const std::uint32_t mask = std::uint32_t(mem_mask & 0x00ff) << 16;
	return ((current & ~mask) | ((std::uint32_t(data) << 16) & mask)) & DMA_ADDRESS_MASK;
}

constexpr std::uint32_t dma_address_low_write(std::uint32_t current, std::uint16_t data, std::uint16_t mem_mask)
{
	const std::uint32_t mask = mem_mask;
	return ((current & ~mask) | (std::uint32_t(data) & mask)) & DMA_ADDRESS_MASK;
}

struct dma_address_result
{
	bool valid;
	std::uint32_t address;
};

// Sequence-control address modes implemented by the SCC68070 are fixed and
// increment.  The remaining encodings are reserved and must not silently act
// as decrement modes.
constexpr dma_address_result dma_address_after_transfer(std::uint32_t address, std::uint8_t mode, std::uint32_t operand_size)
{
	switch (mode & 0x03)
	{
	case 0x00:
		return { true, address & DMA_ADDRESS_MASK };
	case 0x01:
		return { true, (address + operand_size) & DMA_ADDRESS_MASK };
	default:
		return { false, address & DMA_ADDRESS_MASK };
	}
}

struct dma_count_result
{
	bool valid;
	std::uint16_t remaining;
	bool complete;
};

// MTC is decremented once for each successfully transferred memory operand.
// A programmed zero count is not a transferable operand; completion occurs
// when a non-zero count is decremented to zero.
constexpr dma_count_result dma_count_after_transfer(std::uint16_t counter)
{
	if (!counter)
		return { false, 0, false };
	const std::uint16_t remaining = std::uint16_t(counter - 1);
	return { true, remaining, remaining == 0 };
}

constexpr std::uint8_t dma_status_after_count(std::uint8_t status, bool complete)
{
	// CSR: COC=0x80, CA=0x08.
	return complete ? std::uint8_t((status & ~0x08U) | 0x80U) : status;
}

constexpr std::uint8_t dma_status_after_device_termination(std::uint8_t status)
{
	// Device termination completes the current operand and sets NDT + COC.
	return std::uint8_t((status & ~0x08U) | 0xa0U);
}

constexpr std::uint8_t dma_status_after_error(std::uint8_t status)
{
	// Bus error/software abort terminate the operation with ERR + COC.
	return std::uint8_t((status & ~0x08U) | 0x90U);
}

// Philips documents MTCH/MTCL, MAC and DAC as not affected by RESET.  Keep
// the resettable controller fields together so production code cannot
// accidentally zero the transfer/address counters while resetting them.
template <typename Channel>
constexpr void reset_dma_control_state(Channel &channel, std::uint8_t device_control, std::uint8_t operation_control, std::uint8_t sequence_control)
{
	channel.channel_status = 0;
	channel.channel_error = 0;
	channel.device_control = device_control;
	channel.operation_control = operation_control;
	channel.sequence_control = sequence_control;
	channel.channel_control = 0;
}

enum class interrupt_source : std::uint8_t
{
	int1,
	int2,
	timer,
	uart_rx,
	uart_tx,
	i2c,
	dma1,
	dma2,
	none
};

template <std::size_t Count>
constexpr std::uint8_t highest_interrupt_level(const std::array<std::uint8_t, Count> &levels)
{
	std::uint8_t highest = 0;
	for (std::uint8_t level : levels)
		if (level > highest)
			highest = level;
	return highest;
}

constexpr interrupt_source first_interrupt_source(const std::array<std::uint8_t, 8> &levels, std::uint8_t acknowledged_level)
{
	for (std::size_t index = 0; index < levels.size(); ++index)
		if (levels[index] != 0 && levels[index] == acknowledged_level)
			return static_cast<interrupt_source>(index);
	return interrupt_source::none;
}

constexpr std::uint8_t i2c_status_after_data_access(std::uint8_t status)
{
	// IDR access sets PIN and clears AL and AAS.
	return (status | 0x10) & ~std::uint8_t(0x0c);
}

constexpr std::uint8_t uart_status_read_value(std::uint8_t status)
{
	// USR bit 1 is unused and reads as zero in the Philips programming model.
	return status & ~std::uint8_t(0x02);
}

constexpr std::uint8_t uart_control_after_misc_command(std::uint8_t control)
{
	// UCR miscellaneous commands are strobes; receiver/transmitter controls persist.
	return control & 0x0f;
}

// UMR/RMR format from the Philips specification:
// bit 0: 0=7 data bits, 1=8 data bits
// bit 1: 0=1 stop bit, 1=2 stop bits
// bit 2: parity type (0=odd, 1=even)
// bit 3: parity control (0=inhibited, 1=enabled)
// bit 4: CTS controls transmitter when set
// bits 7:6: normal/auto-echo/local-loopback/remote-loopback.
constexpr unsigned uart_character_bits(std::uint8_t mode)
{
	return 7U + (mode & 0x01U);
}

constexpr unsigned uart_stop_bits(std::uint8_t mode)
{
	return 1U + ((mode >> 1) & 0x01U);
}

constexpr bool uart_parity_enabled(std::uint8_t mode)
{
	return (mode & 0x08U) != 0;
}

constexpr bool uart_even_parity(std::uint8_t mode)
{
	return (mode & 0x04U) != 0;
}

constexpr unsigned uart_frame_bits(std::uint8_t mode)
{
	return 1U + uart_character_bits(mode) + (uart_parity_enabled(mode) ? 1U : 0U) + uart_stop_bits(mode);
}

constexpr std::uint8_t uart_character_mask(std::uint8_t mode)
{
	return (mode & 0x01U) ? 0xffU : 0x7fU;
}

inline constexpr std::array<std::uint32_t, 8> UART_BAUD_DIVISORS =
{
	65536, 32768, 16384, 4096, 2048, 1024, 512, 256
};

constexpr std::uint32_t uart_baud_clock(std::uint32_t system_clock, std::uint32_t external_clock, bool use_external_clock)
{
	return use_external_clock ? external_clock : system_clock / 4;
}

constexpr std::uint32_t uart_baud_divisor(std::uint8_t selector)
{
	return UART_BAUD_DIVISORS[selector & 7];
}

enum class timer_mode : std::uint8_t
{
	inhibited = 0,
	match = 1,
	capture = 2,
	count = 3
};

enum class timer_edge : std::uint8_t
{
	inhibited = 0,
	rising = 1,
	falling = 2,
	both = 3
};

enum class timer_event : std::uint8_t
{
	match,
	capture,
	overflow
};

constexpr timer_mode timer_channel_mode(std::uint8_t control, unsigned channel)
{
	return channel == 1
		? static_cast<timer_mode>((control >> 4) & 0x03)
		: channel == 2
			? static_cast<timer_mode>(control & 0x03)
			: timer_mode::inhibited;
}

constexpr timer_edge timer_channel_edge(std::uint8_t control, unsigned channel)
{
	return channel == 1
		? static_cast<timer_edge>((control >> 6) & 0x03)
		: channel == 2
			? static_cast<timer_edge>((control >> 2) & 0x03)
			: timer_edge::inhibited;
}

constexpr bool timer_edge_matches(timer_edge edge, bool previous, bool current)
{
	if (previous == current)
		return false;
	return edge == timer_edge::both
		|| (edge == timer_edge::rising && !previous && current)
		|| (edge == timer_edge::falling && previous && !current);
}

constexpr std::uint8_t timer_status_bit(unsigned channel, timer_event event)
{
	if (channel == 1)
	{
		switch (event)
		{
		case timer_event::match: return 0x40;
		case timer_event::capture: return 0x20;
		case timer_event::overflow: return 0x10;
		}
	}
	if (channel == 2)
	{
		switch (event)
		{
		case timer_event::match: return 0x08;
		case timer_event::capture: return 0x04;
		case timer_event::overflow: return 0x02;
		}
	}
	return 0;
}

struct timer_count_result
{
	std::uint16_t value;
	bool overflow;
};

constexpr timer_count_result timer_count_external_event(std::uint16_t value)
{
	const std::uint16_t next = std::uint16_t(value + 1);
	return { next, next == 0 };
}

constexpr std::uint16_t mmu_status_control_word(std::uint8_t status, std::uint8_t control)
{
	return (std::uint16_t(status) << 8) | control;
}

constexpr std::uint8_t mmu_control_value(std::uint8_t control)
{
	return control & 0xc0;
}

constexpr std::uint16_t mmu_segment_length(std::uint16_t length)
{
	return length & 0x07ff;
}

constexpr std::uint16_t mmu_base_address(std::uint16_t base)
{
	return base & 0x3fff;
}

} // namespace scc68070

#endif // MAME_MACHINE_SCC68070_HELPERS_H
