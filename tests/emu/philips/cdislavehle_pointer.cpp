// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdislavehle_pointer.h"

namespace
{

int16_t expected_signed_delta(uint16_t previous, uint16_t current)
{
	const uint16_t raw = uint16_t(current - previous);

	if (raw <= 0x7fffU)
		return int16_t(raw);

	return int16_t(int32_t(raw) - 0x10000);
}

uint8_t donor_button_field(uint8_t buttons)
{
	uint8_t field = 0x01;

	if (buttons & 0x01)
		field |= 0x02;

	if (buttons & 0x02)
		field |= 0x04;

	if (buttons & 0x04)
		field |= 0x06;

	return uint8_t(field << 3);
}

struct first_failure
{
	bool valid = false;
	uint32_t a = 0;
	uint32_t b = 0;
	uint32_t c = 0;
	uint32_t actual_a = 0;
	uint32_t actual_b = 0;
};

} // anonymous namespace


TEST_CASE(
	"CD-i SLAVE pointer set-position decoding reconstructs every 10-bit coordinate",
	"[emu][philips][cdi][pointer]")
{
	uint64_t cases = 0;
	uint64_t failures = 0;
	first_failure first;

	for (uint32_t y = 0; y < 1024; ++y)
	{
		for (uint32_t x = 0; x < 1024; ++x)
		{
			// SLAVE set-position command representation:
			// byte0 high bits select the command, low 6 bits contain Y.
			// byte1 contains X high 3 bits and Y high 4 bits.
			// byte2 contains X low 7 bits.
			const uint8_t byte0 = uint8_t(0xc0U | (y & 0x3fU));
			const uint8_t byte1 = uint8_t(
				(((x >> 7) & 0x07U) << 4) |
				((y >> 6) & 0x0fU));
			const uint8_t byte2 = uint8_t(x & 0x7fU);

			const auto decoded =
				cdi_slave_pointer::decode_set_position(byte0, byte1, byte2);

			++cases;

			if (decoded.x != int16_t(x) || decoded.y != int16_t(y))
			{
				++failures;

				if (!first.valid)
				{
					first.valid = true;
					first.a = x;
					first.b = y;
					first.actual_a = uint16_t(decoded.x);
					first.actual_b = uint16_t(decoded.y);
				}
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	if (first.valid)
	{
		INFO(
			"first expected=(" << first.a << "," << first.b <<
			") actual=(" << first.actual_a << "," << first.actual_b << ")");
	}

	REQUIRE(cases == 1'048'576);
	REQUIRE(failures == 0);
}


TEST_CASE(
	"CD-i SLAVE pointer readback exhaustively preserves screen coordinates and donor button encoding",
	"[emu][philips][cdi][pointer]")
{
	uint64_t cases = 0;
	uint64_t failures = 0;
	first_failure first;

	for (uint32_t y = 0; y <= 559; ++y)
	{
		for (uint32_t x = 0; x <= 767; ++x)
		{
			for (uint32_t buttons = 0; buttons < 8; ++buttons)
			{
				const auto packet = cdi_slave_pointer::encode_readback(
					int16_t(x), int16_t(y), uint8_t(buttons));

				const uint32_t decoded_x =
					((packet[0] & 0x07U) << 7) |
					(packet[1] & 0x7fU);

				const uint32_t decoded_y =
					((packet[2] & 0x07U) << 7) |
					(packet[3] & 0x7fU);

				const uint8_t expected_buttons =
					donor_button_field(uint8_t(buttons));

				const uint8_t actual_buttons =
					uint8_t(packet[0] & 0x38U);

				++cases;

				if (
					decoded_x != x ||
					decoded_y != y ||
					actual_buttons != expected_buttons)
				{
					++failures;

					if (!first.valid)
					{
						first.valid = true;
						first.a = x;
						first.b = y;
						first.c = buttons;
						first.actual_a = actual_buttons;
						first.actual_b = expected_buttons;
					}
				}
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	if (first.valid)
	{
		INFO(
			"first x=" << first.a <<
			" y=" << first.b <<
			" buttons=" << first.c <<
			" actual_button_field=" << first.actual_a <<
			" expected_button_field=" << first.actual_b);
	}

	REQUIRE(cases == 3'440'640);
	REQUIRE(failures == 0);
}


TEST_CASE(
	"CD-i SLAVE pointer host counters handle every one-step 16-bit wrap transition",
	"[emu][philips][cdi][pointer]")
{
	uint64_t cases = 0;
	uint64_t failures = 0;
	first_failure first;

	for (uint32_t previous = 0; previous <= 0xffffU; ++previous)
	{
		for (const int delta : { -1, 1 })
		{
			const uint16_t current =
				uint16_t(uint32_t(previous) + uint32_t(delta));

			const int16_t actual =
				cdi_slave_pointer::host_delta(uint16_t(previous), current);

			++cases;

			if (actual != delta)
			{
				++failures;

				if (!first.valid)
				{
					first.valid = true;
					first.a = previous;
					first.b = current;
					first.c = uint16_t(actual);
				}
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	if (first.valid)
	{
		INFO(
			"first previous=" << first.a <<
			" current=" << first.b <<
			" actual_raw=" << first.c);
	}

	REQUIRE(cases == 131'072);
	REQUIRE(failures == 0);
}


TEST_CASE(
	"CD-i SLAVE pointer host delta matches signed modulo displacement across boundary magnitudes",
	"[emu][philips][cdi][pointer]")
{
	const std::array<int32_t, 19> deltas =
	{
		-32768, -32767,
		-1024, -256, -255, -128, -127,
		-2, -1, 0, 1, 2,
		127, 128, 255, 256, 1024,
		32766, 32767
	};

	uint64_t cases = 0;
	uint64_t failures = 0;
	first_failure first;

	for (uint32_t previous = 0; previous <= 0xffffU; ++previous)
	{
		for (const int32_t requested_delta : deltas)
		{
			const uint16_t current =
				uint16_t(previous + uint32_t(requested_delta));

			const int16_t expected =
				expected_signed_delta(uint16_t(previous), current);

			const int16_t actual =
				cdi_slave_pointer::host_delta(uint16_t(previous), current);

			++cases;

			if (actual != expected)
			{
				++failures;

				if (!first.valid)
				{
					first.valid = true;
					first.a = previous;
					first.b = current;
					first.c = uint16_t(actual);
					first.actual_a = uint16_t(expected);
				}
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	if (first.valid)
	{
		INFO(
			"first previous=" << first.a <<
			" current=" << first.b <<
			" actual_raw=" << first.c <<
			" expected_raw=" << first.actual_a);
	}

	REQUIRE(cases == uint64_t(65'536) * deltas.size());
	REQUIRE(failures == 0);
}


TEST_CASE(
	"CD-i SLAVE pointer treats 0xffff as valid host counter data after initialization",
	"[emu][philips][cdi][pointer]")
{
	// The first sample must be processed even when it happens to match the
	// reset placeholders exactly.  Only an initialized, unchanged sample may
	// take the early-return path.
	REQUIRE(cdi_slave_pointer::host_sample_changed(
		false,
		0xffff,
		0xffff,
		0x00,
		0xffff,
		0xffff,
		0x00));

	REQUIRE_FALSE(cdi_slave_pointer::host_sample_changed(
		true,
		0xffff,
		0xffff,
		0x00,
		0xffff,
		0xffff,
		0x00));

	REQUIRE(cdi_slave_pointer::host_sample_changed(
		true, 0xffff, 0xffff, 0x00, 0x0000, 0xffff, 0x00));

	REQUIRE(cdi_slave_pointer::host_sample_changed(
		true, 0xffff, 0xffff, 0x00, 0xffff, 0x0000, 0x00));

	REQUIRE(cdi_slave_pointer::host_sample_changed(
		true, 0xffff, 0xffff, 0x00, 0xffff, 0xffff, 0x01));

	const auto initial = cdi_slave_pointer::decode_host_movement(
		false,
		0x0000,
		0x0000,
		0xffff,
		0x2468);

	REQUIRE(initial.x == 0);
	REQUIRE(initial.y == 0);

	// The first sample initialized both counters.  X=0xffff is therefore
	// real data, not an initialization sentinel.  The next poll must retain
	// both the X wrap and simultaneous Y motion.
	const auto next = cdi_slave_pointer::decode_host_movement(
		true,
		0xffff,
		0x2468,
		0x0000,
		0x2467);

	REQUIRE(next.x == 1);
	REQUIRE(next.y == -1);
}


TEST_CASE(
	"CD-i SLAVE pointer clamping is exhaustive around every legal X and Y position",
	"[emu][philips][cdi][pointer]")
{
	uint64_t cases = 0;
	uint64_t failures = 0;
	first_failure first;

	for (int32_t x = 0; x <= 767; ++x)
	{
		for (int32_t delta = -1024; delta <= 1024; ++delta)
		{
			const int32_t raw = x + delta;
			const int16_t expected =
				raw < 0 ? 0 : (raw > 767 ? 767 : int16_t(raw));

			const int16_t actual = cdi_slave_pointer::clamp_x(raw);

			++cases;

			if (actual != expected)
			{
				++failures;

				if (!first.valid)
				{
					first.valid = true;
					first.a = x;
					first.b = uint32_t(delta);
					first.c = uint16_t(actual);
					first.actual_a = uint16_t(expected);
				}
			}
		}
	}

	for (int32_t y = 0; y <= 559; ++y)
	{
		for (int32_t delta = -1024; delta <= 1024; ++delta)
		{
			const int32_t raw = y + delta;
			const int16_t expected =
				raw < 0 ? 0 : (raw > 559 ? 559 : int16_t(raw));

			const int16_t actual = cdi_slave_pointer::clamp_y(raw);

			++cases;

			if (actual != expected)
			{
				++failures;

				if (!first.valid)
				{
					first.valid = true;
					first.a = y;
					first.b = uint32_t(delta);
					first.c = uint16_t(actual);
					first.actual_a = uint16_t(expected);
				}
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	REQUIRE(cases == uint64_t(768 + 560) * 2049);
	REQUIRE(failures == 0);
}


TEST_CASE(
	"CD-i SLAVE pointer can escape every screen corner by one unit",
	"[emu][philips][cdi][pointer]")
{
	REQUIRE(cdi_slave_pointer::clamp_x(0 - 1) == 0);
	REQUIRE(cdi_slave_pointer::clamp_x(0 + 1) == 1);
	REQUIRE(cdi_slave_pointer::clamp_x(767 - 1) == 766);
	REQUIRE(cdi_slave_pointer::clamp_x(767 + 1) == 767);

	REQUIRE(cdi_slave_pointer::clamp_y(0 - 1) == 0);
	REQUIRE(cdi_slave_pointer::clamp_y(0 + 1) == 1);
	REQUIRE(cdi_slave_pointer::clamp_y(559 - 1) == 558);
	REQUIRE(cdi_slave_pointer::clamp_y(559 + 1) == 559);
}


TEST_CASE(
	"CD-i SLAVE BIOS reposition followed by unit host movement preserves every legal starting point",
	"[emu][philips][cdi][pointer]")
{
	constexpr uint16_t host_origin = 0x4a55;

	uint64_t cases = 0;
	uint64_t failures = 0;
	first_failure first;

	for (uint32_t y = 0; y <= 559; ++y)
	{
		for (uint32_t x = 0; x <= 767; ++x)
		{
			const uint8_t byte0 = uint8_t(0xc0U | (y & 0x3fU));
			const uint8_t byte1 = uint8_t(
				(((x >> 7) & 0x07U) << 4) |
				((y >> 6) & 0x0fU));
			const uint8_t byte2 = uint8_t(x & 0x7fU);

			const auto start =
				cdi_slave_pointer::decode_set_position(byte0, byte1, byte2);

			for (const std::array<int, 2> movement :
				{
					std::array<int, 2>{ -1, 0 },
					std::array<int, 2>{  1, 0 },
					std::array<int, 2>{  0,-1 },
					std::array<int, 2>{  0, 1 }
				})
			{
				const uint16_t host_x =
					uint16_t(host_origin + movement[0]);
				const uint16_t host_y =
					uint16_t(host_origin + movement[1]);

				const int16_t dx =
					cdi_slave_pointer::host_delta(host_origin, host_x);
				const int16_t dy =
					cdi_slave_pointer::host_delta(host_origin, host_y);

				const int16_t actual_x =
					cdi_slave_pointer::clamp_x(int32_t(start.x) + dx);
				const int16_t actual_y =
					cdi_slave_pointer::clamp_y(int32_t(start.y) + dy);

				const int16_t expected_x =
					cdi_slave_pointer::clamp_x(int32_t(x) + movement[0]);
				const int16_t expected_y =
					cdi_slave_pointer::clamp_y(int32_t(y) + movement[1]);

				++cases;

				if (actual_x != expected_x || actual_y != expected_y)
				{
					++failures;

					if (!first.valid)
					{
						first.valid = true;
						first.a = x;
						first.b = y;
						first.c =
							uint32_t((movement[0] + 1) * 3 + (movement[1] + 1));
						first.actual_a =
							(uint32_t(uint16_t(actual_x)) << 16) |
							uint16_t(actual_y);
						first.actual_b =
							(uint32_t(uint16_t(expected_x)) << 16) |
							uint16_t(expected_y);
					}
				}
			}
		}
	}

	INFO("cases=" << cases);
	INFO("failures=" << failures);

	if (first.valid)
	{
		INFO(
			"first start=(" << first.a << "," << first.b << ")" <<
			" actual_packed=" << first.actual_a <<
			" expected_packed=" << first.actual_b);
	}

	REQUIRE(cases == uint64_t(768) * 560 * 4);
	REQUIRE(failures == 0);
}
