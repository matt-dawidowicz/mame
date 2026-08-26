// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#include <array>
#include <cstdint>
#include <string_view>

#include "catch.hpp"

#include "cdimono2.h"

TEST_CASE(
	"CD-i Mono-II board map has ordered non-overlapping documented regions",
	"[emu][philips][cdi][mono2][structure][map]")
{
	for (std::size_t index = 0; index < cdi_mono2::ADDRESS_REGIONS.size(); ++index)
	{
		const auto &region = cdi_mono2::ADDRESS_REGIONS[index];
		INFO("region=" << region.name);
		REQUIRE(region.start <= region.end);
		REQUIRE(cdi_mono2::find_region(region.start) == &region);
		REQUIRE(cdi_mono2::find_region(region.end) == &region);

		if (index)
			REQUIRE(cdi_mono2::ADDRESS_REGIONS[index - 1].end < region.start);
	}

	REQUIRE(cdi_mono2::find_region(0x100000) == nullptr);
	REQUIRE(cdi_mono2::find_region(0xffffff) == nullptr);
}

TEST_CASE(
	"CD-i Mono-II startup clocks and mapped board components stay explicit",
	"[emu][philips][cdi][mono2][structure][startup]")
{
	REQUIRE(cdi_mono2::MAIN_CLOCK == 30'209'800);
	REQUIRE(cdi_mono2::MCU_CLOCK == 4'000'000);
	REQUIRE(cdi_mono2::DRVDSP_CLOCK == 27'000'000);

	REQUIRE(cdi_mono2::find_region(cdi_mono2::PLANE_A_START)->state == cdi_mono2::mapping::mapped);
	REQUIRE(cdi_mono2::find_region(cdi_mono2::PLANE_B_START)->state == cdi_mono2::mapping::mapped);
	REQUIRE(cdi_mono2::find_region(cdi_mono2::NVRAM_START)->state == cdi_mono2::mapping::mapped);
	REQUIRE(cdi_mono2::find_region(cdi_mono2::BOOT_ROM_START)->state == cdi_mono2::mapping::mapped);
	REQUIRE(cdi_mono2::find_region(cdi_mono2::MCD212_START)->state == cdi_mono2::mapping::mapped);
}

TEST_CASE(
	"CD-i Mono-II SLAVE IRQ2 pin conversion and reset state are deterministic",
	"[emu][philips][cdi][mono2][structure][irq][reset]")
{
	REQUIRE(cdi_mono2::SLAVE_IRQ2_CONNECTED);
	REQUIRE(cdi_mono2::RESET_IRQ2_LINE == 0);

	for (unsigned data = 0; data <= 0xff; ++data)
	{
		INFO("port_b=" << data);
		REQUIRE(cdi_mono2::slave_irq2_line(uint8_t(data)) == ((data & 0x20U) ? 0 : 1));
	}
}

TEST_CASE(
	"CD-i Mono-II DRVDSP host register addresses retain the missing A0 wiring",
	"[emu][philips][cdi][mono2][structure][dsp]")
{
	constexpr std::array<uint32_t, 7> EXPECTED =
	{{
		0x300001, 0x300003, 0x300005, 0x300007,
		0x30000b, 0x30000d, 0x30000f
	}};

	for (std::size_t index = 0; index < EXPECTED.size(); ++index)
	{
		const auto &reg = cdi_mono2::DRVDSP_HOST_REGISTERS[index];
		INFO("register=" << reg.name);
		REQUIRE(reg.address == EXPECTED[index]);
		REQUIRE((reg.address & 1U) == 1U);
		REQUIRE(cdi_mono2::drv_dsp_host_register(reg.address));
	}

	REQUIRE_FALSE(cdi_mono2::drv_dsp_host_register(0x300000));
	REQUIRE_FALSE(cdi_mono2::drv_dsp_host_register(0x300009));
	REQUIRE_FALSE(cdi_mono2::drv_dsp_host_register(0x300010));
}

TEST_CASE(
	"CD-i Mono-II board integration boundaries remain explicit",
	"[emu][philips][cdi][mono2][structure][boundary]")
{
	const auto *const dsp = cdi_mono2::find_region(cdi_mono2::DRVDSP_HOST_START);
	const auto *const slave = cdi_mono2::find_region(cdi_mono2::SLAVE_HOST_START);
	REQUIRE(dsp != nullptr);
	REQUIRE(slave != nullptr);
	REQUIRE(dsp->state == cdi_mono2::mapping::blocked);
	REQUIRE(slave->state == cdi_mono2::mapping::blocked);

	REQUIRE_FALSE(cdi_mono2::SLAVE_HOST_DTACK_AVAILABLE);
	REQUIRE_FALSE(cdi_mono2::MCU_SPI_PIN_API_AVAILABLE);
	REQUIRE_FALSE(cdi_mono2::DRVDSP_DEVICE_ENABLED);
	REQUIRE_FALSE(cdi_mono2::DRVDSP_HOST_RANGE_MAPPED);
}
