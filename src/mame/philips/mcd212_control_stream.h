// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_MCD212_CONTROL_STREAM_H
#define MAME_PHILIPS_MCD212_CONTROL_STREAM_H

#pragma once

#include <cstdint>

namespace mcd212_control
{

// Each MCD212 plane is 512 KiB.  VSR pixel fetches already expose that 19-bit
// byte-address wrap explicitly.  ICA/DCA control streams use the same plane RAM
// and therefore need the corresponding 18-bit word-address wrap before every
// host array access.  This is a memory-safety/address-bus invariant, not a claim
// about fetch-slot timing.
constexpr uint32_t PLANE_RAM_BYTES = 0x80000;
constexpr uint32_t PLANE_RAM_WORDS = PLANE_RAM_BYTES / 2;
constexpr uint32_t PLANE_WORD_MASK = PLANE_RAM_WORDS - 1;

constexpr uint32_t word_index(uint32_t word_address)
{
	return word_address & PLANE_WORD_MASK;
}

constexpr uint32_t word_address_from_byte(uint32_t byte_address)
{
	return word_index(byte_address >> 1);
}

constexpr uint32_t advance_word(uint32_t word_address, uint32_t words = 1)
{
	return word_index(word_address + words);
}

struct command_fetch
{
	uint32_t first_word;
	uint32_t second_word;
	uint32_t next_word;
};

constexpr command_fetch command_words(uint32_t word_address)
{
	uint32_t const first = word_index(word_address);
	uint32_t const second = advance_word(first);
	return { first, second, advance_word(second) };
}

} // namespace mcd212_control

#endif // MAME_PHILIPS_MCD212_CONTROL_STREAM_H
