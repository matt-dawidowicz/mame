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
	// USR bit 1 is hard-wired high.  TXEMT compatibility remains in usr_r().
	return status | 0x02;
}

constexpr std::uint8_t uart_control_after_misc_command(std::uint8_t control)
{
	// UCR miscellaneous commands are strobes; receiver/transmitter controls persist.
	return control & 0x0f;
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
