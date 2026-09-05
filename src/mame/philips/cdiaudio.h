// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIAUDIO_H
#define MAME_PHILIPS_CDIAUDIO_H

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

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
	return attenuation_muted(value)
		? 0.0
		: std::pow(10.0, -double(attenuation_decibels(value)) / 20.0);
}

struct attenuation_gains
{
	double ll;
	double lr;
	double rr;
	double rl;
};

struct stereo_sample
{
	double left;
	double right;
};

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

// Output conversion for already-scaled PCM.  The rounding is an explicit host
// compatibility boundary, not a claim about CDIC/VMPEG silicon arithmetic.
inline int16_t quantize_deemphasized_pcm16(double sample)
{
	if (sample != sample)
		return 0;
	if (sample <= -32768.0)
		return -32768;
	if (sample >= 32767.0)
		return 32767;
	return int16_t(sample >= 0.0 ? sample + 0.5 : sample - 0.5);
}

inline attenuation_gains make_nominal_attenuation_gains(attenuation_matrix const &matrix)
{
	return {
		nominal_attenuation_gain(matrix[ATTEN_LL]),
		nominal_attenuation_gain(matrix[ATTEN_LR]),
		nominal_attenuation_gain(matrix[ATTEN_RR]),
		nominal_attenuation_gain(matrix[ATTEN_RL])
	};
}

constexpr stereo_sample mix_attenuated_stereo(
		attenuation_gains const &gain, double left, double right)
{
	return {
		left * gain.ll + right * gain.rl,
		left * gain.lr + right * gain.rr
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
