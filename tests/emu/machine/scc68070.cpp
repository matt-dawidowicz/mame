// license:BSD-3-Clause
// copyright-holders:MAMEdev Team

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "scc68070_helpers.h"

namespace
{

struct dma_reset_model
{
	std::uint8_t channel_status;
	std::uint8_t channel_error;
	std::uint8_t device_control;
	std::uint8_t operation_control;
	std::uint8_t sequence_control;
	std::uint8_t channel_control;
	std::uint16_t transfer_counter;
	std::uint32_t memory_address_counter;
	std::uint32_t device_address_counter;
};

} // anonymous namespace

TEST_CASE("SCC68070 interrupt arbitration selects the highest level", "[emu][machine][scc68070][irq]")
{
	REQUIRE(scc68070::highest_interrupt_level(std::array<std::uint8_t, 9>{}) == 0);
	REQUIRE(scc68070::highest_interrupt_level(std::array<std::uint8_t, 9>{ 2, 5, 4, 0, 0, 0, 0, 0, 7 }) == 7);
	REQUIRE(scc68070::highest_interrupt_level(std::array<std::uint8_t, 9>{ 2, 5, 4, 6, 0, 3, 1, 0, 0 }) == 6);
}

TEST_CASE("SCC68070 same-level interrupt acknowledgement follows documented priority", "[emu][machine][scc68070][irq]")
{
	std::array<std::uint8_t, 8> levels{ 5, 5, 5, 5, 5, 5, 5, 5 };
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
	REQUIRE(scc68070::first_interrupt_source(std::array<std::uint8_t, 8>{ 1, 2, 3, 4, 5, 6, 7, 1 }, 0) == scc68070::interrupt_source::none);
}

TEST_CASE("SCC68070 DMA address writes preserve untouched byte lanes", "[emu][machine][scc68070][dma]")
{
	REQUIRE(scc68070::dma_address_high_write(0x123456, 0x00ab, 0xffff) == 0xab3456);
	REQUIRE(scc68070::dma_address_high_write(0x123456, 0x00cd, 0x00ff) == 0xcd3456);
	REQUIRE(scc68070::dma_address_high_write(0x123456, 0xef00, 0xff00) == 0x123456);

	REQUIRE(scc68070::dma_address_low_write(0x123456, 0xbeef, 0xffff) == 0x12beef);
	REQUIRE(scc68070::dma_address_low_write(0x123456, 0x00aa, 0x00ff) == 0x1234aa);
	REQUIRE(scc68070::dma_address_low_write(0x123456, 0xbb00, 0xff00) == 0x12bb56);
}

TEST_CASE("SCC68070 DMA RESET preserves transfer and address counters", "[emu][machine][scc68070][dma][reset]")
{
	dma_reset_model channel{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1357, 0x123456, 0x654321 };

	scc68070::reset_dma_control_state(channel, 0x30, 0x02, 0x04);

	REQUIRE(channel.channel_status == 0x00);
	REQUIRE(channel.channel_error == 0x00);
	REQUIRE(channel.device_control == 0x30);
	REQUIRE(channel.operation_control == 0x02);
	REQUIRE(channel.sequence_control == 0x04);
	REQUIRE(channel.channel_control == 0x00);
	REQUIRE(channel.transfer_counter == 0x1357);
	REQUIRE(channel.memory_address_counter == 0x123456);
	REQUIRE(channel.device_address_counter == 0x654321);
}

TEST_CASE("SCC68070 I2C data access sets PIN and clears AL and AAS", "[emu][machine][scc68070][i2c]")
{
	for (unsigned status = 0; status <= 0xff; ++status)
	{
		const std::uint8_t result = scc68070::i2c_status_after_data_access(std::uint8_t(status));
		REQUIRE(result == ((status | 0x10) & ~0x0c));
	}
}

TEST_CASE("SCC68070 UART fixed and command bits retain only documented state", "[emu][machine][scc68070][uart]")
{
	for (unsigned value = 0; value <= 0xff; ++value)
	{
		REQUIRE(scc68070::uart_status_read_value(std::uint8_t(value)) == (value | 0x02));
		REQUIRE(scc68070::uart_control_after_misc_command(std::uint8_t(value)) == (value & 0x0f));
	}
}

TEST_CASE("SCC68070 UART baud clock selects internal divide-by-four or XCKI", "[emu][machine][scc68070][uart][timing]")
{
	REQUIRE(scc68070::uart_baud_clock(19'660'800, 7'372'800, false) == 4'915'200);
	REQUIRE(scc68070::uart_baud_clock(19'660'800, 7'372'800, true) == 7'372'800);
	REQUIRE(scc68070::uart_baud_clock(19'660'800, 0, true) == 0);
	REQUIRE(scc68070::uart_baud_divisor(0) == 65536);
	REQUIRE(scc68070::uart_baud_divisor(7) == 256);
	REQUIRE(scc68070::uart_baud_divisor(0x0f) == 256);
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
