// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "catch.hpp"

#include "cdidvc_fidelity.h"
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

TEST_CASE("CD-i DVC audio sample clock compares directly with SCR PTS and DCLK", "[emu][philips][dvc][audio][avsync][clock]")
{
	constexpr uint64_t anchor90 = 123'456;

	// Exact one-second boundaries must agree in all three timing domains.
	for (uint32_t rate : { 44'100U, 48'000U })
	{
		uint64_t const frames = rate;
		uint64_t const clock90 = cdi_dvc::audio_sample_clock90(anchor90, frames, rate);
		REQUIRE(clock90 == cdi_dvc::mpeg_timestamp_normalize(anchor90 + 90'000));
		auto const observed = cdi_dvc::observe_audio_clock(
			anchor90, frames, rate, clock90, clock90,
			uint32_t((clock90 >> 1) & 0xffffffffU));
		REQUIRE(observed.sample_clock90 == clock90);
		REQUIRE(observed.sample_minus_scr90 == 0);
		REQUIRE(observed.sample_minus_pts90 == 0);
		REQUIRE(observed.sample_minus_dclk45 == 0);
	}

	// Thirty minutes of exact-rate audio must not accumulate arithmetic drift.
	for (uint32_t rate : { 44'100U, 48'000U })
	{
		uint64_t const frames = uint64_t(rate) * 30U * 60U;
		uint64_t const expected90 = cdi_dvc::mpeg_timestamp_normalize(
			anchor90 + uint64_t(90'000) * 30U * 60U);
		uint64_t const observed90 = cdi_dvc::audio_sample_clock90(anchor90, frames, rate);
		REQUIRE(observed90 == expected90);
		auto const observed = cdi_dvc::observe_audio_clock(
			anchor90, frames, rate, expected90, expected90,
			uint32_t((expected90 >> 1) & 0xffffffffU));
		REQUIRE(observed.sample_minus_scr90 == 0);
		REQUIRE(observed.sample_minus_pts90 == 0);
		REQUIRE(observed.sample_minus_dclk45 == 0);
	}

	// The observation signs are explicit: positive means the emitted sample
	// clock is ahead of the timing reference.
	auto ahead = cdi_dvc::observe_audio_clock(
		anchor90, 48'000, 48'000,
		anchor90 + 89'990, anchor90 + 89'980,
		uint32_t(((anchor90 + 89'970) >> 1) & 0xffffffffU));
	REQUIRE(ahead.sample_minus_scr90 == 10);
	REQUIRE(ahead.sample_minus_pts90 == 20);
	REQUIRE(ahead.sample_minus_dclk45 == 15);

	REQUIRE(cdi_dvc::audio_sample_clock90(anchor90, 1234, 0) ==
		cdi_dvc::mpeg_timestamp_normalize(anchor90));
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
