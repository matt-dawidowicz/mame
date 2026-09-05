// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstdint>

#include "catch.hpp"

#include "cdidvc_utils.h"

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding covers valid syntax fields", "[emu][philips][dvc][audio]")
{
	constexpr uint16_t bitrate_kbps[16] =
		{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 };
	constexpr uint16_t sample_rate_hz[4] = { 44'100, 48'000, 32'000, 0 };
	constexpr uint32_t fixed_header =
		(0x7ffU << 21) |
		(3U << 19) |
		(2U << 17);

	// Enumerate all 17 non-sync/version/layer bits.  Filtering only the two
	// unsupported bitrate indices and the reserved sample-rate index leaves
	// every indexed Layer II header combination, including CRC, padding,
	// private/copyright/original flags, mode extension and emphasis.
	for (uint32_t fields = 0; fields <= 0x1ffffU; ++fields)
	{
		unsigned const bitrate_index = (fields >> 12) & 0x0f;
		unsigned const sample_rate_index = (fields >> 10) & 0x03;
		if (bitrate_index == 0 || bitrate_index == 15 || sample_rate_index == 3)
			continue;

		bool const protection_absent = ((fields >> 16) & 1U) != 0;
		bool const padding = ((fields >> 9) & 1U) != 0;
		bool const private_bit = ((fields >> 8) & 1U) != 0;
		unsigned const channel_mode = (fields >> 6) & 0x03;
		unsigned const mode_extension = (fields >> 4) & 0x03;
		unsigned const emphasis = fields & 0x03;
		uint32_t const header = fixed_header | fields;
		auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
		uint16_t const expected_size = uint16_t(
			(144'000U * bitrate_kbps[bitrate_index]) /
				sample_rate_hz[sample_rate_index] + unsigned(padding));

		INFO("fields=" << fields
			<< " bitrate_index=" << bitrate_index
			<< " sample_rate_index=" << sample_rate_index
			<< " protection_absent=" << protection_absent
			<< " padding=" << padding
			<< " channel_mode=" << channel_mode
			<< " mode_extension=" << mode_extension
			<< " emphasis=" << emphasis);
		auto const expected_status = emphasis == 2
			? cdi_dvc::mpeg1_layer2_audio_header_status::accepted_reserved_emphasis
			: cdi_dvc::mpeg1_layer2_audio_header_status::supported;
		REQUIRE(decoded.status == expected_status);
		REQUIRE(decoded.valid);
		REQUIRE(decoded.bitrate_kbps == bitrate_kbps[bitrate_index]);
		REQUIRE(decoded.sample_rate_hz == sample_rate_hz[sample_rate_index]);
		REQUIRE(decoded.frame_size_bytes == expected_size);
		REQUIRE(decoded.bitrate_index == bitrate_index);
		REQUIRE(decoded.sample_rate_index == sample_rate_index);
		REQUIRE(decoded.channel_mode == channel_mode);
		REQUIRE(decoded.mode_extension == mode_extension);
		REQUIRE(decoded.emphasis == emphasis);
		REQUIRE(decoded.private_bit == private_bit);
		REQUIRE(decoded.has_crc == !protection_absent);
		REQUIRE(decoded.padding == padding);
	}
}

TEST_CASE("CD-i DVC MPEG-1 Layer II header decoding classifies every rejected field", "[emu][philips][dvc][audio]")
{
	using status = cdi_dvc::mpeg1_layer2_audio_header_status;

	uint32_t const valid_base =
		(0x7ffU << 21) |
		(3U << 19) |
		(2U << 17) |
		(1U << 16) |
		(8U << 12) |
		(1U << 10);

	REQUIRE(cdi_dvc::decode_mpeg1_layer2_audio_header(valid_base).valid);

	for (unsigned sync = 0; sync < 0x7ff; ++sync)
	{
		uint32_t const header = (valid_base & ~(0x7ffU << 21)) | (sync << 21);
		INFO("sync=" << sync);
		auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
		REQUIRE(decoded.status == status::invalid_sync);
		REQUIRE_FALSE(decoded.valid);
	}

	for (unsigned version = 0; version < 3; ++version)
	{
		uint32_t const header = (valid_base & ~(3U << 19)) | (version << 19);
		INFO("version=" << version);
		auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
		REQUIRE(decoded.status == status::invalid_version);
		REQUIRE_FALSE(decoded.valid);
	}

	for (unsigned layer = 0; layer < 4; ++layer)
	{
		uint32_t const header = (valid_base & ~(3U << 17)) | (layer << 17);
		INFO("layer=" << layer);
		auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
		REQUIRE(decoded.status == (layer == 2 ? status::supported : status::invalid_layer));
		REQUIRE(decoded.valid == (layer == 2));
	}

	uint32_t const free_format = valid_base & ~(0x0fU << 12);
	auto const free_decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(free_format);
	REQUIRE(free_decoded.status == status::unsupported_free_format);
	REQUIRE_FALSE(free_decoded.valid);
	REQUIRE(free_decoded.bitrate_index == 0);
	REQUIRE(free_decoded.sample_rate_hz == 48'000);

	uint32_t const bad_bitrate = (valid_base & ~(0x0fU << 12)) | (0x0fU << 12);
	auto const bitrate_decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(bad_bitrate);
	REQUIRE(bitrate_decoded.status == status::invalid_bitrate);
	REQUIRE_FALSE(bitrate_decoded.valid);

	uint32_t const bad_sample_rate = (valid_base & ~(0x03U << 10)) | (0x03U << 10);
	auto const rate_decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(bad_sample_rate);
	REQUIRE(rate_decoded.status == status::invalid_sample_rate);
	REQUIRE_FALSE(rate_decoded.valid);
}

TEST_CASE("CD-i DVC Layer II profile exhausts Green Book bitrate rate private and emphasis constraints", "[emu][philips][dvc][audio]")
{
	constexpr uint32_t fixed_header =
		(0x7ffU << 21) |
		(3U << 19) |
		(2U << 17) |
		(1U << 16);

	for (unsigned bitrate_index = 1; bitrate_index <= 14; ++bitrate_index)
	{
		for (unsigned sample_rate_index = 0; sample_rate_index < 3; ++sample_rate_index)
		{
			for (unsigned channel_mode = 0; channel_mode < 4; ++channel_mode)
			{
				for (unsigned private_bit = 0; private_bit < 2; ++private_bit)
				{
					for (unsigned emphasis = 0; emphasis < 4; ++emphasis)
					{
						uint32_t const raw = fixed_header |
							(bitrate_index << 12) |
							(sample_rate_index << 10) |
							(private_bit << 8) |
							(channel_mode << 6) |
							emphasis;
						auto const header = cdi_dvc::decode_mpeg1_layer2_audio_header(raw);
						bool const mono = channel_mode == 3;
						bool const bitrate_permitted = mono
							? bitrate_index <= 10
							: bitrate_index >= 4 && bitrate_index != 5;
						uint8_t expected = 0;
						if (!bitrate_permitted)
							expected |= cdi_dvc::CDI_LAYER2_PROFILE_BITRATE_CHANNEL;
						if (sample_rate_index != 0)
							expected |= cdi_dvc::CDI_LAYER2_PROFILE_SAMPLE_RATE;
						if (private_bit)
							expected |= cdi_dvc::CDI_LAYER2_PROFILE_PRIVATE_BIT;
						if (emphasis > 1)
							expected |= cdi_dvc::CDI_LAYER2_PROFILE_EMPHASIS;

						INFO("bitrate_index=" << bitrate_index
							<< " sample_rate_index=" << sample_rate_index
							<< " channel_mode=" << channel_mode
							<< " private_bit=" << private_bit
							<< " emphasis=" << emphasis);
						REQUIRE(header.valid);
						REQUIRE(cdi_dvc::cdi_full_motion_layer2_profile_violations(header) == expected);
					}
				}
			}
		}
	}

	auto const invalid = cdi_dvc::decode_mpeg1_layer2_audio_header(fixed_header);
	REQUIRE_FALSE(invalid.valid);
	REQUIRE(cdi_dvc::cdi_full_motion_layer2_profile_violations(invalid) ==
		cdi_dvc::CDI_LAYER2_PROFILE_INVALID_HEADER);
}
