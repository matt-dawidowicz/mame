// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#include "catch.hpp"

#include "cdidvc_utils.h"

#define PLM_NO_STDIO
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"

#include "cdidvc_audio_reference_data.h"

namespace {

template <std::size_t Size>
std::vector<int16_t> decode_repeated_frame(
		std::array<uint8_t, Size> const &frame)
{
	constexpr unsigned repeats = 3;
	std::vector<uint8_t> stream;
	stream.reserve(frame.size() * repeats);
	for (unsigned repeat = 0; repeat < repeats; ++repeat)
		stream.insert(stream.end(), frame.begin(), frame.end());

	plm_buffer_t *const buffer = plm_buffer_create_with_capacity(stream.size());
	REQUIRE(buffer != nullptr);
	REQUIRE(plm_buffer_write(buffer, stream.data(), stream.size()) == stream.size());
	plm_buffer_signal_end(buffer);

	plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
	REQUIRE(decoder != nullptr);
	REQUIRE(plm_audio_has_header(decoder));
	REQUIRE(plm_audio_get_samplerate(decoder) == 44'100);

	std::vector<int16_t> pcm;
	pcm.reserve(repeats * PLM_AUDIO_SAMPLES_PER_FRAME * 2);
	for (unsigned repeat = 0; repeat < repeats; ++repeat)
	{
		plm_samples_t *const samples = plm_audio_decode(decoder);
		REQUIRE(samples != nullptr);
		REQUIRE(samples->count == PLM_AUDIO_SAMPLES_PER_FRAME);
		for (float const sample : samples->interleaved)
			pcm.push_back(cdi_dvc::quantize_plm_audio_sample(sample));
	}

	REQUIRE(plm_audio_decode(decoder) == nullptr);
	REQUIRE(plm_audio_has_ended(decoder));
	plm_audio_destroy(decoder);
	return pcm;
}

cdi_dvc_test::reference_channel_metrics measure_channel(
		std::vector<int16_t> const &pcm,
		unsigned channel)
{
	cdi_dvc_test::reference_channel_metrics result { 0, 0, 0 };
	for (std::size_t index = channel; index < pcm.size(); index += 2)
	{
		int32_t const value = pcm[index];
		uint32_t const magnitude = uint32_t(value < 0 ? -value : value);
		result.absolute_sum += magnitude;
		result.square_sum += uint64_t(magnitude) * magnitude;
		if (magnitude > result.peak)
			result.peak = magnitude;
	}
	return result;
}

uint64_t absolute_difference(uint64_t first, uint64_t second)
{
	return first > second ? first - second : second - first;
}

void require_reference_match(
		std::vector<int16_t> const &pcm,
		std::array<cdi_dvc_test::reference_sample, 64> const &reference,
		std::array<cdi_dvc_test::reference_channel_metrics, 2> const &metrics,
		int32_t sample_tolerance)
{
	REQUIRE(pcm.size() == 3 * PLM_AUDIO_SAMPLES_PER_FRAME * 2);

	for (unsigned point = 0; point < reference.size(); ++point)
	{
		std::size_t const sample = 96 + point * 53;
		INFO("reference point=" << point << " sample=" << sample);
		REQUIRE(std::abs(int32_t(pcm[sample * 2]) - reference[point].left) <= sample_tolerance);
		REQUIRE(std::abs(int32_t(pcm[sample * 2 + 1]) - reference[point].right) <= sample_tolerance);
	}

	for (unsigned channel = 0; channel < 2; ++channel)
	{
		auto const actual = measure_channel(pcm, channel);
		auto const expected = metrics[channel];
		INFO("channel=" << channel
			<< " absolute=" << actual.absolute_sum
			<< " squares=" << actual.square_sum
			<< " peak=" << actual.peak);

		// PL_MPEG uses a floating-point synthesis filter while FFmpeg's `mp2`
		// reference decoder is fixed-point.  The bounds retain that expected
		// implementation difference while rejecting wrong scale, channel,
		// allocation-table, or filterbank output.
		REQUIRE(absolute_difference(actual.absolute_sum, expected.absolute_sum) * 16
			<= expected.absolute_sum);
		REQUIRE(absolute_difference(actual.square_sum, expected.square_sum) * 8
			<= expected.square_sum);
		REQUIRE(absolute_difference(actual.peak, expected.peak) <= 1'600);
	}
}

} // anonymous namespace

TEST_CASE("CD-i DVC PL_MPEG PCM quantization is bounded and symmetric", "[emu][philips][dvc][audio]")
{
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(-2.0F) == -32768);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(-1.0F) == -32767);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(-0.5F) == -16384);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(0.0F) == 0);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(0.5F) == 16384);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(1.0F) == 32767);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(2.0F) == 32767);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(
		-std::numeric_limits<float>::infinity()) == -32768);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(
		std::numeric_limits<float>::infinity()) == 32767);
	REQUIRE(cdi_dvc::quantize_plm_audio_sample(
		std::numeric_limits<float>::quiet_NaN()) == 0);
}

