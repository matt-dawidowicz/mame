// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "catch.hpp"

#include "cdislavehle_pointer.h"
#include "cdislavehle_transport.h"

namespace
{

uint8_t expected_button_field(uint8_t buttons)
{
	uint8_t field = 0x08;
	if (buttons & 0x01U)
		field |= 0x10U;
	if (buttons & 0x02U)
		field |= 0x20U;
	if (buttons & 0x04U)
		field |= 0x30U;
	return field;
}

} // anonymous namespace


TEST_CASE(
	"CD-i SLAVE channel bounds reject every index outside the four-channel transport",
	"[emu][philips][cdi][hardening][transport]")
{
	constexpr std::size_t CHANNELS = 4;

	for (std::size_t index = 0; index < 16; ++index)
	{
		INFO("index=" << index);
		REQUIRE(cdi_slave_transport::channel_index_valid(index, CHANNELS) == (index < CHANNELS));
	}

	REQUIRE_FALSE(cdi_slave_transport::channel_index_valid(0, 0));
	REQUIRE_FALSE(cdi_slave_transport::channel_index_valid(
		std::numeric_limits<std::size_t>::max(), CHANNELS));
}


TEST_CASE(
	"CD-i SLAVE response windows never escape the fixed output buffer",
	"[emu][philips][cdi][hardening][transport]")
{
	constexpr std::size_t CAPACITY = 4;

	for (std::size_t index = 0; index <= CAPACITY + 2; ++index)
	{
		for (std::size_t count = 0; count <= CAPACITY + 2; ++count)
		{
			const bool expected = count == 0
				? index == 0
				: index < CAPACITY && count <= CAPACITY - index;

			INFO("index=" << index << " count=" << count);
			REQUIRE(cdi_slave_transport::response_window_fits(index, count, CAPACITY) == expected);
		}
	}

	REQUIRE_FALSE(cdi_slave_transport::response_window_fits(
		std::numeric_limits<std::size_t>::max(), 1, CAPACITY));
	REQUIRE_FALSE(cdi_slave_transport::response_window_fits(
		1, std::numeric_limits<std::size_t>::max(), CAPACITY));
	REQUIRE_FALSE(cdi_slave_transport::response_window_fits(
		std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), CAPACITY));
}


TEST_CASE(
	"CD-i SLAVE input writes accept exactly the fixed command-buffer range",
	"[emu][philips][cdi][hardening][transport]")
{
	constexpr std::size_t CAPACITY = 17;

	for (std::size_t index = 0; index <= CAPACITY + 2; ++index)
	{
		INFO("index=" << index);
		REQUIRE(cdi_slave_transport::input_write_fits(index, CAPACITY) == (index < CAPACITY));
	}

	REQUIRE_FALSE(cdi_slave_transport::input_write_fits(
		std::numeric_limits<std::size_t>::max(), CAPACITY));
}


TEST_CASE(
	"CD-i SLAVE pointer readback ignores every non-pointer host button bit",
	"[emu][philips][cdi][hardening][pointer]")
{
	constexpr std::array<cdi_slave_pointer::position, 5> positions =
	{{
		{ 0, 0 },
		{ 1, 1 },
		{ 383, 279 },
		{ 766, 558 },
		{ 767, 559 }
	}};

	for (const auto &position : positions)
	{
		for (unsigned buttons = 0; buttons <= 0xff; ++buttons)
		{
			const auto packet = cdi_slave_pointer::encode_readback(
				position.x, position.y, uint8_t(buttons));

			INFO("x=" << position.x << " y=" << position.y << " buttons=" << buttons);
			REQUIRE((packet[0] & 0x38U) == expected_button_field(uint8_t(buttons)));
			REQUIRE(((packet[0] & 0x07U) << 7 | (packet[1] & 0x7fU)) == uint16_t(position.x));
			REQUIRE(((packet[2] & 0x07U) << 7 | (packet[3] & 0x7fU)) == uint16_t(position.y));
		}
	}
}


TEST_CASE(
	"CD-i SLAVE pointer clamps the complete signed 32-bit input extremes",
	"[emu][philips][cdi][hardening][pointer]")
{
	REQUIRE(cdi_slave_pointer::clamp_x(std::numeric_limits<int32_t>::min()) == 0);
	REQUIRE(cdi_slave_pointer::clamp_x(-1) == 0);
	REQUIRE(cdi_slave_pointer::clamp_x(0) == 0);
	REQUIRE(cdi_slave_pointer::clamp_x(767) == 767);
	REQUIRE(cdi_slave_pointer::clamp_x(768) == 767);
	REQUIRE(cdi_slave_pointer::clamp_x(std::numeric_limits<int32_t>::max()) == 767);

	REQUIRE(cdi_slave_pointer::clamp_y(std::numeric_limits<int32_t>::min()) == 0);
	REQUIRE(cdi_slave_pointer::clamp_y(-1) == 0);
	REQUIRE(cdi_slave_pointer::clamp_y(0) == 0);
	REQUIRE(cdi_slave_pointer::clamp_y(559) == 559);
	REQUIRE(cdi_slave_pointer::clamp_y(560) == 559);
	REQUIRE(cdi_slave_pointer::clamp_y(std::numeric_limits<int32_t>::max()) == 559);
}
