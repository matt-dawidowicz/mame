// license:BSD-3-Clause
// copyright-holders:MAMEdev Team

#include <array>

#include "catch.hpp"

#include "scc68070_helpers.h"

TEST_CASE("SCC68070 interrupt arbitration selects the highest level", "[emu][machine][scc68070][irq]")
{
	REQUIRE(scc68070::highest_interrupt_level(std::array<uint8_t, 9>{}) == 0);
	REQUIRE(scc68070::highest_interrupt_level(std::array<uint8_t, 9>{ 2, 5, 4, 0, 0, 0, 0, 0, 7 }) == 7);
	REQUIRE(scc68070::highest_interrupt_level(std::array<uint8_t, 9>{ 2, 5, 4, 6, 0, 3, 1, 0, 0 }) == 6);
}

TEST_CASE("SCC68070 same-level interrupt acknowledgement follows documented priority", "[emu][machine][scc68070][irq]")
{
	std::array<uint8_t, 8> levels{ 5, 5, 5, 5, 5, 5, 5, 5 };
	constexpr std::array<scc68070::interrupt_source, 8> order = {
		scc68070::interrupt_source::int1,
		scc68070::interrupt_source::int2,
		scc68070::interrupt_source::timer,
		scc68070::interrupt_source::uart_rx,
		scc68070::interrupt_source::uart_tx,
		scc68070::interrupt_source::i2c,
		scc68070::interrupt_source::dma1,
		scc68070::interrupt_source::dma2
	};

	for (std::size_t index = 0; index < levels.size(); ++index)
	{
		REQUIRE(scc68070::first_interrupt_source(levels, 5) == order[index]);
		levels[index] = 0;
	}
	REQUIRE(scc68070::first_interrupt_source(levels, 5) == scc68070::interrupt_source::none);
	REQUIRE(scc68070::first_interrupt_source(std::array<uint8_t, 8>{ 1, 2, 3, 4, 5, 6, 7, 1 }, 0) == scc68070::interrupt_source::none);
}

TEST_CASE("SCC68070 DMA address modes handle fixed, increment, wrap, and reserved encodings", "[emu][machine][scc68070][dma]")
{
	auto result = scc68070::dma_address_after_transfer(0x123456, 0x00, 2);
	REQUIRE(result.valid);
	REQUIRE(result.address == 0x123456);

	result = scc68070::dma_address_after_transfer(0x123456, 0x01, 1);
	REQUIRE(result.valid);
	REQUIRE(result.address == 0x123457);

	result = scc68070::dma_address_after_transfer(0xffffff, 0x01, 2);
	REQUIRE(result.valid);
	REQUIRE(result.address == 0x000001);

	REQUIRE_FALSE(scc68070::dma_address_after_transfer(0x123456, 0x02, 1).valid);
	REQUIRE_FALSE(scc68070::dma_address_after_transfer(0x123456, 0x03, 2).valid);
}

TEST_CASE("SCC68070 I2C data access sets PIN and clears AL and AAS", "[emu][machine][scc68070][i2c]")
{
	for (unsigned status = 0; status <= 0xff; ++status)
	{
		const uint8_t result = scc68070::i2c_status_after_data_access(uint8_t(status));
		REQUIRE(result == ((status | 0x10) & ~0x0c));
	}
}

TEST_CASE("SCC68070 UART fixed and command bits retain only documented state", "[emu][machine][scc68070][uart]")
{
	for (unsigned value = 0; value <= 0xff; ++value)
	{
		REQUIRE(scc68070::uart_status_read_value(uint8_t(value)) == (value | 0x02));
		REQUIRE(scc68070::uart_control_after_misc_command(uint8_t(value)) == (value & 0x0f));
	}
}

