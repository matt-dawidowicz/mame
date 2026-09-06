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

constexpr std::uint16_t MMU_ATTRIBUTE_SUPERVISOR = 0x4000;
constexpr std::uint16_t MMU_ATTRIBUTE_EXECUTE = 0x2000;
constexpr std::uint16_t MMU_ATTRIBUTE_READ = 0x1000;
constexpr std::uint16_t MMU_ATTRIBUTE_WRITE = 0x0800;
constexpr std::uint16_t MMU_ATTRIBUTE_STACK = 0x0080;

constexpr std::uint8_t MMU_STATUS_NOT_PRESENT = 0x80;
constexpr std::uint8_t MMU_STATUS_STACK = 0x40;
constexpr std::uint8_t MMU_STATUS_LENGTH = 0x20;
constexpr std::uint8_t MMU_STATUS_ACCESS = 0x10;
constexpr std::uint8_t MMU_STATUS_SUPERVISOR = 0x08;
constexpr std::uint8_t MMU_STATUS_EXECUTE = 0x04;
constexpr std::uint8_t MMU_STATUS_READ = 0x02;
constexpr std::uint8_t MMU_STATUS_WRITE = 0x01;

struct mmu_descriptor
{
	std::uint16_t attr = 0;
	std::uint16_t length = 0;
	std::uint8_t segment = 0;
	std::uint16_t base = 0;
};

enum class mmu_access_type : std::uint8_t
{
	execute,
	read,
	write
};

enum class mmu_translation_status : std::uint8_t
{
	disabled,
	translated,
	not_present,
	length_violation,
	attribute_violation,
	multiple_match
};

