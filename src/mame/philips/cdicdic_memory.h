// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDICDIC_MEMORY_H
#define MAME_PHILIPS_CDICDIC_MEMORY_H

#pragma once

#include <cstdint>

namespace cdic_hle
{

// CDIC RAM is owned as a byte array, while the SCC68070 observes 16-bit words.
// Keep that logical little-endian word view explicit rather than relying on the
// host's native uint16_t representation, alignment, or aliasing behavior.
constexpr uint16_t read_ram_word(const uint8_t *bytes)
{
	return uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8);
}

constexpr void write_ram_word(uint8_t *bytes, uint16_t value)
{
	bytes[0] = uint8_t(value);
	bytes[1] = uint8_t(value >> 8);
}

constexpr void combine_ram_word(
		uint8_t *bytes, uint16_t data, uint16_t mem_mask)
{
	uint16_t const current = read_ram_word(bytes);
	write_ram_word(bytes, uint16_t((current & ~mem_mask) | (data & mem_mask)));
}

} // namespace cdic_hle

#endif // MAME_PHILIPS_CDICDIC_MEMORY_H
