// license:BSD-3-Clause
// copyright-holders:Matt Jordan
#ifndef MAME_PHILIPS_CDISLAVEHLE_TRANSPORT_H
#define MAME_PHILIPS_CDISLAVEHLE_TRANSPORT_H

#pragma once

#include <cstddef>

namespace cdi_slave_transport
{

// Multiple asynchronous SLAVE responses must not postpone an interrupt
// that is already due sooner.  Preserve the earliest pending deadline.
template <typename Delay>
constexpr Delay select_interrupt_delay(
	Delay current_remaining,
	Delay incoming) noexcept
{
	return incoming < current_remaining
		? incoming
		: current_remaining;
}


// Reset returns asynchronous pointer reporting to its disabled state.
constexpr bool reset_pointer_input_enabled(bool previous) noexcept
{
	(void)previous;
	return false;
}

// Keep every externally supplied channel/index/count value inside the fixed
// transport storage before the device indexes a channel or response byte.
constexpr bool channel_index_valid(std::size_t index, std::size_t channel_count) noexcept
{
	return index < channel_count;
}

constexpr bool input_write_fits(std::size_t index, std::size_t capacity) noexcept
{
	return index < capacity;
}

constexpr bool response_window_fits(
	std::size_t index,
	std::size_t count,
	std::size_t capacity) noexcept
{
	// Empty responses are normalized to index zero.  A non-empty response must
	// keep the complete unread window within the fixed output buffer.
	if (count == 0)
		return index == 0;

	return index < capacity && count <= capacity - index;
}

// A response may exist in a channel before its interrupt deadline expires.
// Stage-10E behavior effectively made every prepared response immediately
// readable and allowed every pending response to hold IRQ asserted.
// These helpers keep queued response data separate from response readiness.
constexpr bool response_ready_on_prepare(bool immediate) noexcept
{
	return immediate;
}

constexpr bool response_readable(bool pending, bool ready) noexcept
{
	return pending && ready;
}

constexpr bool response_holds_irq(bool pending, bool ready) noexcept
{
	return pending && ready;
}

} // namespace cdi_slave_transport

#endif // MAME_PHILIPS_CDISLAVEHLE_TRANSPORT_H
