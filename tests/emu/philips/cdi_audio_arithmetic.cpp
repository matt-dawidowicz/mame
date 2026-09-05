// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

#include "catch.hpp"

#include "cdiaudio.h"
#include "cdicdic_state.h"

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

	// These are candidate-grid facts, not hardware attribution.  A Q15
	// coefficient becomes zero at 97 dB while Q23 remains non-zero through the
	// entire 0-127 dB public register range.  High-range captures can therefore
	// distinguish at least these two model families; the existing 0-29 dB trace
	// cannot.
	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(96, 15).coefficient != 0);
	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(97, 15).coefficient == 0);
	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(127, 23).coefficient != 0);
	REQUIRE(cdi_audio::quantize_nominal_attenuation_gain(127, 31).coefficient != 0);

	// The already measured 0-29 dB interval is compatible with several widths.
	// Quantify that ambiguity so a future capture is judged against an explicit
	// resolution requirement rather than by eye.
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

TEST_CASE("CDIC XA predictor rounding ties toward positive infinity before saturation", "[emu][philips][audio][xa][rounding][saturation]")
{
	// Filter 1 is 240/256.  A history value of +/-8 produces an exact half
	// integer predictor (+/-7.5).  The retained +128 then arithmetic-shift
	// model therefore resolves both ties upward: +8 and -7.  This pins the
	// current FFmpeg-correlated compatibility rule without calling it silicon.
	auto positive_tie = cdic_hle::decode_xa_sample(0x10, 0, 8, 0);
	auto negative_tie = cdic_hle::decode_xa_sample(0x10, 0, -8, 0);
	REQUIRE(positive_tie.output == 8);
	REQUIRE(negative_tie.output == -7);

	// Ranges are arithmetic shifts of the sign-extended code.  Pin negative
	// odd values around the floor/truncation distinction as well.
	auto negative_code = cdic_hle::decode_xa_sample(0x01, -3, 0, 0);
	REQUIRE(negative_code.output == -2);

	// Saturation is the final 16-bit boundary and predictor history receives
	// that clipped result, not the over-range intermediate.
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
