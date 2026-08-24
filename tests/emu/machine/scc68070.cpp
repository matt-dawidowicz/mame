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
