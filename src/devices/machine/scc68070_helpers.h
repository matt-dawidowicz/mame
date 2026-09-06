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
	std::uint16_t remaining;
	bool complete;
};

// MTC is a 16-bit down-counter representing blocks of 1..65536 operands.
// Programming 0 means 65536 operands: the first transfer wraps it to ffff.
// Count termination occurs only when a pre-transfer value of 1 reaches zero.
constexpr dma_count_result dma_count_after_transfer(std::uint16_t counter)
{
	return { std::uint16_t(counter - 1), counter == 1 };
}

constexpr std::uint8_t dma_status_after_count(std::uint8_t status, bool complete)
{
	return complete ? std::uint8_t((status & ~0x08U) | 0x80U) : status;
}

constexpr std::uint8_t dma_status_after_device_termination(std::uint8_t status)
{
	return std::uint8_t((status & ~0x08U) | 0xa0U);
}

constexpr std::uint8_t dma_status_after_error(std::uint8_t status)
{
	return std::uint8_t((status & ~0x08U) | 0x90U);
}

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
			return static_cast<interrupt_source>(index);
	return interrupt_source::none;
}

constexpr std::uint8_t i2c_status_after_data_access(std::uint8_t status)
{
	return (status | 0x10) & ~std::uint8_t(0x0c);
}

constexpr std::uint8_t uart_status_read_value(std::uint8_t status)
{
	return status & ~std::uint8_t(0x02);
}

constexpr std::uint8_t uart_control_after_misc_command(std::uint8_t control)
{
	return control & 0x0f;
}

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
