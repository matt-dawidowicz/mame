// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdislavehle_transport.h"


TEST_CASE(
	"CD-i SLAVE delayed responses are not readable before their deadline",
	"[emu][philips][cdi][slave][pointer][transport][readiness][irq2]")
{
	REQUIRE(cdi_slave_transport::response_ready_on_prepare(true));
	REQUIRE_FALSE(cdi_slave_transport::response_ready_on_prepare(false));
}


TEST_CASE(
	"CD-i SLAVE response readability requires both pending data and readiness",
	"[emu][philips][cdi][slave][pointer][transport][readiness][irq2]")
{
	REQUIRE_FALSE(cdi_slave_transport::response_readable(false, false));
	REQUIRE_FALSE(cdi_slave_transport::response_readable(false, true));
	REQUIRE_FALSE(cdi_slave_transport::response_readable(true, false));
	REQUIRE(cdi_slave_transport::response_readable(true, true));
}


TEST_CASE(
	"CD-i SLAVE only ready responses keep the interrupt asserted",
	"[emu][philips][cdi][slave][pointer][transport][readiness][irq2]")
{
	REQUIRE_FALSE(cdi_slave_transport::response_holds_irq(false, false));
	REQUIRE_FALSE(cdi_slave_transport::response_holds_irq(false, true));
	REQUIRE_FALSE(cdi_slave_transport::response_holds_irq(true, false));
	REQUIRE(cdi_slave_transport::response_holds_irq(true, true));
}


TEST_CASE(
	"CD-i SLAVE four-channel IRQ arbitration ignores pending but unready responses",
	"[emu][philips][cdi][slave][pointer][transport][readiness][irq2]")
{
	// State:
	// 0 = empty
	// 1 = pending but not ready
	// 2 = pending and ready

	uint64_t cases = 0;
	uint64_t failures = 0;

	for (unsigned a = 0; a < 3; ++a)
	for (unsigned b = 0; b < 3; ++b)
	for (unsigned c = 0; c < 3; ++c)
	for (unsigned d = 0; d < 3; ++d)
	{
		const std::array<unsigned, 4> state { a, b, c, d };

		bool actual_irq = false;
		bool expected_irq = false;

		for (const unsigned value : state)
		{
			const bool pending = value != 0;
			const bool ready = value == 2;

			if (cdi_slave_transport::response_holds_irq(pending, ready))
				actual_irq = true;

			if (pending && ready)
				expected_irq = true;
		}

		++cases;

		if (actual_irq != expected_irq)
			++failures;
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	REQUIRE(cases == 81);
	REQUIRE(failures == 0);
}


TEST_CASE(
	"CD-i SLAVE delayed disc response cannot hold IRQ after immediate pointer response is consumed",
	"[emu][philips][cdi][slave][pointer][transport][readiness][irq2]")
{
	// Pointer packet has just been fully consumed.
	const bool pointer_pending = false;
	const bool pointer_ready = false;

	// Disc response exists, but its deadline has not arrived.
	const bool disc_pending = true;
	const bool disc_ready = false;

	const bool irq =
		cdi_slave_transport::response_holds_irq(
			pointer_pending, pointer_ready) ||
		cdi_slave_transport::response_holds_irq(
			disc_pending, disc_ready);

	REQUIRE_FALSE(irq);
}
