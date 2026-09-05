// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdicdic_memory.h"

TEST_CASE("CDIC byte-owned RAM exposes host-independent 16-bit words", "[emu][philips][cdic][ram][portability]")
{
	std::array<uint8_t, 2> bytes{};

	for (uint32_t value = 0; value <= 0xffff; ++value)
	{
		cdic_hle::write_ram_word(bytes.data(), uint16_t(value));
		INFO("value=" << value);
		REQUIRE(bytes[0] == uint8_t(value));
		REQUIRE(bytes[1] == uint8_t(value >> 8));
		REQUIRE(cdic_hle::read_ram_word(bytes.data()) == value);
	}

	// Sector payload code constructs a logical big-byte-first source word.
	// Preserve the existing SCC-visible value while defining its byte storage.
	cdic_hle::write_ram_word(bytes.data(), uint16_t((0x12U << 8) | 0x34U));
	REQUIRE(bytes[0] == 0x34);
	REQUIRE(bytes[1] == 0x12);
	REQUIRE(cdic_hle::read_ram_word(bytes.data()) == 0x1234);

	// Synthesized Q bytes occupy complete CDIC RAM words in the current HLE.
	cdic_hle::write_ram_word(bytes.data(), 0x0041);
	REQUIRE(bytes[0] == 0x41);
	REQUIRE(bytes[1] == 0x00);
	REQUIRE(cdic_hle::read_ram_word(bytes.data()) == 0x0041);
}

TEST_CASE("CDIC masked RAM writes match COMBINE_DATA semantics", "[emu][philips][cdic][ram][portability]")
{
	constexpr std::array<uint16_t, 6> initial_values =
		{ 0x0000, 0xffff, 0x1234, 0xabcd, 0x00ff, 0xff00 };
	constexpr std::array<uint16_t, 6> masks =
		{ 0x0000, 0xffff, 0xff00, 0x00ff, 0x0f0f, 0xf0f0 };

	std::array<uint8_t, 2> bytes{};
	for (uint16_t initial : initial_values)
	{
		for (uint16_t mask : masks)
		{
			for (uint32_t data = 0; data <= 0xffff; ++data)
			{
				cdic_hle::write_ram_word(bytes.data(), initial);
				cdic_hle::combine_ram_word(bytes.data(), uint16_t(data), mask);
				uint16_t const expected = uint16_t((initial & ~mask) | (uint16_t(data) & mask));
				INFO("initial=" << initial << " mask=" << mask << " data=" << data);
				REQUIRE(cdic_hle::read_ram_word(bytes.data()) == expected);
			}
		}
	}
}
