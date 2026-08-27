// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDI_DVC_DMA_SERVICE_H
#define MAME_PHILIPS_CDI_DVC_DMA_SERVICE_H

#pragma once

#include <cstdint>

namespace cdi_dvc_dma
{

enum class reconfigure_action : uint8_t
{
	ignore,
	stop_service,
	retry_request
};

enum class post_transfer_action : uint8_t
{
	schedule_next,
	complete
};

constexpr bool request_configuration_valid(
		bool memory_to_device,
		bool word_transfer,
		bool memory_mode_valid,
		uint16_t remaining) noexcept
{
	return memory_to_device && word_transfer && memory_mode_valid && remaining != 0;
}

constexpr reconfigure_action reconfigure(
		bool &service_active,
		uint8_t channel,
		bool request_asserted,
		bool channel_active) noexcept
{
	if (channel != 1 || !request_asserted)
		return reconfigure_action::ignore;

	if (service_active)
	{
		if (channel_active)
			return reconfigure_action::ignore;

		service_active = false;
		return reconfigure_action::stop_service;
	}

	return reconfigure_action::retry_request;
}

constexpr post_transfer_action post_transfer(
		bool &service_active,
		bool channel_active) noexcept
{
	if (channel_active)
		return post_transfer_action::schedule_next;

	service_active = false;
	return post_transfer_action::complete;
}

} // namespace cdi_dvc_dma

#endif // MAME_PHILIPS_CDI_DVC_DMA_SERVICE_H