TEST_CASE("CD-i DVC PL_MPEG exposes emphasis from every Layer II frame", "[emu][philips][dvc][audio][deemphasis]")
{
	using cdi_dvc_test::STEREO_FRAME;
	std::vector<uint8_t> stream;
	stream.reserve(STEREO_FRAME.size() * 4);
	for (uint8_t emphasis = 0; emphasis < 4; ++emphasis)
	{
		std::size_t const offset = stream.size();
		stream.insert(stream.end(), STEREO_FRAME.begin(), STEREO_FRAME.end());
		stream[offset + 3] = uint8_t((stream[offset + 3] & 0xfc) | emphasis);
	}

	plm_buffer_t *const buffer = plm_buffer_create_with_capacity(stream.size());
	REQUIRE(buffer != nullptr);
	REQUIRE(plm_buffer_write(buffer, stream.data(), stream.size()) == stream.size());
	plm_buffer_signal_end(buffer);
	plm_audio_t *const decoder = plm_audio_create_with_buffer(buffer, 1);
	REQUIRE(decoder != nullptr);
	REQUIRE(plm_audio_has_header(decoder));
	REQUIRE(plm_audio_get_emphasis(decoder) == 0);

	for (uint8_t emphasis = 0; emphasis < 4; ++emphasis)
	{
		INFO("frame emphasis=" << unsigned(emphasis));
		REQUIRE(plm_audio_decode(decoder) != nullptr);
		REQUIRE(plm_audio_get_emphasis(decoder) == emphasis);
	}
	REQUIRE(plm_audio_decode(decoder) == nullptr);
	plm_audio_destroy(decoder);
}

TEST_CASE("CD-i DVC Layer II output tracks FFmpeg fixed-point reference PCM", "[emu][philips][dvc][audio][reference]")
{
	using namespace cdi_dvc_test;

	SECTION("stereo")
	{
		auto const pcm = decode_repeated_frame(STEREO_FRAME);
		require_reference_match(pcm, STEREO_REFERENCE, STEREO_METRICS, 1'200);
	}

	SECTION("joint stereo")
	{
		auto const pcm = decode_repeated_frame(JOINT_STEREO_FRAME);
		require_reference_match(pcm, JOINT_STEREO_REFERENCE, JOINT_STEREO_METRICS, 1'500);
	}

	SECTION("dual channel")
	{
		auto frame = STEREO_FRAME;
		frame[3] = 0x80;
		auto const pcm = decode_repeated_frame(frame);
		require_reference_match(pcm, STEREO_REFERENCE, STEREO_METRICS, 1'200);
	}

	SECTION("mono")
	{
		auto const pcm = decode_repeated_frame(MONO_FRAME);
		require_reference_match(pcm, MONO_REFERENCE, MONO_METRICS, 600);
	}
}
