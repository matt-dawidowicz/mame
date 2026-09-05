// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "catch.hpp"

#include "cdiaudio.h"
#include "cdicdic_state.h"
#include "cdidvc_utils.h"

TEST_CASE("CD-i attenuation candidate coefficient grids expose high-range discrimination", "[emu][philips][audio][attenuation][quantization]")
{
	std::array<uint8_t, 3> const fractional_widths = { 15, 23, 31 };

	for (uint8_t const fractional_bits : fractional_widths)
	{
		uint64_t previous = std::numeric_limits<uint64_t>::max();
		for (unsigned db = 0; db < 128; ++db)
		{
			auto const quantized = cdi_audio::quantize_nominal_attenuation_gain(
				uint8_t(db), fractional_bits);
			INFO("Q" << unsigned(fractional_bits) << " attenuation=" << db);
			REQUIRE(quantized.valid);
			REQUIRE(quantized.coefficient <= previous);
			REQUIRE(quantized.as_double() >= 0.0);
			REQUIRE(quantized.as_double() <= 1.0);
			previous = quantized.coefficient;
		}

		auto const unity = cdi_audio::quantize_nominal_attenuation_gain(0, fractional_bits);
		REQUIRE(unity.coefficient == (uint64_t(1) << fractional_bits));
		REQUIRE(unity.as_double() == 1.0);

		for (unsigned muted = 0x80; muted <= 0xff; ++muted)
		{
			auto const quantized = cdi_audio::quantize_nominal_attenuation_gain(
				uint8_t(muted), fractional_bits);
			INFO("Q" << unsigned(fractional_bits) << " muted raw=" << muted);
			REQUIRE(quantized.valid);
			REQUIRE(quantized.coefficient == 0);
			REQUIRE(quantized.as_double() == 0.0);
		}
	}

	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(96, 15).coefficient != 0);
	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(97, 15).coefficient == 0);
	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(127, 23).coefficient != 0);
	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(127, 31).coefficient != 0);

	double maximum_q15_error_db = 0.0;
	double maximum_q23_error_db = 0.0;
	for (unsigned db = 0; db <= 29; ++db)
	{
		double const ideal = cdi_audio::nominal_attenuation_gain(uint8_t(db));
		for (auto const candidate : { std::pair<uint8_t, double *>{ 15, &maximum_q15_error_db },
				std::pair<uint8_t, double *>{ 23, &maximum_q23_error_db } })
		{
			double const reconstructed = cdi_audio::quantize_nominal_attenuation_gain(
				uint8_t(db), candidate.first).as_double();
			double const error_db = std::abs(20.0 * std::log10(reconstructed / ideal));
			*candidate.second = std::max(*candidate.second, error_db);
		}
	}
	REQUIRE(maximum_q15_error_db < 0.0033);
	REQUIRE(maximum_q23_error_db < 0.000014);

	REQUIRE_FALSE(cdi_audio::quantize_nominal_attenuation_gain(0, 53).valid);
}

TEST_CASE("CD-i host PCM saturation and half-way rounding are explicit", "[emu][philips][audio][rounding][saturation]")
{
	REQUIRE(cdi_audio::saturate_pcm16(std::numeric_limits<int64_t>::min()) == -32768);
	REQUIRE(cdi_audio::saturate_pcm16(-32769) == -32768);
	REQUIRE(cdi_audio::saturate_pcm16(-32768) == -32768);
	REQUIRE(cdi_audio::saturate_pcm16(32767) == 32767);
	REQUIRE(cdi_audio::saturate_pcm16(32768) == 32767);
	REQUIRE(cdi_audio::saturate_pcm16(std::numeric_limits<int64_t>::max()) == 32767);

	for (int32_t sample = -32768; sample <= 32767; ++sample)
	{
		INFO("PCM integer=" << sample);
		REQUIRE(cdi_audio::quantize_pcm16_nearest_away(double(sample)) == sample);
	}

	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(-32768.5) == -32768);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(-1.5) == -2);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(-0.5) == -1);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(0.5) == 1);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(1.5) == 2);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(32767.5) == 32767);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(
		std::numeric_limits<double>::quiet_NaN()) == 0);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(
		std::numeric_limits<double>::infinity()) == 32767);
	REQUIRE(cdi_audio::quantize_pcm16_nearest_away(
		-std::numeric_limits<double>::infinity()) == -32768);
}

