// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstdint>

#include "catch.hpp"

#include "cdiaudio.h"
#include "cdidvc_utils.h"

TEST_CASE("CD-i attenuation bytes exhaust the Green Book nominal curve", "[emu][philips][audio][attenuation][exhaustive]")
{
	// Independent fixed reference for 10^(-1/20); repeated multiplication
	// prevents this oracle from merely repeating the production pow expression.
	double constexpr one_db_ratio = 0.8912509381337456;
	double previous = 0.0;
	double expected = 1.0;

	for (unsigned raw = 0; raw <= 0xff; ++raw)
	{
		uint8_t const value = uint8_t(raw);
		double const gain = cdi_audio::nominal_attenuation_gain(value);
		INFO("raw=" << raw);

		REQUIRE(cdi_audio::attenuation_muted(value) == bool(raw & 0x80));
		REQUIRE(cdi_audio::attenuation_decibels(value) == (raw & 0x7f));
		if (raw & 0x80)
		{
			REQUIRE(gain == 0.0);
		}
		else
		{
			REQUIRE(gain == Approx(expected).epsilon(1e-14));
			if (raw)
			{
				REQUIRE(gain < previous);
				REQUIRE(gain / previous == Approx(one_db_ratio).epsilon(1e-14));
			}
			previous = gain;
			expected *= one_db_ratio;
		}
	}

	REQUIRE(cdi_audio::nominal_attenuation_gain(0) == 1.0);
	REQUIRE(cdi_audio::nominal_attenuation_gain(20) == Approx(0.1).epsilon(1e-14));
	REQUIRE(cdi_audio::nominal_attenuation_gain(40) == Approx(0.01).epsilon(1e-14));
	REQUIRE(cdi_audio::nominal_attenuation_gain(60) == Approx(0.001).epsilon(1e-14));
	REQUIRE(cdi_audio::nominal_attenuation_gain(0x80) == 0.0);
	REQUIRE(cdi_audio::nominal_attenuation_gain(0xff) == 0.0);
}

TEST_CASE("CD-i four-path attenuation routes channels independently", "[emu][philips][audio][attenuation][exhaustive]")
{
	double constexpr left = 0.25;
	double constexpr right = -0.5;

	for (unsigned path = 0; path < 4; ++path)
	{
		double expected_gain = 1.0;
		for (unsigned db = 0; db < 128; ++db)
		{
			cdi_audio::attenuation_matrix matrix = { 0x80, 0x80, 0x80, 0x80 };
			matrix[path] = uint8_t(db);
			auto const gains = cdi_audio::make_nominal_attenuation_gains(matrix);
			auto const output = cdi_audio::mix_attenuated_stereo(gains, left, right);
			double const expected_left = path == cdi_audio::ATTEN_LL
				? left * expected_gain
				: path == cdi_audio::ATTEN_RL ? right * expected_gain : 0.0;
			double const expected_right = path == cdi_audio::ATTEN_LR
				? left * expected_gain
				: path == cdi_audio::ATTEN_RR ? right * expected_gain : 0.0;

			INFO("path=" << path << " db=" << db);
			REQUIRE(std::abs(output.left - expected_left) <= 1e-15);
			REQUIRE(std::abs(output.right - expected_right) <= 1e-15);
			expected_gain *= 0.8912509381337456;
		}
	}

	auto const straight = cdi_audio::make_nominal_attenuation_gains(
		cdi_audio::STRAIGHT_ATTENUATION);
	auto const straight_output = cdi_audio::mix_attenuated_stereo(straight, left, right);
	REQUIRE(straight_output.left == left);
	REQUIRE(straight_output.right == right);
}