TEST_CASE("SCC68070 MMU register fields mask reserved bits and compose byte lanes", "[emu][machine][scc68070][mmu]")
{
	REQUIRE(scc68070::mmu_status_control_word(0xa5, 0x40) == 0xa540);
	REQUIRE(scc68070::mmu_control_value(0xff) == 0xc0);
	REQUIRE(scc68070::mmu_control_value(0x3f) == 0x00);
	REQUIRE(scc68070::mmu_segment_length(0xffff) == 0x07ff);
	REQUIRE(scc68070::mmu_segment_length(0x0401) == 0x0401);
	REQUIRE(scc68070::mmu_base_address(0xffff) == 0x3fff);
	REQUIRE(scc68070::mmu_base_address(0x2001) == 0x2001);
}

TEST_CASE("SCC68070 MMU disabled preserves the external 24-bit address", "[emu][machine][scc68070][mmu][translation]")
{
	const std::array<scc68070::mmu_descriptor, 8> descriptors{};
	constexpr std::array<uint32_t, 8> addresses = {
		0x000000, 0x000001, 0x0003ff, 0x123456,
		0x7fffff, 0xfffffe, 0xffffff, 0x12abcdef
	};

	for (uint32_t address : addresses)
	{
		const auto result = scc68070::mmu_translate(0x00, descriptors, address);
		REQUIRE(result.status == scc68070::mmu_translation_status::disabled);
		REQUIRE(result.physical_address == (address & scc68070::MMU_ADDRESS_MASK));
		REQUIRE(result.descriptor == 0xff);
	}
}

TEST_CASE("SCC68070 MMU selects every on-chip descriptor slot independently", "[emu][machine][scc68070][mmu][translation]")
{
	for (uint8_t slot = 0; slot < 8; ++slot)
	{
		std::array<scc68070::mmu_descriptor, 8> descriptors{};
		const uint16_t base = uint16_t(0x0100 + slot * 0x0020);
		descriptors[slot].segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | slot);
		descriptors[slot].length = 0x0010;
		descriptors[slot].base = base;

		const uint32_t logical = (uint32_t(slot) << 21) | (uint32_t(3) << 10) | 0x0155;
		const auto result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, logical);

		REQUIRE(result.status == scc68070::mmu_translation_status::translated);
		REQUIRE(result.descriptor == slot);
		REQUIRE(result.logical_segment == slot);
		REQUIRE(result.displacement == 3);
		REQUIRE(result.physical_address == ((uint32_t(base + 3) << 10) | 0x0155));
	}
}

TEST_CASE("SCC68070 MMU flush filtering and duplicate CAM matches are explicit", "[emu][machine][scc68070][mmu][translation]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	const uint32_t logical = (uint32_t(3) << 21) | 0x0123;

	descriptors[0].segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | 3);
	descriptors[0].length = 4;
	descriptors[0].base = 0x0100;
	descriptors[5].segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | 3);
	descriptors[5].length = 4;
	descriptors[5].base = 0x0200;

	auto result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, logical);
	REQUIRE(result.status == scc68070::mmu_translation_status::multiple_match);
	REQUIRE(result.descriptor == 0xff);
	REQUIRE(scc68070::mmu_status_for_fault(result) == scc68070::MMU_STATUS_NOT_PRESENT);

	// F=0 removes the descriptor from the associative comparison; no undocumented
	// slot priority is used to resolve duplicate active segment numbers.
	descriptors[0].segment = 3;
	result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, logical);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.descriptor == 5);
	REQUIRE(result.physical_address == ((uint32_t(0x0200) << 10) | 0x0123));
}

TEST_CASE("SCC68070 MMU enforces first last and out-of-range segment boundaries", "[emu][machine][scc68070][mmu][translation]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	descriptors[4].segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | 2);
	descriptors[4].length = 3; // Maximum displacement: block 3 => four 1 KiB blocks.
	descriptors[4].base = 0x0200;

	const uint8_t control = scc68070::MMU_CONTROL_ENABLE;
	const uint32_t segment_start = uint32_t(2) << 21;

	auto result = scc68070::mmu_translate(control, descriptors, segment_start);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.physical_address == (uint32_t(0x0200) << 10));

	const uint32_t last_byte = segment_start | (uint32_t(3) << 10) | 0x03ff;
	result = scc68070::mmu_translate(control, descriptors, last_byte);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.physical_address == ((uint32_t(0x0203) << 10) | 0x03ff));

	result = scc68070::mmu_translate(control, descriptors, segment_start | (uint32_t(4) << 10));
	REQUIRE(result.status == scc68070::mmu_translation_status::length_violation);
	REQUIRE(result.descriptor == 4);

	result = scc68070::mmu_translate(control, descriptors, segment_start - 1);
	REQUIRE(result.status == scc68070::mmu_translation_status::not_present);
}

