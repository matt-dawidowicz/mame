// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "catch.hpp"

#include "cdi_dvc_dma_service.h"
#include "cdidvc_save_state.h"
#include "cdislavehle_pointer.h"
#include "cdislavehle_transport.h"

namespace
{

uint8_t expected_button_field(uint8_t buttons)
{
	uint8_t field = 0x08;
	if (buttons & 0x01U)
		field |= 0x10U;
	if (buttons & 0x02U)
		field |= 0x20U;
	if (buttons & 0x04U)
		field |= 0x30U;
	return field;
}

} // anonymous namespace


TEST_CASE(
	"CD-i DVC held DMA request survives SCC abort and completes exactly once after re-arm",
	"[emu][philips][cdi][dvc][dma][hardening]")
{
	using cdi_dvc_dma::post_transfer_action;
	using cdi_dvc_dma::reconfigure_action;

	bool service_active = true;
	bool request_asserted = true;
	unsigned completions = 0;

	// A partial transfer is already in progress when firmware aborts SCC DMA
	// channel 2 before the scheduled driver service tick observes the abort.
	REQUIRE(cdi_dvc_dma::reconfigure(
		service_active, 1, request_asserted, false) == reconfigure_action::stop_service);
	REQUIRE_FALSE(service_active);
	REQUIRE(request_asserted);
	REQUIRE(completions == 0);

	// Invalid intermediate programming must not restart the held request.
	REQUIRE_FALSE(cdi_dvc_dma::request_configuration_valid(false, true, true, 2));
	REQUIRE_FALSE(cdi_dvc_dma::request_configuration_valid(true, false, true, 2));
	REQUIRE_FALSE(cdi_dvc_dma::request_configuration_valid(true, true, false, 2));
	REQUIRE_FALSE(cdi_dvc_dma::request_configuration_valid(true, true, true, 0));

	// Once firmware repairs channel 2, the still-asserted DREQ is eligible to
	// retry the existing request-start path.
	REQUIRE(cdi_dvc_dma::request_configuration_valid(true, true, true, 2));
	REQUIRE(cdi_dvc_dma::reconfigure(
		service_active, 1, request_asserted, false) == reconfigure_action::retry_request);

	service_active = true;

	// The first resumed word leaves SCC DMA active and therefore cannot signal
	// DVC completion.
	REQUIRE(cdi_dvc_dma::post_transfer(service_active, true) == post_transfer_action::schedule_next);
	REQUIRE(service_active);
	REQUIRE(completions == 0);

	// The final resumed word is the sole completion edge.
	const post_transfer_action final_action = cdi_dvc_dma::post_transfer(service_active, false);
	REQUIRE(final_action == post_transfer_action::complete);
	REQUIRE_FALSE(service_active);
	if (final_action == post_transfer_action::complete)
		++completions;

	// DVC completion deasserts DREQ. Later SCC writes must not fabricate a
	// second completion or restart the request.
	request_asserted = false;
	REQUIRE(cdi_dvc_dma::reconfigure(
		service_active, 1, request_asserted, false) == reconfigure_action::ignore);
	REQUIRE(completions == 1);
}


TEST_CASE(
	"CD-i SLAVE channel bounds reject every index outside the four-channel transport",
	"[emu][philips][cdi][hardening][transport]")
{
	constexpr std::size_t CHANNELS = 4;

	for (std::size_t index = 0; index < 16; ++index)
	{
		INFO("index=" << index);
		REQUIRE(cdi_slave_transport::channel_index_valid(index, CHANNELS) == (index < CHANNELS));
	}

	REQUIRE_FALSE(cdi_slave_transport::channel_index_valid(0, 0));
	REQUIRE_FALSE(cdi_slave_transport::channel_index_valid(
		std::numeric_limits<std::size_t>::max(), CHANNELS));
}