TEST_CASE("CD-i FMA DSP attenuation follows the captured MA_Cntrl wire order", "[emu][philips][dvc][audio][attenuation][hardware]")
{
	cdi_audio::fma_dsp_audio_control dsp;
	REQUIRE(dsp.attenuation == cdi_audio::RESET_ATTENUATION);

	// Data at the attenuation address is inert until mode 80/target 93 has
	// selected the control block.
	cdi_audio::fma_dsp_address_write(dsp, 7);
	REQUIRE_FALSE(cdi_audio::fma_dsp_data_write(dsp, 0x11));
	REQUIRE(dsp.attenuation == cdi_audio::RESET_ATTENUATION);

	cdi_audio::fma_dsp_address_write(dsp, 0);
	REQUIRE_FALSE(cdi_audio::fma_dsp_data_write(dsp, 0x80));
	cdi_audio::fma_dsp_address_write(dsp, 1);
	REQUIRE_FALSE(cdi_audio::fma_dsp_data_write(dsp, 0x93));
	REQUIRE(dsp.attenuation_write_index == 0);

	// Retained madriv trace for MA_Cntrl(..., 42434445, ...) writes
	// 44,43,45,42, producing the API-order matrix 42,43,44,45.
	cdi_audio::fma_dsp_address_write(dsp, 7);
	REQUIRE(cdi_audio::fma_dsp_data_write(dsp, 0x44));
	REQUIRE(dsp.attenuation[cdi_audio::ATTEN_RR] == 0x44);
	REQUIRE(cdi_audio::fma_dsp_data_write(dsp, 0x43));
	REQUIRE(dsp.attenuation[cdi_audio::ATTEN_LR] == 0x43);
	REQUIRE(cdi_audio::fma_dsp_data_write(dsp, 0x45));
	REQUIRE(dsp.attenuation[cdi_audio::ATTEN_RL] == 0x45);
	REQUIRE(cdi_audio::fma_dsp_data_write(dsp, 0x42));
	REQUIRE((dsp.attenuation == cdi_audio::attenuation_matrix{ 0x42, 0x43, 0x44, 0x45 }));
	REQUIRE(dsp.attenuation_write_index == 0);

	// Leaving DSP control mode prevents accidental gain changes from later
	// writes at the same indirect address.
	cdi_audio::fma_dsp_address_write(dsp, 0);
	REQUIRE_FALSE(cdi_audio::fma_dsp_data_write(dsp, 0xe2));
	cdi_audio::fma_dsp_address_write(dsp, 7);
	REQUIRE_FALSE(cdi_audio::fma_dsp_data_write(dsp, 0x00));
	REQUIRE((dsp.attenuation == cdi_audio::attenuation_matrix{ 0x42, 0x43, 0x44, 0x45 }));
}

TEST_CASE("CD-i FMA DSP attenuation resumes exactly from a partial saved transfer", "[emu][philips][dvc][audio][attenuation][save]")
{
	cdi_audio::fma_dsp_audio_control live;
	cdi_audio::fma_dsp_address_write(live, 0);
	cdi_audio::fma_dsp_data_write(live, 0x80);
	cdi_audio::fma_dsp_address_write(live, 1);
	cdi_audio::fma_dsp_data_write(live, 0x93);
	cdi_audio::fma_dsp_address_write(live, 7);
	cdi_audio::fma_dsp_data_write(live, 0x02);
	cdi_audio::fma_dsp_data_write(live, 0x01);

	cdi_audio::fma_dsp_audio_control restored = live;
	for (uint8_t const value : { uint8_t(0x03), uint8_t(0x00) })
	{
		REQUIRE(cdi_audio::fma_dsp_data_write(live, value));
		REQUIRE(cdi_audio::fma_dsp_data_write(restored, value));
	}

	REQUIRE(live.address == restored.address);
	REQUIRE(live.mode == restored.mode);
	REQUIRE(live.target == restored.target);
	REQUIRE(live.attenuation_write_index == restored.attenuation_write_index);
	REQUIRE(live.attenuation == restored.attenuation);
	REQUIRE((live.attenuation == cdi_audio::attenuation_matrix{ 0x00, 0x01, 0x02, 0x03 }));
}

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

