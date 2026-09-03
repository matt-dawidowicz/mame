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
			return interrupt_source(index);
	return interrupt_source::none;
}

struct dma_address_result
{
	bool valid;
	std::uint32_t address;
};

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

constexpr std::uint8_t i2c_status_after_data_access(std::uint8_t status)
{
	// IDR access sets PIN and clears AL and AAS.
	return (status | 0x10) & ~std::uint8_t(0x0c);
}

constexpr std::uint8_t uart_status_read_value(std::uint8_t status)
{
	// USR bit 1 is hard-wired high.
	return status | 0x02;
}

constexpr std::uint8_t uart_control_after_misc_command(std::uint8_t control)
{
	// UCR miscellaneous commands are strobes; receiver/transmitter controls persist.
	return control & 0x0f;
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
