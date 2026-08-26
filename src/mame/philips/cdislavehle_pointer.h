// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz
#ifndef MAME_PHILIPS_CDISLAVEHLE_POINTER_H
#define MAME_PHILIPS_CDISLAVEHLE_POINTER_H

#pragma once

#include <array>
#include <cstdint>

namespace cdi_slave_pointer
{

struct position
{
	int16_t x;
	int16_t y;
};

using readback_packet = std::array<uint8_t, 4>;

constexpr position decode_set_position(uint8_t byte0, uint8_t byte1, uint8_t byte2) noexcept
{
	// Reconstruct the documented 10-bit high-resolution coordinates from the
	// three-byte Set pointer position command.
	return
	{
		int16_t(((uint16_t(byte1) & 0x70U) << 3) | (uint16_t(byte2) & 0x7fU)),
		int16_t(((uint16_t(byte1) & 0x0fU) << 6) | (uint16_t(byte0) & 0x3fU))
	};
}

constexpr readback_packet encode_readback(int16_t x, int16_t y, uint8_t buttons) noexcept
{
	// Encode the documented high-resolution pointer response packet.
	uint8_t byte3 = uint8_t(((uint16_t(x) & 0x380U) >> 7) | 0x08U);

	if (buttons & 0x01U)
		byte3 |= 0x10U;

	if (buttons & 0x02U)
		byte3 |= 0x20U;

	// The third host button represents both CD-i pointer buttons.
	if (buttons & 0x04U)
		byte3 |= 0x30U;

	const uint8_t byte2 = uint8_t(uint16_t(x) & 0x7fU);
	const uint8_t byte1 = uint8_t((uint16_t(y) & 0x380U) >> 7);
	const uint8_t byte0 = uint8_t(uint16_t(y) & 0x7fU);

	// Matches prepare_readback(data0, data1, data2, data3).
	return { byte3, byte2, byte1, byte0 };
}

constexpr int16_t host_delta(uint16_t previous, uint16_t current) noexcept
{
	// Convert the modulo-16-bit counter displacement without relying on
	// implementation-defined out-of-range signed narrowing.
	uint16_t const raw = uint16_t(current - previous);
	if (raw <= 0x7fffU)
		return int16_t(raw);

	return int16_t(int32_t(raw) - 0x10000);
}

constexpr bool host_sample_changed(
	bool initialized,
	uint16_t previous_x,
	uint16_t previous_y,
	uint8_t previous_buttons,
	uint16_t current_x,
	uint16_t current_y,
	uint8_t current_buttons) noexcept
{
	return
		!initialized ||
		previous_x != current_x ||
		previous_y != current_y ||
		previous_buttons != current_buttons;
}

constexpr position decode_host_movement(
	bool initialized,
	uint16_t previous_x,
	uint16_t previous_y,
	uint16_t current_x,
	uint16_t current_y) noexcept
{
	if (!initialized)
		return { 0, 0 };

	return
	{
		host_delta(previous_x, current_x),
		host_delta(previous_y, current_y)
	};
}

constexpr int16_t clamp_x(int32_t value) noexcept
{
	return value < 0 ? 0 : (value > 767 ? 767 : int16_t(value));
}

constexpr int16_t clamp_y(int32_t value) noexcept
{
	return value < 0 ? 0 : (value > 559 ? 559 : int16_t(value));
}

} // namespace cdi_slave_pointer

#endif // MAME_PHILIPS_CDISLAVEHLE_POINTER_H