struct mmu_translation_result
{
	mmu_translation_status status = mmu_translation_status::not_present;
	std::uint32_t physical_address = 0;
	std::uint8_t descriptor = 0xff;
	std::uint8_t logical_segment = 0;
	std::uint16_t displacement = 0;
	std::uint16_t effective_displacement = 0;
	std::uint16_t attributes = 0;
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

constexpr std::uint16_t mmu_maximum_displacement(std::uint8_t control)
{
	return mmu_mode2(control) ? 0x007f : 0x07ff;
}

constexpr std::uint16_t mmu_effective_segment_length(std::uint8_t control, std::uint16_t length)
{
	length &= 0x07ff;
	return mmu_mode2(control) ? std::uint16_t(length >> 4) : length;
}

template <typename Descriptor>
constexpr bool mmu_descriptor_valid(const Descriptor &desc)
{
	return (desc.segment & MMU_DESCRIPTOR_FLUSH_VALID) != 0;
}

template <typename Descriptor>
constexpr std::uint8_t mmu_descriptor_segment(std::uint8_t control, const Descriptor &desc)
{
	return mmu_mode2(control)
		? std::uint8_t(desc.segment & MMU_DESCRIPTOR_SEGMENT_MASK)
		: std::uint8_t(desc.segment & 0x07);
}

template <typename Descriptor>
constexpr bool mmu_descriptor_stack(const Descriptor &desc)
{
	return (desc.attr & MMU_ATTRIBUTE_STACK) != 0;
}

template <typename Descriptor>
constexpr std::uint16_t mmu_effective_displacement(std::uint8_t control, const Descriptor &desc, std::uint16_t displacement)
{
	return mmu_descriptor_stack(desc)
		? std::uint16_t(mmu_maximum_displacement(control) - displacement)
		: displacement;
}

template <typename Descriptor>
constexpr std::uint32_t mmu_physical_address(
		std::uint8_t control,
		const Descriptor &desc,
		std::uint16_t displacement,
		std::uint32_t address)
{
	const std::uint16_t effective = mmu_effective_displacement(control, desc, displacement);
	const std::uint32_t base = desc.base & 0x3fff;
	const std::uint32_t physical_block = mmu_descriptor_stack(desc)
		? (base - effective) & 0x3fff
		: (base + effective) & 0x3fff;
	return ((physical_block << MMU_BLOCK_SHIFT) | (address & MMU_BLOCK_OFFSET_MASK)) & MMU_ADDRESS_MASK;
}

template <typename Descriptor>
constexpr bool mmu_access_permitted(const Descriptor &desc, mmu_access_type access, bool supervisor)
{
	if ((desc.attr & MMU_ATTRIBUTE_SUPERVISOR) && !supervisor)
		return false;

	switch (access)
	{
	case mmu_access_type::execute:
		return (desc.attr & MMU_ATTRIBUTE_EXECUTE) != 0;
	case mmu_access_type::read:
		return (desc.attr & MMU_ATTRIBUTE_READ) != 0;
	case mmu_access_type::write:
		return (desc.attr & MMU_ATTRIBUTE_WRITE) != 0;
	}

	return false;
}

template <typename Descriptor>
constexpr mmu_translation_result mmu_translate_impl(
		std::uint8_t control,
		const Descriptor *descriptors,
		std::size_t count,
		std::uint32_t logical_address)
{
	logical_address &= MMU_ADDRESS_MASK;
	const std::uint8_t logical_segment = mmu_logical_segment(control, logical_address);
	const std::uint16_t displacement = mmu_logical_displacement(control, logical_address);

	if (!mmu_enabled(control))
		return { mmu_translation_status::disabled, logical_address, 0xff, logical_segment, displacement, displacement, 0 };

	std::uint8_t match = 0xff;
	for (std::size_t index = 0; index < count; ++index)
	{
		const Descriptor &desc = descriptors[index];
		if (!mmu_descriptor_valid(desc) || mmu_descriptor_segment(control, desc) != logical_segment)
			continue;

		if (match != 0xff)
			return { mmu_translation_status::multiple_match, logical_address, 0xff, logical_segment, displacement, displacement, 0 };

		match = std::uint8_t(index);
	}

	if (match == 0xff)
		return { mmu_translation_status::not_present, logical_address, 0xff, logical_segment, displacement, displacement, 0 };

	const Descriptor &desc = descriptors[match];
	const std::uint16_t effective_displacement = mmu_effective_displacement(control, desc, displacement);
	if (effective_displacement > mmu_effective_segment_length(control, desc.length))
	{
		return {
			mmu_translation_status::length_violation,
			logical_address,
			match,
			logical_segment,
			displacement,
			effective_displacement,
			desc.attr
		};
	}

	return {
		mmu_translation_status::translated,
		mmu_physical_address(control, desc, displacement, logical_address),
		match,
		logical_segment,
		displacement,
		effective_displacement,
		desc.attr
	};
}

template <typename Descriptor, std::size_t Count>
constexpr mmu_translation_result mmu_translate(
		std::uint8_t control,
		const std::array<Descriptor, Count> &descriptors,
		std::uint32_t logical_address)
{
	return mmu_translate_impl(control, descriptors.data(), Count, logical_address);
}

template <typename Descriptor, std::size_t Count>
constexpr mmu_translation_result mmu_translate(
		std::uint8_t control,
		const Descriptor (&descriptors)[Count],
		std::uint32_t logical_address)
{
	return mmu_translate_impl(control, descriptors, Count, logical_address);
}

template <typename Descriptor, std::size_t Count>
constexpr mmu_translation_result mmu_translate_access(
		std::uint8_t control,
		const std::array<Descriptor, Count> &descriptors,
		std::uint32_t logical_address,
		mmu_access_type access,
		bool supervisor)
{
	auto result = mmu_translate(control, descriptors, logical_address);
	if (result.status == mmu_translation_status::translated && !mmu_access_permitted(descriptors[result.descriptor], access, supervisor))
		result.status = mmu_translation_status::attribute_violation;
	return result;
}

template <typename Descriptor, std::size_t Count>
constexpr mmu_translation_result mmu_translate_access(
		std::uint8_t control,
		const Descriptor (&descriptors)[Count],
		std::uint32_t logical_address,
		mmu_access_type access,
		bool supervisor)
{
	auto result = mmu_translate(control, descriptors, logical_address);
	if (result.status == mmu_translation_status::translated && !mmu_access_permitted(descriptors[result.descriptor], access, supervisor))
		result.status = mmu_translation_status::attribute_violation;
	return result;
}

constexpr bool mmu_translation_succeeded(const mmu_translation_result &result)
{
	return result.status == mmu_translation_status::disabled || result.status == mmu_translation_status::translated;
}

constexpr std::uint8_t mmu_descriptor_status_bits(std::uint16_t attributes)
{
	std::uint8_t status = 0;
	if (attributes & MMU_ATTRIBUTE_STACK)
		status |= MMU_STATUS_STACK;
	if (attributes & MMU_ATTRIBUTE_SUPERVISOR)
		status |= MMU_STATUS_SUPERVISOR;
	if (attributes & MMU_ATTRIBUTE_EXECUTE)
		status |= MMU_STATUS_EXECUTE;
	if (attributes & MMU_ATTRIBUTE_READ)
		status |= MMU_STATUS_READ;
	if (attributes & MMU_ATTRIBUTE_WRITE)
		status |= MMU_STATUS_WRITE;
	return status;
}

constexpr std::uint8_t mmu_status_for_fault(const mmu_translation_result &result)
{
	switch (result.status)
	{
	case mmu_translation_status::not_present:
	case mmu_translation_status::multiple_match:
		return MMU_STATUS_NOT_PRESENT;

	case mmu_translation_status::length_violation:
		return MMU_STATUS_LENGTH | mmu_descriptor_status_bits(result.attributes);

	case mmu_translation_status::attribute_violation:
		return MMU_STATUS_ACCESS | mmu_descriptor_status_bits(result.attributes);

	case mmu_translation_status::disabled:
	case mmu_translation_status::translated:
		return 0;
	}

	return 0;
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
