// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstdint>

#include "catch.hpp"

#include "cdidvc_utils.h"

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding covers valid profile fields", "[emu][philips][dvc][audio]")
{
	constexpr uint16_t bitrate_kbps[16] =
		{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 };
	constexpr uint16_t sample_rate_hz[4] = { 44'100, 48'000, 32'000, 0 };

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
				bool const expected_valid = bitrate_kbps[bitrate_index] != 0
					&& sample_rate_hz[sample_rate_index] != 0;

				INFO("bitrate_index=" << bitrate_index
					<< " sample_rate_index=" << sample_rate_index
					<< " channel_mode=" << channel_mode);
				REQUIRE(decoded.valid == expected_valid);
				REQUIRE(decoded.bitrate_kbps == (expected_valid ? bitrate_kbps[bitrate_index] : 0));
				REQUIRE(decoded.sample_rate_hz == (expected_valid ? sample_rate_hz[sample_rate_index] : 0));
				REQUIRE(decoded.channel_mode == channel_mode);
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