TEST_CASE("SCC68070 MMU minimum and maximum segment lengths follow mode geometry", "[emu][machine][scc68070][mmu][translation]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	descriptors[0].segment = scc68070::MMU_DESCRIPTOR_FLUSH_VALID;
	descriptors[0].base = 0;

	// Mode 1 length is the maximum 11-bit displacement.  Zero therefore
	// describes one 1 KiB block, while 0x7ff describes 2048 blocks (2 MiB).
	descriptors[0].length = 0;
	auto result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, 0x0003ff);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, 0x000400);
	REQUIRE(result.status == scc68070::mmu_translation_status::length_violation);

	descriptors[0].length = 0x07ff;
	result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, 0x1fffff);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.displacement == 0x07ff);

	// Mode 2 uses a seven-bit segment number and the seven MSBs of the stored
	// length field.  The low four length bits do not alter the block boundary.
	const uint8_t mode2_control = scc68070::MMU_CONTROL_ENABLE | scc68070::MMU_CONTROL_SEGMENT_NUMBER;
	descriptors = {};
	descriptors[6].segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | 0x55);
	descriptors[6].base = 0x0100;
	descriptors[6].length = 0x000f;
	const uint32_t mode2_start = uint32_t(0x55) << 17;

	result = scc68070::mmu_translate(mode2_control, descriptors, mode2_start | 0x03ff);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	result = scc68070::mmu_translate(mode2_control, descriptors, mode2_start | 0x0400);
	REQUIRE(result.status == scc68070::mmu_translation_status::length_violation);

	descriptors[6].length = 0x07ff;
	const uint32_t mode2_last = mode2_start | (uint32_t(0x7f) << 10) | 0x03ff;
	result = scc68070::mmu_translate(mode2_control, descriptors, mode2_last);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.displacement == 0x007f);
	REQUIRE(scc68070::mmu_effective_segment_length(mode2_control, 0x07f0) == 0x007f);
	REQUIRE(scc68070::mmu_effective_segment_length(mode2_control, 0x07ff) == 0x007f);
}

TEST_CASE("SCC68070 MMU enforces supervisor execute read and write attributes independently", "[emu][machine][scc68070][mmu][protection]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	auto &desc = descriptors[0];
	desc.segment = scc68070::MMU_DESCRIPTOR_FLUSH_VALID;
	desc.length = 0x07ff;
	desc.base = 0x0100;
	const uint8_t control = scc68070::MMU_CONTROL_ENABLE;
	const uint32_t logical = 0x001234;

	desc.attr = scc68070::MMU_ATTRIBUTE_EXECUTE;
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::execute, false).status == scc68070::mmu_translation_status::translated);
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::read, false).status == scc68070::mmu_translation_status::attribute_violation);
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::write, false).status == scc68070::mmu_translation_status::attribute_violation);

	desc.attr = scc68070::MMU_ATTRIBUTE_READ;
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::execute, false).status == scc68070::mmu_translation_status::attribute_violation);
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::read, false).status == scc68070::mmu_translation_status::translated);
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::write, false).status == scc68070::mmu_translation_status::attribute_violation);

	desc.attr = scc68070::MMU_ATTRIBUTE_WRITE;
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::execute, false).status == scc68070::mmu_translation_status::attribute_violation);
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::read, false).status == scc68070::mmu_translation_status::attribute_violation);
	REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, scc68070::mmu_access_type::write, false).status == scc68070::mmu_translation_status::translated);

	desc.attr = scc68070::MMU_ATTRIBUTE_SUPERVISOR
		| scc68070::MMU_ATTRIBUTE_EXECUTE
		| scc68070::MMU_ATTRIBUTE_READ
		| scc68070::MMU_ATTRIBUTE_WRITE;
	for (scc68070::mmu_access_type access : { scc68070::mmu_access_type::execute, scc68070::mmu_access_type::read, scc68070::mmu_access_type::write })
	{
		REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, access, false).status == scc68070::mmu_translation_status::attribute_violation);
		REQUIRE(scc68070::mmu_translate_access(control, descriptors, logical, access, true).status == scc68070::mmu_translation_status::translated);
	}

	// Disabling the MMU bypasses protection as well as address translation.
	desc.attr = 0;
	REQUIRE(scc68070::mmu_translate_access(0, descriptors, logical, scc68070::mmu_access_type::write, false).status == scc68070::mmu_translation_status::disabled);
}