TEST_CASE(
	"CD-i SLAVE response windows never escape the fixed output buffer",
	"[emu][philips][cdi][hardening][transport]")
{
	constexpr std::size_t CAPACITY = 4;

	for (std::size_t index = 0; index <= CAPACITY + 2; ++index)
	{
		for (std::size_t count = 0; count <= CAPACITY + 2; ++count)
		{
			const bool expected = count == 0
				? index == 0
				: index < CAPACITY && count <= CAPACITY - index;

			INFO("index=" << index << " count=" << count);
			REQUIRE(cdi_slave_transport::response_window_fits(index, count, CAPACITY) == expected);
		}
	}

	REQUIRE_FALSE(cdi_slave_transport::response_window_fits(
		std::numeric_limits<std::size_t>::max(), 1, CAPACITY));
	REQUIRE_FALSE(cdi_slave_transport::response_window_fits(
		1, std::numeric_limits<std::size_t>::max(), CAPACITY));
	REQUIRE_FALSE(cdi_slave_transport::response_window_fits(
		std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), CAPACITY));
}


TEST_CASE(
	"CD-i SLAVE input writes accept exactly the fixed command-buffer range",
	"[emu][philips][cdi][hardening][transport]")
{
	constexpr std::size_t CAPACITY = 17;

	for (std::size_t index = 0; index <= CAPACITY + 2; ++index)
	{
		INFO("index=" << index);
		REQUIRE(cdi_slave_transport::input_write_fits(index, CAPACITY) == (index < CAPACITY));
	}

	REQUIRE_FALSE(cdi_slave_transport::input_write_fits(
		std::numeric_limits<std::size_t>::max(), CAPACITY));
}


TEST_CASE(
	"CD-i SLAVE pointer readback ignores every non-pointer host button bit",
	"[emu][philips][cdi][hardening][pointer]")
{
	constexpr std::array<cdi_slave_pointer::position, 5> positions =
	{{
		{ 0, 0 },
		{ 1, 1 },
		{ 383, 279 },
		{ 766, 558 },
		{ 767, 559 }
	}};

	for (const auto &position : positions)
	{
		for (unsigned buttons = 0; buttons <= 0xff; ++buttons)
		{
			const auto packet = cdi_slave_pointer::encode_readback(
				position.x, position.y, uint8_t(buttons));

			INFO("x=" << position.x << " y=" << position.y << " buttons=" << buttons);
			REQUIRE((packet[0] & 0x38U) == expected_button_field(uint8_t(buttons)));
			REQUIRE(((packet[0] & 0x07U) << 7 | (packet[1] & 0x7fU)) == uint16_t(position.x));
			REQUIRE(((packet[2] & 0x07U) << 7 | (packet[3] & 0x7fU)) == uint16_t(position.y));
		}
	}
}


TEST_CASE(
	"CD-i SLAVE pointer clamps the complete signed 32-bit input extremes",
	"[emu][philips][cdi][hardening][pointer]")
{
	REQUIRE(cdi_slave_pointer::clamp_x(std::numeric_limits<int32_t>::min()) == 0);
	REQUIRE(cdi_slave_pointer::clamp_x(-1) == 0);
	REQUIRE(cdi_slave_pointer::clamp_x(0) == 0);
	REQUIRE(cdi_slave_pointer::clamp_x(767) == 767);
	REQUIRE(cdi_slave_pointer::clamp_x(768) == 767);
	REQUIRE(cdi_slave_pointer::clamp_x(std::numeric_limits<int32_t>::max()) == 767);

	REQUIRE(cdi_slave_pointer::clamp_y(std::numeric_limits<int32_t>::min()) == 0);
	REQUIRE(cdi_slave_pointer::clamp_y(-1) == 0);
	REQUIRE(cdi_slave_pointer::clamp_y(0) == 0);
	REQUIRE(cdi_slave_pointer::clamp_y(559) == 559);
	REQUIRE(cdi_slave_pointer::clamp_y(560) == 559);
	REQUIRE(cdi_slave_pointer::clamp_y(std::numeric_limits<int32_t>::max()) == 559);
}


