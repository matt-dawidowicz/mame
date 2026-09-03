// license:BSD-3-Clause
// copyright-holders:MAMEdev Team

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "scc68070_helpers.h"

TEST_CASE("SCC68070 UART register addresses match the Philips map", "[emu][machine][scc68070][uart][map]")
{
	REQUIRE(scc68070::UART_RHR_ADDRESS == 0x80002011);
	REQUIRE(scc68070::UART_THR_ADDRESS == 0x80002013);
	REQUIRE(scc68070::UART_USR_ADDRESS == 0x80002015);
	REQUIRE(scc68070::UART_UMR_ADDRESS == 0x80002019);
	REQUIRE(scc68070::UART_UCR_ADDRESS == 0x8000201d);
	REQUIRE(scc68070::UART_UCS_ADDRESS == 0x8000201f);
}

TEST_CASE("SCC68070 UART mode controls exact frame length and character mask", "[emu][machine][scc68070][uart][timing]")
{
	for (unsigned mode = 0; mode <= 0xff; ++mode)
	{
		const unsigned data_bits = 7 + (mode & 1);
		const unsigned stop_bits = 1 + ((mode >> 1) & 1);
		const unsigned parity_bits = (mode & 0x08) ? 1 : 0;
		REQUIRE(scc68070::uart_character_bits(std::uint8_t(mode)) == data_bits);
		REQUIRE(scc68070::uart_stop_bits(std::uint8_t(mode)) == stop_bits);
		REQUIRE(scc68070::uart_parity_enabled(std::uint8_t(mode)) == bool(mode & 0x08));
		REQUIRE(scc68070::uart_even_parity(std::uint8_t(mode)) == bool(mode & 0x04));
		REQUIRE(scc68070::uart_frame_bits(std::uint8_t(mode)) == 1 + data_bits + parity_bits + stop_bits);
		REQUIRE(scc68070::uart_character_mask(std::uint8_t(mode)) == ((mode & 1) ? 0xff : 0x7f));
	}

	REQUIRE(scc68070::uart_frame_bits(0x00) == 9);
	REQUIRE(scc68070::uart_frame_bits(0x01) == 10);
	REQUIRE(scc68070::uart_frame_bits(0x0a) == 11);
	REQUIRE(scc68070::uart_frame_bits(0x0b) == 12);
}

TEST_CASE("SCC68070 UART status bit 1 is reserved low", "[emu][machine][scc68070][uart]")
{
	for (unsigned value = 0; value <= 0xff; ++value)
		REQUIRE(scc68070::uart_status_read_value(std::uint8_t(value)) == (value & ~0x02));
}

TEST_CASE("SCC68070 Timer 1 and Timer 2 decode every control-register mode", "[emu][machine][scc68070][timer]")
{
	for (unsigned control = 0; control <= 0xff; ++control)
	{
		REQUIRE(static_cast<unsigned>(scc68070::timer_channel_mode(std::uint8_t(control), 1)) == ((control >> 4) & 3));
		REQUIRE(static_cast<unsigned>(scc68070::timer_channel_edge(std::uint8_t(control), 1)) == ((control >> 6) & 3));
		REQUIRE(static_cast<unsigned>(scc68070::timer_channel_mode(std::uint8_t(control), 2)) == (control & 3));
		REQUIRE(static_cast<unsigned>(scc68070::timer_channel_edge(std::uint8_t(control), 2)) == ((control >> 2) & 3));
	}
}

TEST_CASE("SCC68070 timer event selection distinguishes edge polarity", "[emu][machine][scc68070][timer]")
{
	using scc68070::timer_edge;
	REQUIRE_FALSE(scc68070::timer_edge_matches(timer_edge::inhibited, false, true));
	REQUIRE(scc68070::timer_edge_matches(timer_edge::rising, false, true));
	REQUIRE_FALSE(scc68070::timer_edge_matches(timer_edge::rising, true, false));
	REQUIRE(scc68070::timer_edge_matches(timer_edge::falling, true, false));
	REQUIRE_FALSE(scc68070::timer_edge_matches(timer_edge::falling, false, true));
	REQUIRE(scc68070::timer_edge_matches(timer_edge::both, false, true));
	REQUIRE(scc68070::timer_edge_matches(timer_edge::both, true, false));
	REQUIRE_FALSE(scc68070::timer_edge_matches(timer_edge::both, true, true));
}

TEST_CASE("SCC68070 timer status bits map every documented event", "[emu][machine][scc68070][timer]")
{
	using scc68070::timer_event;
	REQUIRE(scc68070::timer_status_bit(1, timer_event::match) == 0x40);
	REQUIRE(scc68070::timer_status_bit(1, timer_event::capture) == 0x20);
	REQUIRE(scc68070::timer_status_bit(1, timer_event::overflow) == 0x10);
	REQUIRE(scc68070::timer_status_bit(2, timer_event::match) == 0x08);
	REQUIRE(scc68070::timer_status_bit(2, timer_event::capture) == 0x04);
	REQUIRE(scc68070::timer_status_bit(2, timer_event::overflow) == 0x02);
}

TEST_CASE("SCC68070 external timer counters increment and flag only overflow", "[emu][machine][scc68070][timer]")
{
	for (unsigned value = 0; value <= 0xffff; ++value)
	{
		const auto result = scc68070::timer_count_external_event(std::uint16_t(value));
		REQUIRE(result.value == std::uint16_t(value + 1));
		REQUIRE(result.overflow == (value == 0xffff));
	}
}

TEST_CASE("SCC68070 DMA fixed and increment address modes are exact", "[emu][machine][scc68070][dma]")
{
	for (std::uint32_t size : { 1U, 2U })
	{
		auto result = scc68070::dma_address_after_transfer(0x123456, 0, size);
		REQUIRE(result.valid);
		REQUIRE(result.address == 0x123456);

		result = scc68070::dma_address_after_transfer(0x123456, 1, size);
		REQUIRE(result.valid);
		REQUIRE(result.address == 0x123456 + size);

		result = scc68070::dma_address_after_transfer(0xffffff, 1, size);
		REQUIRE(result.valid);
		REQUIRE(result.address == ((size - 1) & scc68070::DMA_ADDRESS_MASK));

		REQUIRE_FALSE(scc68070::dma_address_after_transfer(0x123456, 2, size).valid);
		REQUIRE_FALSE(scc68070::dma_address_after_transfer(0x123456, 3, size).valid);
	}
}

TEST_CASE("SCC68070 DMA transfer count and termination state are documented", "[emu][machine][scc68070][dma]")
{
	REQUIRE_FALSE(scc68070::dma_count_after_transfer(0).valid);
	for (unsigned counter = 1; counter <= 0xffff; ++counter)
	{
		const auto result = scc68070::dma_count_after_transfer(std::uint16_t(counter));
		REQUIRE(result.valid);
		REQUIRE(result.remaining == std::uint16_t(counter - 1));
		REQUIRE(result.complete == (counter == 1));
	}

	REQUIRE(scc68070::dma_status_after_count(0x08, false) == 0x08);
	REQUIRE(scc68070::dma_status_after_count(0x08, true) == 0x80);
	REQUIRE(scc68070::dma_status_after_device_termination(0x08) == 0xa0);
	REQUIRE(scc68070::dma_status_after_error(0x08) == 0x90);
}