TEST_CASE("CDIC XA arithmetic floor shifts are explicit over their signed domain", "[emu][philips][audio][xa][rounding][portability]")
{
	auto const reference_floor_shift = [](int32_t value, uint8_t shift)
	{
		if (!shift)
			return value;
		if (shift >= 31)
			return value < 0 ? int32_t(-1) : int32_t(0);

		int64_t const divisor = int64_t(1) << shift;
		int64_t quotient = int64_t(value) / divisor;
		int64_t const remainder = int64_t(value) % divisor;
		if (remainder < 0)
			--quotient;
		return int32_t(quotient);
	};

	for (unsigned shift = 0; shift <= 15; ++shift)
	{
		for (int32_t value = -32768; value <= 32767; ++value)
		{
			INFO("value=" << value << " shift=" << shift);
			REQUIRE(cdic_hle::floor_shift_right(value, uint8_t(shift)) ==
				reference_floor_shift(value, uint8_t(shift)));
		}
	}

	constexpr std::array<int32_t, 11> boundaries =
	{
		std::numeric_limits<int32_t>::min(),
		-1073741825,
		-1073741824,
		-65537,
		-1,
		0,
		1,
		65537,
		1073741823,
		1073741824,
		std::numeric_limits<int32_t>::max()
	};
	for (unsigned shift = 0; shift <= 31; ++shift)
	{
		for (int32_t value : boundaries)
		{
			INFO("boundary value=" << value << " shift=" << shift);
			REQUIRE(cdic_hle::floor_shift_right(value, uint8_t(shift)) ==
				reference_floor_shift(value, uint8_t(shift)));
		}
	}

	REQUIRE(cdic_hle::floor_shift_right(-3, 1) == -2);
	REQUIRE(cdic_hle::floor_shift_right(-1, 31) == -1);
	REQUIRE(cdic_hle::floor_shift_right(std::numeric_limits<int32_t>::min(), 31) == -1);
	REQUIRE(cdic_hle::floor_shift_right(std::numeric_limits<int32_t>::max(), 31) == 0);
}

TEST_CASE("CDIC XA predictor rounding ties toward positive infinity before saturation", "[emu][philips][audio][xa][rounding][saturation]")
{
	auto positive_tie = cdic_hle::decode_xa_sample(0x10, 0, 8, 0);
	auto negative_tie = cdic_hle::decode_xa_sample(0x10, 0, -8, 0);
	REQUIRE(positive_tie.output == 8);
	REQUIRE(negative_tie.output == -7);

	auto negative_code = cdic_hle::decode_xa_sample(0x01, -3, 0, 0);
	REQUIRE(negative_code.output == -2);

	auto clipped_high = cdic_hle::decode_xa_sample(0x20, 32767, 32767, -32768);
	REQUIRE(clipped_high.output == 32767);
	REQUIRE(clipped_high.recent == 32767);
	REQUIRE(clipped_high.older == 32767);

	auto clipped_low = cdic_hle::decode_xa_sample(0x20, -32768, -32768, 32767);
	REQUIRE(clipped_low.output == -32768);
	REQUIRE(clipped_low.recent == -32768);
	REQUIRE(clipped_low.older == -32768);

	REQUIRE(cdic_hle::clip_sample(std::numeric_limits<int32_t>::min()) == -32768);
	REQUIRE(cdic_hle::clip_sample(std::numeric_limits<int32_t>::max()) == 32767);
}

TEST_CASE("CDIC XA compatibility arithmetic has an explicit software intermediate envelope", "[emu][philips][audio][xa][rounding][saturation][bounds]")
{
	constexpr int16_t filter[4][2] =
	{
		{ 0, 0 },
		{ 240, 0 },
		{ 460, -208 },
		{ 392, -220 }
	};
	constexpr std::array<int16_t, 2> history_extremes = { -32768, 32767 };

	int64_t minimum_numerator = std::numeric_limits<int64_t>::max();
	int64_t maximum_numerator = std::numeric_limits<int64_t>::min();
	for (unsigned predictor = 0; predictor < 4; ++predictor)
	{
		for (int16_t recent : history_extremes)
		{
			for (int16_t older : history_extremes)
			{
				int64_t const numerator =
					int64_t(filter[predictor][0]) * recent +
					int64_t(filter[predictor][1]) * older + 128;
				minimum_numerator = std::min(minimum_numerator, numerator);
				maximum_numerator = std::max(maximum_numerator, numerator);
			}
		}
	}

	REQUIRE(minimum_numerator == -21888688);
	REQUIRE(maximum_numerator == 21888692);
	REQUIRE(minimum_numerator < -(int64_t(1) << 24));
	REQUIRE(maximum_numerator > (int64_t(1) << 24) - 1);
	REQUIRE(minimum_numerator >= -(int64_t(1) << 25));
	REQUIRE(maximum_numerator <= (int64_t(1) << 25) - 1);

	constexpr int64_t minimum_decoded = -32768 - 85503;
	constexpr int64_t maximum_decoded = 32767 + 85502;
	REQUIRE(minimum_decoded == -118271);
	REQUIRE(maximum_decoded == 118269);
	REQUIRE(minimum_decoded >= -(int64_t(1) << 17));
	REQUIRE(maximum_decoded <= (int64_t(1) << 17) - 1);
}

