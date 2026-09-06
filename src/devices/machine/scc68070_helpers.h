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
constexpr std::uint32_t MMU_ADDRESS_MASK = 0x00ffffff;
constexpr std::uint32_t MMU_BLOCK_SHIFT = 10;
constexpr std::uint32_t MMU_BLOCK_SIZE = 1U << MMU_BLOCK_SHIFT;
constexpr std::uint32_t MMU_BLOCK_OFFSET_MASK = MMU_BLOCK_SIZE - 1;
constexpr std::uint8_t MMU_CONTROL_ENABLE = 0x80;
constexpr std::uint8_t MMU_CONTROL_SEGMENT_NUMBER = 0x40;
constexpr std::uint8_t MMU_DESCRIPTOR_FLUSH_VALID = 0x80;
constexpr std::uint8_t MMU_DESCRIPTOR_SEGMENT_MASK = 0x7f;

struct mmu_descriptor
{
	std::uint16_t attr = 0;
	std::uint16_t length = 0;
	std::uint8_t segment = 0;
	std::uint16_t base = 0;
};

enum class mmu_translation_status : std::uint8_t
{
	disabled,
	translated,
	not_present,
	length_violation,
	multiple_match
};

struct mmu_translation_result
{
	mmu_translation_status status = mmu_translation_status::not_present;
	std::uint32_t physical_address = 0;
	std::uint8_t descriptor = 0xff;
	std::uint8_t logical_segment = 0;
	std::uint16_t displacement = 0;
};

constexpr bool mmu_enabled(std::uint8_t control)
{
	return (control & MMU_CONTROL_ENABLE) != 0;
}

constexpr bool mmu_mode2(std::uint8_t control)
{
	return (control & MMU_CONTROL_SEGMENT_NUMBER) != 0;
}

constexpr std::uint8_t mmu_logical_segment(std::uint8_t control, std::uint32_t address)
{
	address &= MMU_ADDRESS_MASK;
	return mmu_mode2(control)
		? std::uint8_t((address >> 17) & 0x7f)
		: std::uint8_t((address >> 21) & 0x07);
}

constexpr std::uint16_t mmu_logical_displacement(std::uint8_t control, std::uint32_t address)
{
	address &= MMU_ADDRESS_MASK;
	return mmu_mode2(control)
		? std::uint16_t((address >> MMU_BLOCK_SHIFT) & 0x007f)
		: std::uint16_t((address >> MMU_BLOCK_SHIFT) & 0x07ff);
}

constexpr std::uint16_t mmu_effective_segment_length(std::uint8_t control, std::uint16_t length)
{
	length &= 0x07ff;
	return mmu_mode2(control) ? std::uint16_t(length >> 4) : length;
}

constexpr bool mmu_descriptor_valid(const mmu_descriptor &desc)
{
	return (desc.segment & MMU_DESCRIPTOR_FLUSH_VALID) != 0;
}

constexpr std::uint8_t mmu_descriptor_segment(std::uint8_t control, const mmu_descriptor &desc)
{
	return mmu_mode2(control)
		? std::uint8_t(desc.segment & MMU_DESCRIPTOR_SEGMENT_MASK)
		: std::uint8_t(desc.segment & 0x07);
}

constexpr std::uint32_t mmu_physical_address(const mmu_descriptor &desc, std::uint16_t displacement, std::uint32_t address)
{
	const std::uint32_t physical_block = (std::uint32_t(desc.base & 0x3fff) + displacement) & 0x3fff;
	return ((physical_block << MMU_BLOCK_SHIFT) | (address & MMU_BLOCK_OFFSET_MASK)) & MMU_ADDRESS_MASK;
}

template <std::size_t Count>
constexpr mmu_translation_result mmu_translate(
		std::uint8_t control,
		const std::array<mmu_descriptor, Count> &descriptors,
		std::uint32_t logical_address)
{
	logical_address &= MMU_ADDRESS_MASK;
	const std::uint8_t logical_segment = mmu_logical_segment(control, logical_address);
	const std::uint16_t displacement = mmu_logical_displacement(control, logical_address);

	if (!mmu_enabled(control))
		return { mmu_translation_status::disabled, logical_address, 0xff, logical_segment, displacement };

	std::uint8_t match = 0xff;
	for (std::size_t index = 0; index < Count; ++index)
	{
		const mmu_descriptor &desc = descriptors[index];
		if (!mmu_descriptor_valid(desc) || mmu_descriptor_segment(control, desc) != logical_segment)
			continue;

		if (match != 0xff)
			return { mmu_translation_status::multiple_match, logical_address, 0xff, logical_segment, displacement };

		match = std::uint8_t(index);
	}

	if (match == 0xff)
		return { mmu_translation_status::not_present, logical_address, 0xff, logical_segment, displacement };

	const mmu_descriptor &desc = descriptors[match];
	if (displacement > mmu_effective_segment_length(control, desc.length))
		return { mmu_translation_status::length_violation, logical_address, match, logical_segment, displacement };

	return {
		mmu_translation_status::translated,
		mmu_physical_address(desc, displacement, logical_address),
		match,
		logical_segment,
		displacement
	};
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