TEST_CASE("SCC68070 MMU status records fault class and descriptor attributes", "[emu][machine][scc68070][mmu][status]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	auto &desc = descriptors[0];
	desc.segment = scc68070::MMU_DESCRIPTOR_FLUSH_VALID;
	desc.length = 0;
	desc.base = 0x0200;
	desc.attr = scc68070::MMU_ATTRIBUTE_SUPERVISOR
		| scc68070::MMU_ATTRIBUTE_EXECUTE
		| scc68070::MMU_ATTRIBUTE_READ
		| scc68070::MMU_ATTRIBUTE_WRITE;

	auto result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, 0x000400);
	REQUIRE(result.status == scc68070::mmu_translation_status::length_violation);
	REQUIRE(scc68070::mmu_status_for_fault(result) == 0x2f);

	result = scc68070::mmu_translate_access(scc68070::MMU_CONTROL_ENABLE, descriptors, 0x000100, scc68070::mmu_access_type::read, false);
	REQUIRE(result.status == scc68070::mmu_translation_status::attribute_violation);
	REQUIRE(scc68070::mmu_status_for_fault(result) == 0x1f);

	descriptors = {};
	result = scc68070::mmu_translate(scc68070::MMU_CONTROL_ENABLE, descriptors, 0x000100);
	REQUIRE(result.status == scc68070::mmu_translation_status::not_present);
	REQUIRE(scc68070::mmu_status_for_fault(result) == 0x80);
}

TEST_CASE("SCC68070 MMU stack segments grow from high to low logical and physical addresses", "[emu][machine][scc68070][mmu][stack]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	auto &desc = descriptors[2];
	desc.segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | 3);
	desc.attr = scc68070::MMU_ATTRIBUTE_STACK | scc68070::MMU_ATTRIBUTE_READ | scc68070::MMU_ATTRIBUTE_WRITE;
	desc.base = 0x0200;
	desc.length = 0;

	const uint8_t control = scc68070::MMU_CONTROL_ENABLE;
	const uint32_t segment_start = uint32_t(3) << 21;
	const uint32_t top_block = segment_start | (uint32_t(0x07ff) << 10);

	auto result = scc68070::mmu_translate(control, descriptors, top_block | 0x03ff);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.displacement == 0x07ff);
	REQUIRE(result.effective_displacement == 0);
	REQUIRE(result.physical_address == ((uint32_t(0x0200) << 10) | 0x03ff));

	result = scc68070::mmu_translate(control, descriptors, top_block - 0x0400);
	REQUIRE(result.status == scc68070::mmu_translation_status::length_violation);
	REQUIRE(result.effective_displacement == 1);
	REQUIRE(scc68070::mmu_status_for_fault(result) == (scc68070::MMU_STATUS_STACK | scc68070::MMU_STATUS_LENGTH | scc68070::MMU_STATUS_READ | scc68070::MMU_STATUS_WRITE));

	desc.length = 3;
	result = scc68070::mmu_translate(control, descriptors, top_block - 0x0c00);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.effective_displacement == 3);
	REQUIRE(result.physical_address == (uint32_t(0x01fd) << 10));

	result = scc68070::mmu_translate(control, descriptors, top_block - 0x1000);
	REQUIRE(result.status == scc68070::mmu_translation_status::length_violation);

	desc.length = 0x07ff;
	result = scc68070::mmu_translate(control, descriptors, segment_start);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.effective_displacement == 0x07ff);
	REQUIRE(result.physical_address == (uint32_t((0x0200 - 0x07ff) & 0x3fff) << 10));
}