TEST_CASE("CD-i current DAC-boundary models make underrun and stop retention deterministic", "[emu][philips][audio][dac][transition][underrun]")
{
	// DVC timestamp silence has priority over queued PCM and does not consume it.
	std::vector<int16_t> dvc_queue = { 100, -100, 200, -200 };
	std::size_t read = 0;
	uint64_t wait = 2;
	auto silence0 = cdi_dvc::take_audio_output_frame(dvc_queue, read, wait);
	auto silence1 = cdi_dvc::take_audio_output_frame(dvc_queue, read, wait);
	REQUIRE(silence0.kind == cdi_dvc::audio_output_kind::scheduled_silence);
	REQUIRE(silence1.kind == cdi_dvc::audio_output_kind::scheduled_silence);
	REQUIRE(silence0.left == 0);
	REQUIRE(silence0.right == 0);
	REQUIRE(read == 0);
	REQUIRE(wait == 0);

	auto pcm0 = cdi_dvc::take_audio_output_frame(dvc_queue, read, wait);
	auto pcm1 = cdi_dvc::take_audio_output_frame(dvc_queue, read, wait);
	REQUIRE(pcm0.kind == cdi_dvc::audio_output_kind::pcm);
	REQUIRE(pcm0.left == 100);
	REQUIRE(pcm0.right == -100);
	REQUIRE(pcm1.kind == cdi_dvc::audio_output_kind::pcm);
	REQUIRE(pcm1.left == 200);
	REQUIRE(pcm1.right == -200);
	REQUIRE(pcm1.drained);

	// Empty queue underrun deterministically emits a zero frame in the current
	// MAME model.  This pins emulator policy, not the physical VMPEG DAC edge.
	auto starved = cdi_dvc::take_audio_output_frame(dvc_queue, read, wait);
	REQUIRE(starved.kind == cdi_dvc::audio_output_kind::starvation);
	REQUIRE(starved.left == 0);
	REQUIRE(starved.right == 0);

	// An incomplete stereo pair is retained rather than consuming half a frame.
	dvc_queue.push_back(321);
	auto incomplete = cdi_dvc::take_audio_output_frame(dvc_queue, read, wait);
	REQUIRE(incomplete.kind == cdi_dvc::audio_output_kind::starvation);
	REQUIRE(dvc_queue.size() == 1);
	REQUIRE(read == 0);
	dvc_queue.push_back(-321);
	auto refilled = cdi_dvc::take_audio_output_frame(dvc_queue, read, wait);
	REQUIRE(refilled.kind == cdi_dvc::audio_output_kind::pcm);
	REQUIRE(refilled.left == 321);
	REQUIRE(refilled.right == -321);
	REQUIRE(refilled.drained);

	// CDIC realtime stop disables consumption and clears the in-flight period,
	// but intentionally does not erase ready halves.  Restart therefore resumes
	// from the retained next half.  Again, this is current HLE queue policy and
	// not a claim that a physical DAC holds or flushes its analogue sample.
	cdic_hle::realtime_audio_state cdic;
	cdic.enabled = true;
	cdic.ready = { true, true };
	cdic.next_play = 0;
	cdic.periods_remaining = 3;
	cdic_hle::stop_realtime_audio(cdic);
	REQUIRE_FALSE(cdic.enabled);
	REQUIRE(cdic.periods_remaining == 0);
	REQUIRE(cdic.ready[0]);
	REQUIRE(cdic.ready[1]);
	REQUIRE(cdic_hle::take_realtime_audio_buffer(cdic) == cdic_hle::NO_AUDIO_BUFFER);

	cdic_hle::start_realtime_audio(cdic);
	REQUIRE(cdic_hle::take_realtime_audio_buffer(cdic) == 0);
	REQUIRE_FALSE(cdic.ready[0]);
	REQUIRE(cdic.ready[1]);
	REQUIRE(cdic.next_play == 1);
}