TEST_CASE(
	"CD-i DVC save mirrors accept every exact capacity boundary and reject the next value",
	"[emu][philips][cdi][dvc][hardening][save]")
{
	REQUIRE(cdi_dvc::save_replay_fits(
		cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY, cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY));
	REQUIRE_FALSE(cdi_dvc::save_replay_fits(
		cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY + 1, cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY));

	REQUIRE(cdi_dvc::save_replay_fits(
		cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY, cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY));
	REQUIRE_FALSE(cdi_dvc::save_replay_fits(
		cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY + 1, cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY));

	REQUIRE(cdi_dvc::save_audio_pcm_fits(cdi_dvc::SAVE_AUDIO_PCM_VALUES));
	REQUIRE_FALSE(cdi_dvc::save_audio_pcm_fits(cdi_dvc::SAVE_AUDIO_PCM_VALUES + 1));

	REQUIRE(cdi_dvc::save_video_queue_fits(cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES));
	REQUIRE_FALSE(cdi_dvc::save_video_queue_fits(cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES + 1));

	REQUIRE(cdi_dvc::save_picture_events_fit(cdi_dvc::SAVE_PICTURE_EVENTS));
	REQUIRE_FALSE(cdi_dvc::save_picture_events_fit(cdi_dvc::SAVE_PICTURE_EVENTS + 1));

	REQUIRE(cdi_dvc::save_video_replay_pumps_fit(cdi_dvc::SAVE_VIDEO_REPLAY_PUMP_EVENTS));
	REQUIRE_FALSE(cdi_dvc::save_video_replay_pumps_fit(cdi_dvc::SAVE_VIDEO_REPLAY_PUMP_EVENTS + 1));
}


TEST_CASE(
	"CD-i DVC saved video geometry requires exact bounded pixel storage",
	"[emu][philips][cdi][dvc][hardening][save]")
{
	REQUIRE(cdi_dvc::save_video_frame_fits(
		cdi_dvc::SAVE_VIDEO_MAX_WIDTH,
		cdi_dvc::SAVE_VIDEO_MAX_HEIGHT,
		cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME));

	REQUIRE_FALSE(cdi_dvc::save_video_frame_fits(
		cdi_dvc::SAVE_VIDEO_MAX_WIDTH + 1,
		cdi_dvc::SAVE_VIDEO_MAX_HEIGHT,
		cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME));
	REQUIRE_FALSE(cdi_dvc::save_video_frame_fits(
		cdi_dvc::SAVE_VIDEO_MAX_WIDTH,
		cdi_dvc::SAVE_VIDEO_MAX_HEIGHT + 1,
		cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME));
	REQUIRE_FALSE(cdi_dvc::save_video_frame_fits(320, 240, 320U * 240U - 1U));
	REQUIRE_FALSE(cdi_dvc::save_video_frame_fits(320, 240, 320U * 240U + 1U));

	REQUIRE(cdi_dvc::video_present_frame_fits(false, 0, 0, 0));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(false, 0, 0, 1));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(false, 1, 1, 0));
	REQUIRE_FALSE(cdi_dvc::video_present_frame_fits(true, 0, 0, 0));
}


TEST_CASE(
	"CD-i DVC replay pump offsets reject backwards and out-of-range journals without overflow",
	"[emu][philips][cdi][dvc][hardening][save]")
{
	constexpr std::size_t BYTES = cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY;

	REQUIRE(cdi_dvc::save_video_replay_pump_offset_valid(0, 0, BYTES));
	REQUIRE(cdi_dvc::save_video_replay_pump_offset_valid(0, BYTES, BYTES));
	REQUIRE(cdi_dvc::save_video_replay_pump_offset_valid(BYTES, BYTES, BYTES));
	REQUIRE_FALSE(cdi_dvc::save_video_replay_pump_offset_valid(1, 0, BYTES));
	REQUIRE_FALSE(cdi_dvc::save_video_replay_pump_offset_valid(0, BYTES + 1, BYTES));
	REQUIRE_FALSE(cdi_dvc::save_video_replay_pump_offset_valid(
		std::numeric_limits<std::size_t>::max() - 1,
		std::numeric_limits<std::size_t>::max(), BYTES));
}
