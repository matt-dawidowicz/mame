// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "cdidvc_utils.h"

TEST_CASE("CD-i DVC anchored MPEG clock advances from 45 kHz DCLK", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(0, 0, 0) == 0);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(1'000, 100, 100) == 1'000);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(1'000, 100, 101) == 1'002);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(1'000, 100, 145) == 1'090);

	constexpr uint64_t wrap = cdi_dvc::MPEG_TIMESTAMP_MODULUS;
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(wrap - 2, 0, 1) == 0);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(wrap - 1, 0, 1) == 1);

	// DCLK is a 32-bit clock.  Unsigned subtraction must preserve elapsed
	// ticks when the anchor is immediately before a wrap.
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(10, 0xffffffffU, 0) == 12);
	REQUIRE(cdi_dvc::mpeg_clock_from_dclk(10, 0xfffffffeU, 1) == 16);
}

TEST_CASE("CD-i DVC presentation due comparison follows 33-bit timestamp ordering", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::mpeg_presentation_due(100, 100));
	REQUIRE(cdi_dvc::mpeg_presentation_due(99, 100));
	REQUIRE_FALSE(cdi_dvc::mpeg_presentation_due(101, 100));
	REQUIRE(cdi_dvc::mpeg_presentation_due(101, 100, 1));
	REQUIRE_FALSE(cdi_dvc::mpeg_presentation_due(102, 100, 1));

	constexpr uint64_t mask = cdi_dvc::MPEG_TIMESTAMP_MASK;
	REQUIRE_FALSE(cdi_dvc::mpeg_presentation_due(1, mask));
	REQUIRE(cdi_dvc::mpeg_presentation_due(1, mask, 2));
	REQUIRE(cdi_dvc::mpeg_presentation_due(mask, 1));
}

TEST_CASE("CD-i DVC presentation selection chooses the newest timestamp already due", "[emu][philips][dvc]")
{
	uint64_t const ordered[] = { 90, 100, 110 };

	auto selected = cdi_dvc::select_latest_due_presentation(ordered, 3, 100);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 1);
	REQUIRE(selected.consume_count == 2);

	selected = cdi_dvc::select_latest_due_presentation(ordered, 3, 89);
	REQUIRE_FALSE(selected.valid);
	REQUIRE(selected.consume_count == 0);

	selected = cdi_dvc::select_latest_due_presentation(ordered, 3, 200);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 2);
	REQUIRE(selected.consume_count == 3);

	selected = cdi_dvc::select_latest_due_presentation(nullptr, 3, 100);
	REQUIRE_FALSE(selected.valid);

	uint64_t const early[] = { 100, 102, 104 };
	selected = cdi_dvc::select_latest_due_presentation(early, 3, 100, 2);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 1);
	REQUIRE(selected.consume_count == 2);

	constexpr uint64_t mask = cdi_dvc::MPEG_TIMESTAMP_MASK;
	uint64_t const wrapped[] = { mask - 1, 1, 3 };
	selected = cdi_dvc::select_latest_due_presentation(wrapped, 3, 1);
	REQUIRE(selected.valid);
	REQUIRE(selected.selected_index == 1);
	REQUIRE(selected.consume_count == 2);
}

TEST_CASE("CD-i DVC DCLK delays convert to output samples with ceiling semantics", "[emu][philips][dvc]")
{
	REQUIRE(cdi_dvc::dclk_delay_to_samples(-1, 48'000) == 0);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(0, 48'000) == 0);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(1, 0) == 0);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(45'000, 48'000) == 48'000);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(45'000, 44'100) == 44'100);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(1, 48'000) == 2);
	REQUIRE(cdi_dvc::dclk_delay_to_samples(15, 48'000) == 16);
}

TEST_CASE("CD-i DVC Full Motion picture-rate codes expose exact rational rates", "[emu][philips][dvc]")
{
	auto rate = cdi_dvc::full_motion_picture_rate_from_code(1);
	REQUIRE(rate.numerator == 24'000);
	REQUIRE(rate.denominator == 1'001);

	rate = cdi_dvc::full_motion_picture_rate_from_code(2);
	REQUIRE(rate.numerator == 24);
	REQUIRE(rate.denominator == 1);

	rate = cdi_dvc::full_motion_picture_rate_from_code(3);
	REQUIRE(rate.numerator == 25);
	REQUIRE(rate.denominator == 1);

	rate = cdi_dvc::full_motion_picture_rate_from_code(4);
	REQUIRE(rate.numerator == 30'000);
	REQUIRE(rate.denominator == 1'001);

	rate = cdi_dvc::full_motion_picture_rate_from_code(5);
	REQUIRE(rate.numerator == 30);
	REQUIRE(rate.denominator == 1);

	for (uint8_t code : { uint8_t(0), uint8_t(6), uint8_t(7), uint8_t(8), uint8_t(15) })
	{
		rate = cdi_dvc::full_motion_picture_rate_from_code(code);
		REQUIRE(rate.numerator == 0);
		REQUIRE(rate.denominator == 1);
	}
}
