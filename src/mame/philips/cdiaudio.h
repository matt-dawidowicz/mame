// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIAUDIO_H
#define MAME_PHILIPS_CDIAUDIO_H

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "cdiaudio_dsp56001.h"

namespace cdi_audio
{

// Green Book IV.6.3 defines four independent one-decibel attenuators.  The
// public SC_Atten/MA_Cntrl byte order is LL, LR, RR, RL; bit 7 mutes a path and
// bits 6:0 contain its nominal attenuation in dB.
enum attenuation_path : unsigned
{
	ATTEN_LL = 0,
	ATTEN_LR = 1,
	ATTEN_RR = 2,
	ATTEN_RL = 3
};

using attenuation_matrix = std::array<uint8_t, 4>;

constexpr attenuation_matrix RESET_ATTENUATION = { 0x80, 0xff, 0x80, 0xff };
constexpr attenuation_matrix STRAIGHT_ATTENUATION = { 0x00, 0x80, 0x00, 0x80 };

constexpr bool attenuation_muted(uint8_t value)
{
	return bool(value & 0x80);
}

constexpr uint8_t attenuation_decibels(uint8_t value)
{
	return value & 0x7f;
}

// This is the Green Book nominal transfer function.  It deliberately does not
// claim the quantization, tolerance, or analogue floor of any particular CD-i
// player revision.
inline double nominal_attenuation_gain(uint8_t value)
{
	if (attenuation_muted(value))
		return 0.0;

	// The register exposes only 128 nominal dB values.  Build the exact same
	// Green Book curve once, then use indexed lookups in steady-state audio.
	static const std::array<double, 128> gains = []
	{
		std::array<double, 128> result{};
		for (unsigned db = 0; db < result.size(); ++db)
			result[db] = std::pow(10.0, -double(db) / 20.0);
		return result;
	}();
	return gains[attenuation_decibels(value)];
}

// Candidate fixed-point representation used to compare possible attenuator
// coefficient widths against captures.  This remains parameterized for CDIC
// and other paths whose coefficient storage is not established.
struct quantized_attenuation_gain
{
	uint64_t coefficient;
	uint8_t fractional_bits;
	bool valid;

