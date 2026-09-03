// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDICDIC_AUDIO_H
#define MAME_PHILIPS_CDICDIC_AUDIO_H

#pragma once

#include <cstdint>

namespace cdic_audio
{

struct coding_info
{
	bool valid;
	bool emphasis;
	bool stereo;
	uint8_t bits_per_sample;
	uint16_t sample_rate_divisor;
	uint8_t maximum_range;
	uint8_t sector_count;
};

constexpr coding_info decode_coding(uint8_t coding)
{
	const uint8_t channel_mode = coding & 0x03;
	const uint8_t sample_rate = coding & 0x0c;
	const uint8_t bits_per_sample = coding & 0x30;
	const bool valid =
		!(coding & 0x80) &&
		channel_mode <= 0x01 &&
		(sample_rate == 0x00 || sample_rate == 0x04) &&
		(bits_per_sample == 0x00 || bits_per_sample == 0x10);

	if (!valid)
		return { false, bool(coding & 0x40), false, 0, 0, 0, 0 };

	const bool eight_bit = bits_per_sample == 0x10;
	const bool stereo = channel_mode == 0x01;
	const bool half_rate = sample_rate == 0x04;

	uint8_t sectors = 2;
	if (!eight_bit)
		sectors *= 2;
	if (half_rate)
		sectors *= 2;
	if (!stereo)
		sectors *= 2;

	return
	{
		true,
		bool(coding & 0x40),
		stereo,
		uint8_t(eight_bit ? 8 : 4),
		uint16_t(half_rate ? 1024 : 512),
		uint8_t(eight_bit ? 8 : 12),
		sectors
	};
}

constexpr uint8_t normalize_sound_parameter(uint8_t parameter, uint8_t maximum_range)
{
	const uint8_t range = parameter & 0x0f;
	return uint8_t((parameter & 0xf0) | (range > maximum_range ? maximum_range : range));
}

constexpr int16_t clip_sample(int64_t sample)
{
	return int16_t(sample < -32768 ? -32768 : sample > 32767 ? 32767 : sample);
}

constexpr uint32_t attenuation_gain_q30(uint8_t control)
{
	if (control & 0x80)
		return 0;

	// 10^(-1/20) in Q30.  Repeated multiplication gives the Green Book's
	// one-decibel attenuation steps without per-sample floating-point math.
	constexpr uint64_t ONE = uint64_t(1) << 30;
	constexpr uint64_t ONE_DB = 956'973'408;
	uint64_t gain = ONE;
	for (unsigned db = control & 0x7f; db; --db)
		gain = (gain * ONE_DB + (ONE >> 1)) >> 30;
	return uint32_t(gain);
}

constexpr int16_t mix_one_channel(
		int16_t first,
		uint32_t first_gain,
		int16_t second,
		uint32_t second_gain)
{
	const int64_t mixed = int64_t(first) * first_gain + int64_t(second) * second_gain;
	const int64_t rounded = mixed >= 0
		? (mixed + (int64_t(1) << 29)) >> 30
		: -(((-mixed) + (int64_t(1) << 29)) >> 30);
	return clip_sample(rounded);
}

struct stereo_sample
{
	int16_t left;
	int16_t right;
};

constexpr stereo_sample mix_sample(
		int16_t left,
		int16_t right,
		uint8_t left_to_left,
		uint8_t left_to_right,
		uint8_t right_to_right,
		uint8_t right_to_left)
{
	return
	{
		mix_one_channel(
			left, attenuation_gain_q30(left_to_left),
			right, attenuation_gain_q30(right_to_left)),
		mix_one_channel(
			left, attenuation_gain_q30(left_to_right),
			right, attenuation_gain_q30(right_to_right))
	};
}

struct deemphasis_coefficients
{
	double b0;
	double b1;
	double a1;
};

constexpr deemphasis_coefficients deemphasis_for_rate(uint32_t sample_rate)
{
	// CD/CD-i 50/15 microsecond de-emphasis.  Bilinear-transforming
	// H(s)=(1+s*15us)/(1+s*50us) preserves unity DC gain and 0.3 gain at
	// Nyquist while providing stable coefficients for all CD-i sample rates.
	const double k = 2.0 * double(sample_rate);
	const double numerator_tc = k * 15.0e-6;
	const double denominator_tc = k * 50.0e-6;
	const double denominator = 1.0 + denominator_tc;
	return
	{
		(1.0 + numerator_tc) / denominator,
		(1.0 - numerator_tc) / denominator,
		(1.0 - denominator_tc) / denominator
	};
}

inline int16_t deemphasis_sample(
		int16_t input,
		double &previous_input,
		double &previous_output,
		uint32_t sample_rate)
{
	if (!sample_rate)
		return input;

	const deemphasis_coefficients coefficient = deemphasis_for_rate(sample_rate);
	const double output =
		coefficient.b0 * double(input) +
		coefficient.b1 * previous_input -
		coefficient.a1 * previous_output;

	previous_input = double(input);
	previous_output = output;
	const int64_t rounded = output >= 0.0 ? int64_t(output + 0.5) : int64_t(output - 0.5);
	return clip_sample(rounded);
}

} // namespace cdic_audio

#endif // MAME_PHILIPS_CDICDIC_AUDIO_H
