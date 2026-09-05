// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "mcd212_control_stream.h"

TEST_CASE("MCD212 control-stream word addressing wraps within one plane", "[emu][philips][mcd212][control][bounds]")
{
	REQUIRE(mcd212_control::PLANE_RAM_BYTES == 0x80000);
	REQUIRE(mcd212_control::PLANE_RAM_WORDS == 0x40000);

	REQUIRE(mcd212_control::word_index(0) == 0);
	REQUIRE(mcd212_control::word_index(0x3ffff) == 0x3ffff);
	REQUIRE(mcd212_control::word_index(0x40000) == 0);
	REQUIRE(mcd212_control::word_index(0x40001) == 1);

	REQUIRE(mcd212_control::word_address_from_byte(0x000000) == 0);
	REQUIRE(mcd212_control::word_address_from_byte(0x07fffc) == 0x3fffe);
	REQUIRE(mcd212_control::word_address_from_byte(0x07fffe) == 0x3ffff);
	REQUIRE(mcd212_control::word_address_from_byte(0x07ffff) == 0x3ffff);
	REQUIRE(mcd212_control::word_address_from_byte(0x080000) == 0);
}

TEST_CASE("MCD212 ICA and DCA command pairs cannot index beyond plane RAM", "[emu][philips][mcd212][control][bounds]")
{
	for (uint32_t word = 0; word < mcd212_control::PLANE_RAM_WORDS; ++word)
	{
		auto const fetch = mcd212_control::command_words(word);
		INFO("word=" << word);
		REQUIRE(fetch.first_word < mcd212_control::PLANE_RAM_WORDS);
		REQUIRE(fetch.second_word < mcd212_control::PLANE_RAM_WORDS);
		REQUIRE(fetch.next_word < mcd212_control::PLANE_RAM_WORDS);
	}

	auto const final_aligned = mcd212_control::command_words(0x3fffe);
	REQUIRE(final_aligned.first_word == 0x3fffe);
	REQUIRE(final_aligned.second_word == 0x3ffff);
	REQUIRE(final_aligned.next_word == 0);

	auto const final_word = mcd212_control::command_words(0x3ffff);
	REQUIRE(final_word.first_word == 0x3ffff);
	REQUIRE(final_word.second_word == 0);
	REQUIRE(final_word.next_word == 1);

	REQUIRE(mcd212_control::advance_word(0x3ffff) == 0);
	REQUIRE(mcd212_control::advance_word(0x3fffe, 2) == 0);
	REQUIRE(mcd212_control::advance_word(0x3fffd, 4) == 1);
}
