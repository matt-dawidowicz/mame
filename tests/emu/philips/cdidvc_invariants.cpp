// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "cdi_video_timing.h"
#include "cdidvc_utils.h"

TEST_CASE("CD-i DVC timestamp normalization is stable across 33-bit aliases", "[emu][philips][dvc][invariant]")
{
	constexpr uint64_t wrap = cdi_dvc::MPEG_TIMESTAMP_MODULUS;
	constexpr uint64_t mask = cdi_dvc::MPEG_TIMESTAMP_MASK;

	for (uint64_t value : { uint64_t(0), uint64_t(1), uint64_t(90'000), mask - 1, mask })
	{
		INFO("value=" << value);
		REQUIRE(cdi_dvc::mpeg_timestamp_normalize(value) == value);
		REQUIRE(cdi_dvc::mpeg_timestamp_normalize(value + wrap) == value);
		REQUIRE(cdi_dvc::mpeg_timestamp_normalize(value + 2 * wrap) == value);
	}
}

TEST_CASE("CD-i DVC modular timestamp delta reconstructs endpoints", "[emu][philips][dvc][invariant]")
{
	constexpr int64_t half_wrap = int64_t(uint64_t(1) << 32);
	uint64_t state = 0x6a09e667f3bcc909ULL;

	for (unsigned iteration = 0; iteration < 4'096; ++iteration)
	{
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		uint64_t const lhs = cdi_dvc::mpeg_timestamp_normalize(state);
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		uint64_t const rhs = cdi_dvc::mpeg_timestamp_normalize(state);
		int64_t const delta = cdi_dvc::mpeg_timestamp_delta(lhs, rhs);
		int64_t const reverse = cdi_dvc::mpeg_timestamp_delta(rhs, lhs);
		int64_t const tolerance = int64_t(iteration % 257) - 128;

		INFO("iteration=" << iteration << " lhs=" << lhs << " rhs=" << rhs << " delta=" << delta);
		REQUIRE(delta >= -half_wrap);
		REQUIRE(delta < half_wrap);
		REQUIRE(cdi_dvc::mpeg_timestamp_normalize(rhs + uint64_t(delta)) == lhs);
		if (delta == -half_wrap)
			REQUIRE(reverse == -half_wrap);
		else
			REQUIRE(reverse == -delta);
		REQUIRE(cdi_dvc::mpeg_presentation_due(lhs, rhs, tolerance) == (delta <= tolerance));
	}
}

TEST_CASE("CD-i DVC anchored clock preserves one complete DCLK wrap", "[emu][philips][dvc][invariant]")
{
	// A complete 32-bit 45 kHz DCLK wrap corresponds to exactly 2^33 90 kHz
	// ticks, which is one complete MPEG system-clock wrap. Therefore identical
	// 32-bit DCLK samples before/after a full cycle must map to the same 33-bit
	// MPEG clock value.
	for (uint64_t anchor90 : { uint64_t(0), uint64_t(1), uint64_t(123'456'789), cdi_dvc::MPEG_TIMESTAMP_MASK })
	{
		INFO("anchor90=" << anchor90);
		REQUIRE(cdi_dvc::mpeg_clock_from_dclk(anchor90, 0x12345678U, 0x12345678U)
				== cdi_dvc::mpeg_timestamp_normalize(anchor90));
	}
}

TEST_CASE("CD-i DVC presentation selection consumes exactly the due prefix", "[emu][philips][dvc][invariant]")
{
	constexpr std::array<uint64_t, 5> timestamps { 100, 110, 120, 130, 140 };

	for (uint64_t clock = 90; clock <= 150; ++clock)
	{
		cdi_dvc::presentation_selection const selected =
			cdi_dvc::select_latest_due_presentation(timestamps.data(), timestamps.size(), clock);

		std::size_t expected_count = 0;
		for (uint64_t timestamp : timestamps)
		{
			if (!cdi_dvc::mpeg_presentation_due(timestamp, clock))
				break;
			++expected_count;
		}

		INFO("clock=" << clock << " expected_count=" << expected_count);
		REQUIRE(selected.valid == bool(expected_count));
		REQUIRE(selected.consume_count == expected_count);
		if (expected_count)
			REQUIRE(selected.selected_index == expected_count - 1);
	}

	cdi_dvc::presentation_selection const empty =
		cdi_dvc::select_latest_due_presentation(timestamps.data(), 0, 150);
	REQUIRE_FALSE(empty.valid);
	REQUIRE(empty.consume_count == 0);
}

TEST_CASE("CD-i DVC presentation selection remains ordered across timestamp wrap", "[emu][philips][dvc][invariant]")
{
	constexpr uint64_t mask = cdi_dvc::MPEG_TIMESTAMP_MASK;
	constexpr std::array<uint64_t, 5> timestamps { mask - 3, mask - 1, 1, 3, 5 };

	struct expected_case
	{
		uint64_t clock;
		std::size_t count;
	};

	constexpr expected_case cases[] = {
		{ mask - 4, 0 },
		{ mask - 3, 1 },
		{ mask - 1, 2 },
		{ 1, 3 },
		{ 3, 4 },
		{ 5, 5 },
	};

	for (expected_case const &item : cases)
	{
		auto const selected = cdi_dvc::select_latest_due_presentation(
			timestamps.data(), timestamps.size(), item.clock);
		INFO("clock=" << item.clock);
		REQUIRE(selected.valid == bool(item.count));
		REQUIRE(selected.consume_count == item.count);
		if (item.count)
			REQUIRE(selected.selected_index == item.count - 1);
	}
}

TEST_CASE("CD-i DVC DCLK-to-sample conversion is the minimal non-early integer delay", "[emu][philips][dvc][invariant]")
{
	constexpr uint32_t rates[] = { 32'000, 44'100, 48'000 };

	for (uint32_t rate : rates)
	{
		for (int32_t ticks45 = 1; ticks45 <= 2'000; ++ticks45)
		{
			uint64_t const samples = cdi_dvc::dclk_delay_to_samples(ticks45, rate);
			uint64_t const target = uint64_t(uint32_t(ticks45)) * rate;

			INFO("rate=" << rate << " ticks45=" << ticks45 << " samples=" << samples);
			REQUIRE(samples > 0);
			REQUIRE(samples * cdi_dvc::DCLK_HZ >= target);
			REQUIRE((samples - 1) * cdi_dvc::DCLK_HZ < target);
		}
	}
}

TEST_CASE("CD-i DVC Full Motion picture-rate decoder rejects every unsupported code", "[emu][philips][dvc][invariant]")
{
	for (unsigned value = 0; value <= 0xff; ++value)
	{
		uint8_t const code = uint8_t(value);
		auto const rate = cdi_dvc::full_motion_picture_rate_from_code(code);
		bool const supported = code >= 1 && code <= 5;

		INFO("code=" << value);
		REQUIRE((rate.numerator != 0) == supported);
		REQUIRE(rate.denominator != 0);
		if (!supported)
		{
			REQUIRE(rate.numerator == 0);
			REQUIRE(rate.denominator == 1);
		}
	}
}

TEST_CASE("CD-i MCD212 controlled timing is structurally consistent for every control-bit combination", "[emu][philips][cdi][video][invariant]")
{
	for (unsigned cf_value = 0; cf_value <= 1; ++cf_value)
	{
		for (unsigned fd_value = 0; fd_value <= 1; ++fd_value)
		{
			for (unsigned st_value = 0; st_value <= 1; ++st_value)
			{
				bool const cf = bool(cf_value);
				bool const fd = bool(fd_value);
				bool const st = bool(st_value);
				uint32_t const clock = cf ? (fd ? 30'209'800 : 30'000'000) : 28'000'000;
				auto const timing = cdi_video::controlled_halfline_screen_profile(cf, fd, st, clock);

				uint16_t const active_half_lines = uint16_t(cdi_video::noninterlace_active_lines(fd, st) * 2U);
				uint16_t const expected_hactive = uint16_t(cdi_video::horizontal_active_cycles(cf, st) * 8U);
				uint16_t const expected_htotal = uint16_t(cdi_video::horizontal_total_cycles(cf) * 8U);

				INFO("cf=" << cf << " fd=" << fd << " st=" << st);
				REQUIRE(timing.pixel_clock_hz == clock);
				REQUIRE(timing.htotal == expected_htotal);
				REQUIRE(timing.hvis_start == 0);
				REQUIRE(timing.hvis_end == expected_hactive);
				REQUIRE(timing.hvis_end <= timing.htotal);
				REQUIRE(timing.vtotal == cdi_video::interlace_total_half_lines(fd));
				REQUIRE(timing.vvis_end > timing.vvis_start);
				REQUIRE(timing.vvis_end <= timing.vtotal);
				REQUIRE(uint16_t(timing.vvis_end - timing.vvis_start) == active_half_lines);
			}
		}
	}
}