TEST_CASE("CD-i DVC audio access scan accepts pack prefixes and direct frame sync", "[emu][philips][dvc][mpeg][audio]")
{
	using route = cdi_dvc::mpeg1_audio_access_route;

	uint32_t prefix = 0;
	uint32_t header_window = 0;
	uint16_t frame_bytes_remaining = 0;
	uint8_t header_bytes = 0;
	auto const scan = [&](uint8_t data)
	{
		auto const result = cdi_dvc::route_mpeg1_audio_access_byte(
			prefix, header_window, frame_bytes_remaining, header_bytes, data);
		prefix = result.start_code_prefix;
		header_window = result.audio_header_window;
		frame_bytes_remaining = result.frame_bytes_remaining;
		header_bytes = result.audio_header_bytes;
		return result;
	};

	REQUIRE(scan(0x00).route == route::scanning);
	REQUIRE(scan(0x00).route == route::scanning);
	auto const pack_prefix = scan(0x01);
	REQUIRE(pack_prefix.route == route::system_start_code);
	REQUIRE(pack_prefix.start_code_prefix == 0x000001);

	prefix = 0;
	header_window = 0;
	header_bytes = 0;
	REQUIRE(scan(0xff).route == route::scanning);
	REQUIRE(scan(0xfd).route == route::scanning);
	REQUIRE(scan(0xa2).route == route::scanning);
	auto const audio_frame = scan(0x00);
	REQUIRE(audio_frame.route == route::audio_header);
	REQUIRE(audio_frame.detected_audio_header == 0xfffda200);
	REQUIRE(audio_frame.frame_size_bytes == 627);
	REQUIRE(audio_frame.frame_bytes_remaining == 623);

	// A coincidental system prefix inside the length-bounded frame remains
	// payload.  Completion occurs on exactly the 627th byte including header.
	for (unsigned byte = 0; byte < 620; ++byte)
	{
		uint8_t const data = byte == 100 ? 0x00 : byte == 101 ? 0x00 : byte == 102 ? 0x01 : 0x5a;
		auto const payload = scan(data);
		INFO("payload byte=" << byte);
		REQUIRE(payload.route == route::audio_payload);
		REQUIRE_FALSE(payload.frame_complete);
	}
	REQUIRE(scan(0x00).route == route::audio_payload);
	REQUIRE(scan(0x00).route == route::audio_payload);
	auto const frame_end = scan(0x01);
	REQUIRE(frame_end.route == route::audio_payload);
	REQUIRE(frame_end.frame_complete);
	REQUIRE(frame_end.frame_bytes_remaining == 0);

	// Once the exact frame boundary is reached, scanning resumes and the same
	// byte sequence is recognized as a system start-code prefix.
	REQUIRE(scan(0x00).route == route::scanning);
	REQUIRE(scan(0x00).route == route::scanning);
	REQUIRE(scan(0x01).route == route::system_start_code);

	// The windows roll across arbitrary non-frame bytes.  A start-code prefix
	// is reported as soon as its third byte arrives, while a reserved-bitrate
	// false sync remains in scan mode.
	prefix = 0;
	header_window = 0;
	frame_bytes_remaining = 0;
	header_bytes = 0;
	REQUIRE(scan(0x55).route == route::scanning);
	REQUIRE(scan(0x00).route == route::scanning);
	REQUIRE(scan(0x00).route == route::scanning);
	REQUIRE(scan(0x01).route == route::system_start_code);

	prefix = 0;
	header_window = 0;
	frame_bytes_remaining = 0;
	header_bytes = 0;
	REQUIRE(scan(0xff).route == route::scanning);
	REQUIRE(scan(0xfd).route == route::scanning);
	REQUIRE(scan(0xf2).route == route::scanning);
	REQUIRE(scan(0x00).route == route::scanning);
}

TEST_CASE("CD-i DVC direct audio access classification exhausts Layer II header fields", "[emu][philips][dvc][mpeg][audio][exhaustive]")
{
	using route = cdi_dvc::mpeg1_audio_access_route;
	constexpr uint16_t bitrate_kbps[16] =
		{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 };
	constexpr uint16_t sample_rate_hz[4] = { 44'100, 48'000, 32'000, 0 };
	constexpr uint32_t fixed_header =
		(0x7ffU << 21) |
		(3U << 19) |
		(2U << 17);

	for (unsigned protection_absent = 0; protection_absent < 2; ++protection_absent)
	{
		for (uint32_t fields = 0; fields <= 0xffff; ++fields)
		{
			uint32_t const raw = fixed_header | (protection_absent << 16) | fields;
			uint32_t prefix = 0;
			uint32_t header_window = 0;
			uint16_t frame_bytes_remaining = 0;
			uint8_t header_bytes = 0;
			cdi_dvc::mpeg1_audio_access_result result{};
			for (unsigned byte = 0; byte < 4; ++byte)
			{
				result = cdi_dvc::route_mpeg1_audio_access_byte(
					prefix, header_window, frame_bytes_remaining, header_bytes,
					uint8_t(raw >> (24 - byte * 8)));
				prefix = result.start_code_prefix;
				header_window = result.audio_header_window;
				frame_bytes_remaining = result.frame_bytes_remaining;
				header_bytes = result.audio_header_bytes;
			}

			unsigned const bitrate_index = (fields >> 12) & 0x0f;
			unsigned const sample_rate_index = (fields >> 10) & 0x03;
			bool const valid = bitrate_index > 0 && bitrate_index < 15
				&& sample_rate_index < 3;
			uint16_t const expected_size = valid ? uint16_t(
				(144'000U * bitrate_kbps[bitrate_index]) /
					sample_rate_hz[sample_rate_index] + ((fields >> 9) & 1U)) : 0;

			INFO("protection_absent=" << protection_absent << " fields=" << fields);
			REQUIRE(result.route == (valid ? route::audio_header : route::scanning));
			REQUIRE(result.frame_size_bytes == expected_size);
			REQUIRE(result.frame_bytes_remaining == (valid ? expected_size - 4 : 0));
		}
	}
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
