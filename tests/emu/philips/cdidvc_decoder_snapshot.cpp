// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz
// Included in the PL_MPEG implementation test translation unit.
#include "cdi_dvc_av_reference_data.h"
#include "cdi_dvc_motion_reference_data.h"
#include <memory>

namespace cdi_decoder_snapshot_test
{
namespace
{
using snapshot_audio = std::unique_ptr<plm_audio_t, decltype(&plm_audio_destroy)>;
using snapshot_video = std::unique_ptr<plm_video_t, decltype(&plm_video_destroy)>;
snapshot_audio fresh_snapshot_audio()
{
	return snapshot_audio(plm_audio_create_with_buffer(plm_buffer_create_with_capacity(128), 1), plm_audio_destroy);
}
snapshot_video fresh_snapshot_video()
{
	return snapshot_video(plm_video_create_with_buffer(plm_buffer_create_with_capacity(128), 1), plm_video_destroy);
}
uint64_t snapshot_bytes_hash(uint8_t const *bytes, std::size_t length, uint64_t hash = 1469598103934665603ULL)
{
	for (std::size_t i = 0; i < length; ++i)
		hash = (hash ^ bytes[i]) * 1099511628211ULL;
	return hash;
}
std::pair<double, uint64_t> snapshot_picture(plm_frame_t const *frame)
{
	uint64_t hash = 1469598103934665603ULL;
	for (auto const *plane : {&frame->y, &frame->cr, &frame->cb})
		hash = snapshot_bytes_hash(plane->data, std::size_t(plane->width) * plane->height, hash);
	return {frame->time, hash};
}
std::pair<double, uint64_t> snapshot_samples(plm_samples_t const *samples)
{
	return {samples->time,
			snapshot_bytes_hash(reinterpret_cast<uint8_t const *>(samples->interleaved), sizeof(samples->interleaved))};
}
} // namespace

TEST_CASE("DVC backend snapshots preserve every moving I P B reference permutation", "[philips][dvc][decoder-snapshot]")
{
	std::array<std::vector<uint8_t>, 3> const streams = {
		std::vector<uint8_t>(cdi_motion_reference::VIDEO_0.begin(), cdi_motion_reference::VIDEO_0.end()),
		std::vector<uint8_t>(cdi_motion_reference::VIDEO_1.begin(), cdi_motion_reference::VIDEO_1.end()),
		std::vector<uint8_t>(cdi_motion_reference::VIDEO_2.begin(), cdi_motion_reference::VIDEO_2.end())};
	for (unsigned profile = 0; profile < streams.size(); ++profile)
	{
		CAPTURE(profile);
		auto reference = fresh_snapshot_video();
		plm_buffer_write(reference->buffer, const_cast<uint8_t *>(streams[profile].data()), streams[profile].size());
		plm_buffer_signal_end(reference->buffer);
		std::vector<std::pair<double, uint64_t>> expected;
		while (auto const *frame = plm_video_decode(reference.get()))
			expected.push_back(snapshot_picture(frame));
		REQUIRE(expected.size() == cdi_motion_reference::PROFILES[profile].frames);
		auto live = fresh_snapshot_video();
		plm_buffer_write(live->buffer, const_cast<uint8_t *>(streams[profile].data()), streams[profile].size());
		plm_buffer_signal_end(live->buffer);
		for (unsigned at = 0; at <= expected.size(); ++at)
		{
			CAPTURE(at);
			std::vector<uint8_t> image(1024 * 1024), roundtrip(image.size());
			auto const length = cdi_dvc::plmpeg_video_snapshot_write(live.get(), image.data(), image.size());
			REQUIRE(length > 0);
			auto restored = fresh_snapshot_video();
			REQUIRE(cdi_dvc::plmpeg_video_snapshot_read(restored.get(), image.data(), length));
			REQUIRE(cdi_dvc::plmpeg_video_snapshot_write(restored.get(), roundtrip.data(), roundtrip.size()) == length);
			CHECK(std::equal(image.begin(), image.begin() + length, roundtrip.begin()));
			for (unsigned frame = at; frame < expected.size(); ++frame)
			{
				auto const *decoded = plm_video_decode(restored.get());
				REQUIRE(decoded);
				CHECK(snapshot_picture(decoded) == expected[frame]);
			}
			CHECK(plm_video_decode(restored.get()) == nullptr);
			CHECK(plm_video_has_ended(restored.get()));
			if (at < expected.size())
				REQUIRE(plm_video_decode(live.get()));
		}
	}
}

TEST_CASE("DVC backend snapshots preserve audio synthesis history and partial input",
		  "[philips][dvc][decoder-snapshot]")
{
	auto live = fresh_snapshot_audio();
	plm_buffer_write(live->buffer, const_cast<uint8_t *>(cdi_av_reference::AUDIO.data()),
					 cdi_av_reference::AUDIO.size());
	plm_buffer_signal_end(live->buffer);
	std::vector<std::pair<double, uint64_t>> expected;
	while (auto const *samples = plm_audio_decode(live.get()))
		expected.push_back(snapshot_samples(samples));
	REQUIRE(expected.size() == 98);
	for (unsigned boundary : {0U, 1U, 3U, 100U, 625U, 626U, 627U, 1800U})
	{
		CAPTURE(boundary);
		live = fresh_snapshot_audio();
		plm_buffer_write(live->buffer, const_cast<uint8_t *>(cdi_av_reference::AUDIO.data()), boundary);
		unsigned decoded = 0;
		while (plm_audio_decode(live.get()))
			++decoded;
		std::vector<uint8_t> image(1024 * 1024), roundtrip(image.size());
		auto const length = cdi_dvc::plmpeg_audio_snapshot_write(live.get(), image.data(), image.size());
		REQUIRE(length > 0);
		auto restored = fresh_snapshot_audio();
		REQUIRE(cdi_dvc::plmpeg_audio_snapshot_read(restored.get(), image.data(), length));
		REQUIRE(cdi_dvc::plmpeg_audio_snapshot_write(restored.get(), roundtrip.data(), roundtrip.size()) == length);
		CHECK(std::equal(image.begin(), image.begin() + length, roundtrip.begin()));
		plm_buffer_write(restored->buffer, const_cast<uint8_t *>(cdi_av_reference::AUDIO.data() + boundary),
						 cdi_av_reference::AUDIO.size() - boundary);
		plm_buffer_signal_end(restored->buffer);
		while (auto const *samples = plm_audio_decode(restored.get()))
		{
			REQUIRE(decoded < expected.size());
			CHECK(snapshot_samples(samples) == expected[decoded++]);
		}
		CHECK(decoded == expected.size());
		CHECK(plm_audio_has_ended(restored.get()));
	}
}

TEST_CASE("DVC decoder snapshot images reject truncation wrong version and short output",
		  "[philips][dvc][decoder-snapshot]")
{
	auto audio = fresh_snapshot_audio();
	auto video = fresh_snapshot_video();
	std::vector<uint8_t> image(1024 * 1024, 0xa5);
	auto const audio_length = cdi_dvc::plmpeg_audio_snapshot_write(audio.get(), image.data(), image.size());
	REQUIRE(audio_length > 0);
	for (std::size_t length : {std::size_t(0), std::size_t(1), std::size_t(4), audio_length - 1})
	{
		auto restored = fresh_snapshot_audio();
		CHECK_FALSE(cdi_dvc::plmpeg_audio_snapshot_read(restored.get(), image.data(), length));
	}
	image[0] ^= 1;
	CHECK_FALSE(cdi_dvc::plmpeg_audio_snapshot_read(audio.get(), image.data(), audio_length));
	auto const video_length = cdi_dvc::plmpeg_video_snapshot_write(video.get(), image.data(), image.size());
	REQUIRE(video_length > 0);
	for (std::size_t length : {std::size_t(0), std::size_t(1), std::size_t(4), video_length - 1})
	{
		auto restored = fresh_snapshot_video();
		CHECK_FALSE(cdi_dvc::plmpeg_video_snapshot_read(restored.get(), image.data(), length));
	}
	image[0] ^= 1;
	CHECK_FALSE(cdi_dvc::plmpeg_video_snapshot_read(video.get(), image.data(), video_length));
	image.assign(1024 * 1024, 0xa5);
	CHECK(cdi_dvc::plmpeg_video_snapshot_write(video.get(), image.data(), video_length - 1) == 0);
	CHECK(image[video_length - 1] == 0xa5);
	CHECK(cdi_dvc::plmpeg_audio_snapshot_write(audio.get(), image.data(), audio_length - 1) == 0);
	CHECK(image[audio_length - 1] == 0xa5);
}

TEST_CASE("DVC snapshots reject invalid audio sample rates and video geometry", "[philips][dvc][decoder-snapshot]")
{
	auto audio = fresh_snapshot_audio();
	plm_buffer_write(audio->buffer, const_cast<uint8_t *>(cdi_av_reference::AUDIO.data()),
					 cdi_av_reference::AUDIO.size());
	REQUIRE(plm_audio_decode(audio.get()));
	std::vector<uint8_t> image(1024 * 1024);
	auto length = cdi_dvc::plmpeg_audio_snapshot_write(audio.get(), image.data(), image.size());
	REQUIRE(length > 0);
	// DVA1: magic, time, decoded count, sample-rate index.
	image[16] = 3;
	auto restored_audio = fresh_snapshot_audio();
	CHECK_FALSE(cdi_dvc::plmpeg_audio_snapshot_read(restored_audio.get(), image.data(), length));
	auto video = fresh_snapshot_video();
	plm_buffer_write(video->buffer, const_cast<uint8_t *>(cdi_motion_reference::VIDEO_0.data()),
					 cdi_motion_reference::VIDEO_0.size());
	REQUIRE(plm_video_decode(video.get()));
	length = cdi_dvc::plmpeg_video_snapshot_write(video.get(), image.data(), image.size());
	REQUIRE(length > 0);
	// DVV1: magic, three doubles, decoded count, width.
	std::fill_n(image.begin() + 32, 4, 0xff);
	auto restored_video = fresh_snapshot_video();
	CHECK_FALSE(cdi_dvc::plmpeg_video_snapshot_read(restored_video.get(), image.data(), length));
}

} // namespace cdi_decoder_snapshot_test
