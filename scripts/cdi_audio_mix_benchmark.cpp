// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "cdiaudio.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
constexpr std::size_t SAMPLE_COUNT = 4096;
constexpr std::size_t PASSES = 1024;
constexpr unsigned ROUNDS = 5;
constexpr double PCM16_NORMALIZATION = 1.0 / 32768.0;

struct input_frame
{
	int16_t left;
	int16_t right;
};

std::array<input_frame, SAMPLE_COUNT> make_input()
{
	std::array<input_frame, SAMPLE_COUNT> result{};
	uint32_t state = 0x13579bdfU;
	for (input_frame &frame : result)
	{
		state = state * 1664525U + 1013904223U;
		frame.left = int16_t(state >> 16);
		state = state * 1664525U + 1013904223U;
		frame.right = int16_t(state >> 16);
	}
	return result;
}

inline cdi_audio::stereo_sample legacy_mix(
		cdi_audio::attenuation_gains const &gain, int16_t left, int16_t right)
{
	return cdi_audio::mix_attenuated_stereo(
		gain,
		double(left) / 32768.0,
		double(right) / 32768.0);
}

inline cdi_audio::stereo_sample direct_mix(
		cdi_audio::attenuation_gains const &gain, int16_t left, int16_t right)
{
	cdi_audio::stereo_pcm16 const mixed =
		cdi_audio::mix_fma_attenuated_pcm16(gain, left, right);
	return {
		double(mixed.left) * PCM16_NORMALIZATION,
		double(mixed.right) * PCM16_NORMALIZATION
	};
}

template <typename Mixer>
double run_once(
		Mixer mixer,
		cdi_audio::attenuation_gains const &gain,
		std::array<input_frame, SAMPLE_COUNT> const &input,
		volatile double &sink)
{
	auto const start = std::chrono::steady_clock::now();
	for (std::size_t pass = 0; pass < PASSES; ++pass)
	{
		for (input_frame const &frame : input)
		{
			cdi_audio::stereo_sample const sample = mixer(gain, frame.left, frame.right);
			sink += sample.left * 0.5 + sample.right;
		}
	}
	auto const finish = std::chrono::steady_clock::now();
	return std::chrono::duration<double, std::nano>(finish - start).count()
		/ double(SAMPLE_COUNT * PASSES);
}

double median(std::vector<double> values)
{
	std::sort(values.begin(), values.end());
	return values[values.size() / 2];
}
}

int main()
{
	std::array<input_frame, SAMPLE_COUNT> const input = make_input();
	cdi_audio::attenuation_matrix const matrix = { 0x00, 0x06, 0x0c, 0x12 };
	cdi_audio::attenuation_gains const gain = cdi_audio::make_fma_attenuation_gains(matrix);

	// The optimized path is allowed only if it is bit-exact with the legacy
	// int16 -> normalized-double -> int16 path for the complete signed-16-bit
	// domain over a deterministic second channel.
	for (int32_t sample = -32768; sample <= 32767; ++sample)
	{
		int16_t const left = int16_t(sample);
		int16_t const right = int16_t(uint16_t(sample) ^ 0xa55aU);
		cdi_audio::stereo_sample const legacy = legacy_mix(gain, left, right);
		cdi_audio::stereo_sample const direct = direct_mix(gain, left, right);
		if (legacy.left != direct.left || legacy.right != direct.right)
		{
			std::cerr << "equivalence failure at sample " << sample << '\n';
			return EXIT_FAILURE;
		}
	}

	volatile double sink = 0.0;
	(void)run_once(legacy_mix, gain, input, sink);
	(void)run_once(direct_mix, gain, input, sink);

	std::vector<double> legacy;
	std::vector<double> direct;
	legacy.reserve(ROUNDS);
	direct.reserve(ROUNDS);
	for (unsigned round = 0; round < ROUNDS; ++round)
	{
		// Alternate order to reduce systematic thermal/frequency bias.
		if (round & 1U)
		{
			direct.push_back(run_once(direct_mix, gain, input, sink));
			legacy.push_back(run_once(legacy_mix, gain, input, sink));
		}
		else
		{
			legacy.push_back(run_once(legacy_mix, gain, input, sink));
			direct.push_back(run_once(direct_mix, gain, input, sink));
		}
	}

	double const legacy_ns = median(legacy);
	double const direct_ns = median(direct);
	std::cout << "CD-i FMA audio-mix benchmark\n"
		<< "frames_per_round=" << (SAMPLE_COUNT * PASSES) << '\n'
		<< "rounds=" << ROUNDS << '\n'
		<< "legacy_ns_per_frame=" << legacy_ns << '\n'
		<< "direct_ns_per_frame=" << direct_ns << '\n'
		<< "speedup=" << (legacy_ns / direct_ns) << "x\n"
		<< "sink=" << sink << '\n';
	return EXIT_SUCCESS;
}
