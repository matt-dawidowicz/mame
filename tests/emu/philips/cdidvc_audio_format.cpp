// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstdint>

#include "catch.hpp"

#include "cdidvc_utils.h"

namespace {

constexpr uint16_t BITRATE_KBPS[16] =
	{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 };
constexpr uint16_t SAMPLE_RATE_HZ[4] = { 44'100, 48'000, 32'000, 0 };

} // anonymous namespace

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding reports validity across the profile matrix", "[emu][philips][dvc][audio]")
{
	for (unsigned bitrate_index = 0; bitrate_index < 16; ++bitrate_index)
	{
		for (unsigned sample_rate_index = 0; sample_rate_index < 4; ++sample_rate_index)
		{
			for (unsigned channel_mode = 0; channel_mode < 4; ++channel_mode)
			{
				uint32_t const header =
					(0x7ffU << 21) |
					(3U << 19) |
					(2U << 17) |
					(bitrate_index << 12) |
					(sample_rate_index << 10) |
					(channel_mode << 6);
				auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
				bool const expected_valid = BITRATE_KBPS[bitrate_index] != 0
					&& SAMPLE_RATE_HZ[sample_rate_index] != 0;

				INFO("bitrate_index=" << bitrate_index
					<< " sample_rate_index=" << sample_rate_index
					<< " channel_mode=" << channel_mode);
				REQUIRE(decoded.valid == expected_valid);
			}
		}
	}
}

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding reports bitrate across the profile matrix", "[emu][philips][dvc][audio]")
{
	for (unsigned bitrate_index = 0; bitrate_index < 16; ++bitrate_index)
	{
		for (unsigned sample_rate_index = 0; sample_rate_index < 4; ++sample_rate_index)
		{
			for (unsigned channel_mode = 0; channel_mode < 4; ++channel_mode)
			{
				uint32_t const header =
					(0x7ffU << 21) |
					(3U << 19) |
					(2U << 17) |
					(bitrate_index << 12) |
					(sample_rate_index << 10) |
					(channel_mode << 6);
				auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
				bool const expected_valid = BITRATE_KBPS[bitrate_index] != 0
					&& SAMPLE_RATE_HZ[sample_rate_index] != 0;
				unsigned const actual_bitrate = decoded.bitrate_kbps;
				unsigned const expected_bitrate = expected_valid ? BITRATE_KBPS[bitrate_index] : 0U;

				INFO("bitrate_index=" << bitrate_index
					<< " sample_rate_index=" << sample_rate_index
					<< " channel_mode=" << channel_mode);
				REQUIRE(actual_bitrate == expected_bitrate);
			}
		}
	}
}

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding reports sample rate across the profile matrix", "[emu][philips][dvc][audio]")
{
	for (unsigned bitrate_index = 0; bitrate_index < 16; ++bitrate_index)
	{
		for (unsigned sample_rate_index = 0; sample_rate_index < 4; ++sample_rate_index)
		{
			for (unsigned channel_mode = 0; channel_mode < 4; ++channel_mode)
			{
				uint32_t const header =
					(0x7ffU << 21) |
					(3U << 19) |
					(2U << 17) |
					(bitrate_index << 12) |
					(sample_rate_index << 10) |
					(channel_mode << 6);
				auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
				bool const expected_valid = BITRATE_KBPS[bitrate_index] != 0
					&& SAMPLE_RATE_HZ[sample_rate_index] != 0;
				unsigned const actual_sample_rate = decoded.sample_rate_hz;
				unsigned const expected_sample_rate = expected_valid ? SAMPLE_RATE_HZ[sample_rate_index] : 0U;

				INFO("bitrate_index=" << bitrate_index
					<< " sample_rate_index=" << sample_rate_index
					<< " channel_mode=" << channel_mode);
				REQUIRE(actual_sample_rate == expected_sample_rate);
			}
		}
	}
}

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding reports channel mode across the profile matrix", "[emu][philips][dvc][audio]")
{
	for (unsigned bitrate_index = 0; bitrate_index < 16; ++bitrate_index)
	{
		for (unsigned sample_rate_index = 0; sample_rate_index < 4; ++sample_rate_index)
		{
			for (unsigned channel_mode = 0; channel_mode < 4; ++channel_mode)
			{
				uint32_t const header =
					(0x7ffU << 21) |
					(3U << 19) |
					(2U << 17) |
					(bitrate_index << 12) |
					(sample_rate_index << 10) |
					(channel_mode << 6);
				auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
				unsigned const actual_channel_mode = decoded.channel_mode;

				INFO("bitrate_index=" << bitrate_index
					<< " sample_rate_index=" << sample_rate_index
					<< " channel_mode=" << channel_mode);
				REQUIRE(actual_channel_mode == channel_mode);
			}
		}
	}
}

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding rejects wrong sync version and layer", "[emu][philips][dvc][audio]")
{
	uint32_t const valid_base =
		(0x7ffU << 21) |
		(3U << 19) |
		(2U << 17) |
		(8U << 12) |
		(1U << 10);

	REQUIRE(cdi_dvc::decode_mpeg1_layer2_audio_header(valid_base).valid);

	for (unsigned sync = 0; sync < 0x7ff; sync += 73)
	{
		uint32_t const header = (valid_base & ~(0x7ffU << 21)) | (sync << 21);
		INFO("sync=" << sync);
		REQUIRE_FALSE(cdi_dvc::decode_mpeg1_layer2_audio_header(header).valid);
	}

	for (unsigned version = 0; version < 3; ++version)
	{
		uint32_t const header = (valid_base & ~(3U << 19)) | (version << 19);
		INFO("version=" << version);
		REQUIRE_FALSE(cdi_dvc::decode_mpeg1_layer2_audio_header(header).valid);
	}

	for (unsigned layer = 0; layer < 4; ++layer)
	{
		uint32_t const header = (valid_base & ~(3U << 17)) | (layer << 17);
		INFO("layer=" << layer);
		REQUIRE(cdi_dvc::decode_mpeg1_layer2_audio_header(header).valid == (layer == 2));
	}
}
