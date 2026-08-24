// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIMONO2_H
#define MAME_PHILIPS_CDIMONO2_H

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace cdi_mono2
{

// Phase-E evidence labels:
// A: primary hardware/service documentation
// B: firmware, source history, or independently inferred behavior
// C: deliberate compatibility model
// D: unknown or blocked; no behavior is fabricated
enum class evidence : uint8_t
{
	hardware,
	inferred,
	compatibility,
	unknown
};

enum class mapping : uint8_t
{
	mapped,
	blocked
};

struct address_region
{
	uint32_t start;
	uint32_t end;
	std::string_view name;
	mapping state;
	evidence basis;
};

inline constexpr uint32_t MAIN_CLOCK = 30'209'800;
inline constexpr uint32_t MCU_CLOCK = 4'000'000;
inline constexpr uint32_t DRVDSP_CLOCK = 27'000'000;

inline constexpr uint32_t PLANE_A_START = 0x000000;
inline constexpr uint32_t PLANE_A_END = 0x07ffff;
inline constexpr uint32_t PLANE_B_START = 0x200000;
inline constexpr uint32_t PLANE_B_END = 0x27ffff;
inline constexpr uint32_t DRVDSP_HOST_START = 0x300000;
inline constexpr uint32_t DRVDSP_HOST_END = 0x30000f;
inline constexpr uint32_t SLAVE_HOST_START = 0x310000;
inline constexpr uint32_t SLAVE_HOST_END = 0x317fff;
inline constexpr uint32_t NVRAM_START = 0x320000;
inline constexpr uint32_t NVRAM_END = 0x323fff;
inline constexpr uint32_t BOOT_ROM_START = 0x400000;
inline constexpr uint32_t BOOT_ROM_END = 0x47ffff;
inline constexpr uint32_t MCD212_START = 0x4fffe0;
inline constexpr uint32_t MCD212_END = 0x4fffff;

inline constexpr std::array<address_region, 7> ADDRESS_REGIONS =
{{
	{ PLANE_A_START, PLANE_A_END, "plane A RAM", mapping::mapped, evidence::inferred },
	{ PLANE_B_START, PLANE_B_END, "plane B RAM", mapping::mapped, evidence::inferred },
	{ DRVDSP_HOST_START, DRVDSP_HOST_END, "DRVDSP host interface", mapping::blocked, evidence::inferred },
	{ SLAVE_HOST_START, SLAVE_HOST_END, "SLAVE host mailbox", mapping::blocked, evidence::hardware },
	{ NVRAM_START, NVRAM_END, "timekeeper NVRAM", mapping::mapped, evidence::inferred },
	{ BOOT_ROM_START, BOOT_ROM_END, "boot ROM", mapping::mapped, evidence::inferred },
	{ MCD212_START, MCD212_END, "MCD212 registers", mapping::mapped, evidence::inferred }
}};

struct host_register
{
	uint32_t address;
	std::string_view name;
};

// The DSP is an 8-bit host device with its register select shifted by the
// disconnected SCC68070 A0 line.  These addresses are documented for future
// host-interface work; they are deliberately not mapped to synthetic storage.
inline constexpr std::array<host_register, 7> DRVDSP_HOST_REGISTERS =
{{
	{ 0x300001, "ICR" },
	{ 0x300003, "CVR" },
	{ 0x300005, "ISR" },
	{ 0x300007, "IVR" },
	{ 0x30000b, "RXH/TXH" },
	{ 0x30000d, "RXM/TXM" },
	{ 0x30000f, "RXL/TXL" }
}};

constexpr const address_region *find_region(uint32_t address) noexcept
{
	for (const address_region &region : ADDRESS_REGIONS)
		if (address >= region.start && address <= region.end)
			return &region;
	return nullptr;
}

constexpr bool drv_dsp_host_register(uint32_t address) noexcept
{
	for (const host_register &reg : DRVDSP_HOST_REGISTERS)
		if (reg.address == address)
			return true;
	return false;
}

// SLAVE port B bit 5 is active-low IRQ2 to the SCC68070.
constexpr int slave_irq2_line(uint8_t port_b) noexcept
{
	return (port_b & 0x20U) ? 0 : 1;
}

inline constexpr int RESET_IRQ2_LINE = 0;

// Explicit implementation boundaries.  These constants are structural
// assertions, not promises of emulated behavior.
inline constexpr bool SLAVE_IRQ2_CONNECTED = true;
inline constexpr bool SLAVE_HOST_DTACK_AVAILABLE = false;
inline constexpr bool MCU_SPI_PIN_API_AVAILABLE = false;
inline constexpr bool DRVDSP_EXECUTION_AVAILABLE = false;
inline constexpr bool DRVDSP_HOST_INTERFACE_AVAILABLE = false;

} // namespace cdi_mono2

#endif // MAME_PHILIPS_CDIMONO2_H