	constexpr double as_double() const
	{
		return valid
			? double(coefficient) / double(uint64_t(1) << fractional_bits)
			: 0.0;
	}
};

inline quantized_attenuation_gain quantize_nominal_attenuation_gain(
		uint8_t value, uint8_t fractional_bits)
{
	// Keep the scale exactly representable in both uint64_t and double so the
	// candidate quantizer itself does not inject an unrelated host precision
	// limit.  The useful campaign candidates are well inside it.
	if (fractional_bits > 52)
		return { 0, fractional_bits, false };
	if (attenuation_muted(value))
		return { 0, fractional_bits, true };

	uint64_t const scale = uint64_t(1) << fractional_bits;
	double const scaled = nominal_attenuation_gain(value) * double(scale);
	return { uint64_t(scaled + 0.5), fractional_bits, true };
}

// The retained Philips VMPEG FMA DSP data image contains the complete 0..127 dB
// coefficient curve as round(2^22 * 10^(-dB/20)).  Keeping the values literal
// makes the emulated FMA transfer independent of host libm rounding and avoids
// recomputing a firmware table in the audio callback.  This evidence is scoped
// to the VMPEG FMA path; it is not attributed to Mono-I CDIC attenuation.
constexpr uint8_t FMA_ATTENUATION_FRACTIONAL_BITS = 22;
constexpr uint32_t FMA_ATTENUATION_SCALE = uint32_t(1) << FMA_ATTENUATION_FRACTIONAL_BITS;
constexpr std::array<uint32_t, 128> FMA_ATTENUATION_Q22 =
{
	0x400000U, 0x390a41U, 0x32d646U, 0x2d4efcU, 0x28619bU, 0x23fd66U, 0x201374U, 0x1c9677U,
	0x197a96U, 0x16b543U, 0x143d13U, 0x1209a3U, 0x10137aU, 0x0e53ecU, 0x0cc50aU, 0x0b6188U,
	0x0a24b0U, 0x090a4dU, 0x080ea0U, 0x072e51U, 0x066666U, 0x05b43aU, 0x05156dU, 0x0487e6U,
	0x0409c3U, 0x039957U, 0x033525U, 0x02dbd9U, 0x028c42U, 0x024554U, 0x02061cU, 0x01cdc4U,
	0x019b8cU, 0x016ecbU, 0x0146e7U, 0x01235aU, 0x0103abU, 0x00e76eU, 0x00ce43U, 0x00b7d5U,
	0x00a3d7U, 0x009206U, 0x008225U, 0x0073fdU, 0x006760U, 0x005c22U, 0x00521dU, 0x00492fU,
	0x00413aU, 0x003a22U, 0x0033d0U, 0x002e2dU, 0x002928U, 0x0024aeU, 0x0020b1U, 0x001d23U,
	0x0019f8U, 0x001725U, 0x0014a0U, 0x001262U, 0x001062U, 0x000e9aU, 0x000d04U, 0x000b99U,
	0x000a56U, 0x000937U, 0x000836U, 0x000752U, 0x000686U, 0x0005d0U, 0x00052eU, 0x00049eU,
	0x00041eU, 0x0003abU, 0x000345U, 0x0002eaU, 0x000299U, 0x000250U, 0x000210U, 0x0001d7U,
	0x0001a3U, 0x000176U, 0x00014dU, 0x000129U, 0x000109U, 0x0000ecU, 0x0000d2U, 0x0000bbU,
	0x0000a7U, 0x000095U, 0x000085U, 0x000076U, 0x000069U, 0x00005eU, 0x000054U, 0x00004bU,
	0x000042U, 0x00003bU, 0x000035U, 0x00002fU, 0x00002aU, 0x000025U, 0x000021U, 0x00001eU,
	0x00001aU, 0x000018U, 0x000015U, 0x000013U, 0x000011U, 0x00000fU, 0x00000dU, 0x00000cU,
	0x00000bU, 0x000009U, 0x000008U, 0x000007U, 0x000007U, 0x000006U, 0x000005U, 0x000005U,
	0x000004U, 0x000004U, 0x000003U, 0x000003U, 0x000003U, 0x000002U, 0x000002U, 0x000002U
};

constexpr uint32_t fma_attenuation_coefficient(uint8_t value)
{
	return attenuation_muted(value) ? 0U : FMA_ATTENUATION_Q22[attenuation_decibels(value)];
}

struct attenuation_gains
{
	uint32_t ll;
	uint32_t lr;
	uint32_t rr;
	uint32_t rl;
};

struct stereo_sample
{
	double left;
	double right;
};

struct stereo_pcm16
{
	int16_t left;
	int16_t right;
};

// Host PCM boundary shared by filtered paths.  Saturation and nearest rounding
// are explicit software policy; they are not evidence for CDIC/VMPEG internal
// accumulator width or the physical DAC's rounding circuit.
constexpr int16_t saturate_pcm16(int64_t sample)
{
	return int16_t(sample < -32768 ? -32768 : sample > 32767 ? 32767 : sample);
}

inline int16_t quantize_pcm16_nearest_away(double sample)
{
	if (sample != sample)
		return 0;
	if (sample <= -32768.0)
		return -32768;
	if (sample >= 32767.0)
		return 32767;
	return saturate_pcm16(int64_t(sample >= 0.0 ? sample + 0.5 : sample - 0.5));
}

// A two-input FMA matrix cannot overflow the documented 56-bit DSP56001
// accumulator when the decoded source samples are signed 16-bit and each Q22
// coefficient is <= unity.  The worst magnitude is exactly 2^38 before the
// Q22 reduction, so accumulator saturation is unreachable in this HLE path.
constexpr int64_t FMA_Q22_MAX_ABS_ACCUMULATOR = int64_t(1) << 38;

constexpr int64_t fma_mix_q22_accumulator(
		int16_t first, uint32_t first_gain,
		int16_t second, uint32_t second_gain)
{
	return int64_t(first) * first_gain + int64_t(second) * second_gain;
}

// DSP56001 RND/MPYR/MACR use convergent (nearest-even) rounding.  The retained
// FMA coefficient image establishes the Q22 scale, while complete instruction-
// path recovery is still tracked separately.  Centralizing the reduction here
// makes that last instruction-level choice explicit and independently testable.
constexpr int32_t fma_reduce_q22_nearest_even(int64_t accumulator)
{
	return int32_t(round_shift_nearest_even(accumulator, FMA_ATTENUATION_FRACTIONAL_BITS));
}

constexpr stereo_pcm16 mix_fma_attenuated_pcm16(
		attenuation_gains const &gain, int16_t left, int16_t right)
{
	int64_t const left_accumulator = fma_mix_q22_accumulator(left, gain.ll, right, gain.rl);
	int64_t const right_accumulator = fma_mix_q22_accumulator(left, gain.lr, right, gain.rr);
	return {
		saturate_pcm16(fma_reduce_q22_nearest_even(left_accumulator)),
		saturate_pcm16(fma_reduce_q22_nearest_even(right_accumulator))
	};
}

// Standards-derived 50/15 microsecond de-emphasis compatibility model.
//
// IEC 60908 defines the continuous-time response.  The 44.1 kHz coefficients
// are the independently published SoX inverse-CD-emphasis fit (within about
// 0.06 dB through 20 kHz).  The two CD-ROM XA rates use the same RBJ high-shelf
// form fitted to that IEC response through 0.475 Nyquist.  These coefficients
// describe the requested frequency response; they do not claim the analogue
// topology or arithmetic of a particular CD-i player revision.
struct deemphasis_coefficients
{
	double b0;
	double b1;
	double b2;
	double a1;
	double a2;
	bool valid;
};

constexpr deemphasis_coefficients deemphasis_coefficients_for_rate(uint32_t sample_rate)
{
	switch (sample_rate)
	{
	case 44'100:
		return { 0.4603507788631884, -0.2844082119124985,
				0.03388877229118692, -1.0542914627856914,
				0.2641228020275685, true };

	case 37'800:
		return { 0.4851941456939215, -0.23841803301734341,
				0.013042671527302522, -0.93671169830289069,
				0.19653048250677144, true };

	case 18'900:
		return { 0.63583440943260772, -0.0060814215137256983,
				-0.031122431232763273, -0.39542123188111106,
				-0.0059482114327702919, true };

	default:
		return { 0.0, 0.0, 0.0, 0.0, 0.0, false };
	}
}

struct deemphasis_filter_state
{
	double input1 = 0.0;
	double input2 = 0.0;
	double output1 = 0.0;
	double output2 = 0.0;
	uint32_t sample_rate = 0;
};

constexpr void reset_deemphasis(deemphasis_filter_state &state)
{
	state = {};
}

// Keep the filter primed while emphasis is disabled.  This makes a coding-bit
// change deterministic and avoids manufacturing a transient from empty state.
// Whether any real CD-i board switches or resets its analogue network at that
// edge remains a measurement question.
inline double apply_50_15_deemphasis(
		deemphasis_filter_state &state, double input,
		uint32_t sample_rate, bool enabled)
{
	deemphasis_coefficients const coefficients =
		deemphasis_coefficients_for_rate(sample_rate);
	if (!coefficients.valid)
		return input;

	if (state.sample_rate != sample_rate)
	{
		reset_deemphasis(state);
		state.sample_rate = sample_rate;
	}

	double const output =
		coefficients.b0 * input
		+ coefficients.b1 * state.input1
		+ coefficients.b2 * state.input2
		- coefficients.a1 * state.output1
		- coefficients.a2 * state.output2;
	state.input2 = state.input1;
	state.input1 = input;
	state.output2 = state.output1;
	state.output1 = output;
	return enabled ? output : input;
}

// CD-i Full Motion permits 50/15 microsecond emphasis only at its mandatory
// 44.1 kHz rate.  Reserved/J.17 values and profile-invalid sample rates remain
// visible to diagnostics but must not silently acquire a made-up response.
constexpr bool cdi_mpeg_deemphasis_enabled(uint8_t emphasis, uint32_t sample_rate)
{
	return emphasis == 1 && sample_rate == 44'100;
}

// Output conversion for already-scaled PCM.  Keep this name at the filter call
// sites while routing through the shared, explicitly documented host boundary.
inline int16_t quantize_deemphasized_pcm16(double sample)
{
	return quantize_pcm16_nearest_away(sample);
}

inline attenuation_gains make_fma_attenuation_gains(attenuation_matrix const &matrix)
{
	return {
		fma_attenuation_coefficient(matrix[ATTEN_LL]),
		fma_attenuation_coefficient(matrix[ATTEN_LR]),
		fma_attenuation_coefficient(matrix[ATTEN_RR]),
		fma_attenuation_coefficient(matrix[ATTEN_RL])
	};
}

// Compatibility name retained for the existing DVC call site.  CDIC does not
// use this helper and therefore does not inherit the FMA DSP coefficient model.
inline attenuation_gains make_nominal_attenuation_gains(attenuation_matrix const &matrix)
{
	return make_fma_attenuation_gains(matrix);
}

inline stereo_sample mix_attenuated_stereo(
		attenuation_gains const &gain, double left, double right)
{
	// DVC supplies exact int16/32768 normalized samples here.  Recover that PCM
	// domain, execute the firmware-derived Q22 matrix with one convergent
	// reduction, then enforce the final signed-16-bit output boundary.
	int16_t const pcm_left = quantize_pcm16_nearest_away(left * 32768.0);
	int16_t const pcm_right = quantize_pcm16_nearest_away(right * 32768.0);
	stereo_pcm16 const mixed = mix_fma_attenuated_pcm16(gain, pcm_left, pcm_right);
	return {
		double(mixed.left) / 32768.0,
		double(mixed.right) / 32768.0
	};
}

// The VMPEG FMA driver writes MA_Cntrl through the DSP56001 indirect port at
// E03022/E03024.  A retained driver trace establishes the wire order as
// RR, LR, RL, LL even though the API and stored matrix use LL, LR, RR, RL.
constexpr std::array<uint8_t, 4> FMA_DSP_ATTENUATION_ORDER =
	{ ATTEN_RR, ATTEN_LR, ATTEN_RL, ATTEN_LL };

struct fma_dsp_audio_control
{
	uint8_t address = 0;
	uint8_t mode = 0;
	uint8_t target = 0;
	uint8_t attenuation_write_index = 0;
	attenuation_matrix attenuation = RESET_ATTENUATION;
};

constexpr void fma_dsp_address_write(fma_dsp_audio_control &state, uint8_t address)
{
	state.address = address;
}

constexpr bool fma_dsp_attenuation_selected(fma_dsp_audio_control const &state)
{
	return state.address == 7 && state.mode == 0x80 && state.target == 0x93;
}

// Returns true only when an active attenuation path was written.  The DSP
// protocol remains selected until the driver changes mode/target, and its
// two-bit path index wraps after a complete four-byte transfer.
constexpr bool fma_dsp_data_write(fma_dsp_audio_control &state, uint8_t data)
{
	switch (state.address)
	{
	case 0:
		state.mode = data;
		return false;

	case 1:
		state.target = data;
		if (state.mode == 0x80 && data == 0x93)
			state.attenuation_write_index = 0;
		return false;

	case 7:
		if (state.mode == 0x80 && state.target == 0x93)
		{
			unsigned const transfer_index = state.attenuation_write_index & 3;
			state.attenuation[FMA_DSP_ATTENUATION_ORDER[transfer_index]] = data;
			state.attenuation_write_index = (transfer_index + 1) & 3;
			return true;
		}
		return false;

	default:
		return false;
	}
}

} // namespace cdi_audio

#endif // MAME_PHILIPS_CDIAUDIO_H
