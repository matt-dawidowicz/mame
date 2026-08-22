// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>
#include <limits>

#include "catch.hpp"

#include "cdislavehle_transport.h"


TEST_CASE(
	"CD-i SLAVE reset always disables asynchronous pointer input",
	"[emu][philips][cdi][pointer][transport]")
{
	REQUIRE(
		cdi_slave_transport::reset_pointer_input_enabled(false)
		== false);

	REQUIRE(
		cdi_slave_transport::reset_pointer_input_enabled(true)
		== false);
}


TEST_CASE(
	"CD-i SLAVE input parser rejects writes beyond its fixed command buffer",
	"[emu][philips][cdi][transport]")
{
	constexpr std::size_t capacity = 17;

	REQUIRE(cdi_slave_transport::input_write_fits(0, capacity));
	REQUIRE(cdi_slave_transport::input_write_fits(capacity - 1, capacity));
	REQUIRE_FALSE(cdi_slave_transport::input_write_fits(capacity, capacity));
	REQUIRE_FALSE(cdi_slave_transport::input_write_fits(capacity + 1, capacity));
	REQUIRE_FALSE(cdi_slave_transport::input_write_fits(0, 0));
}


TEST_CASE(
	"CD-i SLAVE response scheduling preserves the earliest pending interrupt",
	"[emu][philips][cdi][pointer][transport]")
{
	constexpr uint64_t NEVER = std::numeric_limits<uint64_t>::max();

	const std::array<uint64_t, 7> delays =
	{
		0,
		1,
		100,
		250,
		1'000,
		10'000,
		NEVER
	};

	uint64_t cases = 0;
	uint64_t failures = 0;

	uint64_t first_current = 0;
	uint64_t first_incoming = 0;
	uint64_t first_actual = 0;
	uint64_t first_expected = 0;

	bool have_first = false;

	for (const uint64_t current : delays)
	{
		for (const uint64_t incoming : delays)
		{
			const uint64_t expected =
				current < incoming ? current : incoming;

			const uint64_t actual =
				cdi_slave_transport::select_interrupt_delay(
					current, incoming);

			++cases;

			if (actual != expected)
			{
				++failures;

				if (!have_first)
				{
					have_first = true;
					first_current = current;
					first_incoming = incoming;
					first_actual = actual;
					first_expected = expected;
				}
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	if (have_first)
	{
		INFO(
			"first current=" << first_current <<
			" incoming=" << first_incoming <<
			" actual=" << first_actual <<
			" expected=" << first_expected);
	}

	REQUIRE(cases == 49);
	REQUIRE(failures == 0);
}


TEST_CASE(
	"CD-i SLAVE immediate pointer IRQ cannot be postponed by delayed disc response",
	"[emu][philips][cdi][pointer][transport]")
{
	constexpr uint64_t POINTER_IMMEDIATE = 0;
	constexpr uint64_t DISC_STATUS_DELAY = 250;

	const uint64_t result =
		cdi_slave_transport::select_interrupt_delay(
			POINTER_IMMEDIATE,
			DISC_STATUS_DELAY);

	REQUIRE(result == POINTER_IMMEDIATE);
}


TEST_CASE(
	"CD-i SLAVE immediate pointer IRQ preempts an existing delayed disc response",
	"[emu][philips][cdi][pointer][transport]")
{
	constexpr uint64_t DISC_STATUS_DELAY = 250;
	constexpr uint64_t POINTER_IMMEDIATE = 0;

	const uint64_t result =
		cdi_slave_transport::select_interrupt_delay(
			DISC_STATUS_DELAY,
			POINTER_IMMEDIATE);

	REQUIRE(result == POINTER_IMMEDIATE);
}


TEST_CASE(
	"CD-i SLAVE noninterrupting response cannot cancel an already scheduled IRQ",
	"[emu][philips][cdi][pointer][transport]")
{
	constexpr uint64_t NEVER = std::numeric_limits<uint64_t>::max();
	constexpr uint64_t PENDING_IRQ = 100;

	const uint64_t result =
		cdi_slave_transport::select_interrupt_delay(
			PENDING_IRQ,
			NEVER);

	REQUIRE(result == PENDING_IRQ);
}


TEST_CASE(
	"CD-i SLAVE interrupt deadline remains earliest across response sequences",
	"[emu][philips][cdi][pointer][transport]")
{
	constexpr uint64_t NEVER = std::numeric_limits<uint64_t>::max();

	const std::array<uint64_t, 5> delays =
	{
		0,
		1,
		100,
		250,
		NEVER
	};

	uint64_t cases = 0;
	uint64_t failures = 0;

	for (const uint64_t a : delays)
	{
		for (const uint64_t b : delays)
		{
			for (const uint64_t c : delays)
			{
				uint64_t actual = NEVER;

				actual =
					cdi_slave_transport::select_interrupt_delay(
						actual, a);

				actual =
					cdi_slave_transport::select_interrupt_delay(
						actual, b);

				actual =
					cdi_slave_transport::select_interrupt_delay(
						actual, c);

				uint64_t expected = a;

				if (b < expected)
					expected = b;

				if (c < expected)
					expected = c;

				++cases;

				if (actual != expected)
					++failures;
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	REQUIRE(cases == 125);
	REQUIRE(failures == 0);
}