TEST_CASE("SCC68070 MMU mode-2 stack length uses the seven effective length bits", "[emu][machine][scc68070][mmu][stack]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	auto &desc = descriptors[7];
	desc.segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | 0x22);
	desc.attr = scc68070::MMU_ATTRIBUTE_STACK | scc68070::MMU_ATTRIBUTE_READ;
	desc.base = 0x0300;
	desc.length = 0x001f; // Effective length 1; low four bits are ignored.

	const uint8_t control = scc68070::MMU_CONTROL_ENABLE | scc68070::MMU_CONTROL_SEGMENT_NUMBER;
	const uint32_t segment_start = uint32_t(0x22) << 17;
	const uint32_t top_block = segment_start | (uint32_t(0x7f) << 10);

	auto result = scc68070::mmu_translate(control, descriptors, top_block);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.physical_address == (uint32_t(0x0300) << 10));

	result = scc68070::mmu_translate(control, descriptors, top_block - 0x0400);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.physical_address == (uint32_t(0x02ff) << 10));

	result = scc68070::mmu_translate(control, descriptors, top_block - 0x0800);
	REQUIRE(result.status == scc68070::mmu_translation_status::length_violation);
}

TEST_CASE("SCC68070 MMU exposes a cross-block operand fault before physical access", "[emu][machine][scc68070][mmu][boundary]")
{
	std::array<scc68070::mmu_descriptor, 8> descriptors{};
	descriptors[0].segment = scc68070::MMU_DESCRIPTOR_FLUSH_VALID;
	descriptors[0].attr = scc68070::MMU_ATTRIBUTE_READ | scc68070::MMU_ATTRIBUTE_WRITE;
	descriptors[0].length = 0;
	descriptors[0].base = 0x0100;
	const uint8_t control = scc68070::MMU_CONTROL_ENABLE;

	const auto first = scc68070::mmu_translate_access(control, descriptors, 0x0003ff, scc68070::mmu_access_type::write, false);
	const auto second = scc68070::mmu_translate_access(control, descriptors, 0x000400, scc68070::mmu_access_type::write, false);
	REQUIRE(first.status == scc68070::mmu_translation_status::translated);
	REQUIRE(first.physical_address == ((uint32_t(0x0100) << 10) | 0x03ff));
	REQUIRE(second.status == scc68070::mmu_translation_status::length_violation);

	// The CPU access layer preflights both byte translations before issuing either
	// physical write, making this second-byte failure atomic at the emulator level.
	REQUIRE_FALSE(scc68070::mmu_translation_succeeded(second));
}

TEST_CASE("SCC68070 MMU helpers accept the live descriptor register layout without reinterpret casts", "[emu][machine][scc68070][mmu][integration]")
{
	struct live_descriptor_shape
	{
		uint16_t attr;
		uint16_t length;
		uint8_t undefined;
		uint8_t segment;
		uint16_t base;
	};

	live_descriptor_shape descriptors[8]{};
	descriptors[3].attr = scc68070::MMU_ATTRIBUTE_READ;
	descriptors[3].length = 4;
	descriptors[3].segment = uint8_t(scc68070::MMU_DESCRIPTOR_FLUSH_VALID | 1);
	descriptors[3].base = 0x0120;

	const auto result = scc68070::mmu_translate_access(
		scc68070::MMU_CONTROL_ENABLE,
		descriptors,
		(uint32_t(1) << 21) | 0x0555,
		scc68070::mmu_access_type::read,
		false);
	REQUIRE(result.status == scc68070::mmu_translation_status::translated);
	REQUIRE(result.descriptor == 3);
	REQUIRE(result.physical_address == ((uint32_t(0x0121) << 10) | 0x0155));
}
