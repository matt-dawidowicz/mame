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
