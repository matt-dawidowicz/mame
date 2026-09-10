// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "emu.h"
#include "cdi.h"

#include "catch.hpp"
#include "emuopts.h"
#include "main.h"
#include "osdepend.h"
#include "render.h"
#include "ui/menuitem.h"
#include "ui/uimain.h"

#include <memory>
#include <string>
#include <vector>

namespace
{

class cdi_cdic_dma_bounds_state : public cdi_state
{
public:
	cdi_cdic_dma_bounds_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_cdic_dma_bounds(machine_config &config) { cdimono1(config); }
	bool completed() const { return m_completed; }
	std::vector<std::string> const &failures() const { return m_failures; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_cdic_dma_bounds_state::test_step), this);
	}

	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_failures.clear();
		m_completed = false;
		m_test_timer->adjust(attotime::zero);
	}

private:
	static constexpr uint32_t DMA1_STATUS = 0x80004000U;
	static constexpr uint32_t DMA1_CONTROL = 0x80004004U;
	static constexpr uint32_t DMA1_SEQUENCE = 0x80004006U;
	static constexpr uint32_t DMA1_COUNTER = 0x8000400aU;
	static constexpr uint32_t DMA1_MAC_HI = 0x8000400cU;
	static constexpr uint32_t DMA1_MAC_LO = 0x8000400eU;
	static constexpr uint32_t CDIC_DMACTL = 0x00303ff8U;
	// DMA memory-side buffers must live in genuine Mono-I plane RAM.  The old
	// 0x500xxx locations were the phantom expansion aperture guarded by the
	// board-level RAM-map regression.
	static constexpr uint32_t SOURCE = 0x00040100U;
	static constexpr uint32_t DESTINATION = 0x00040200U;
	static constexpr uint16_t WORDS = 3;
	static constexpr uint16_t DMACTL_LAST_TWO_WORDS = 0x3ffc;
	static constexpr uint16_t DEVICE_BUS_ERROR = 0x000a;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	void expect_remaining(uint16_t expected, char const *where)
	{
		uint16_t const actual = m_maincpu->dma_channel_remaining(0);
		if (actual != expected)
			m_failures.push_back(util::string_format("%s: DMA1 remaining expected %u, got %u", where, expected, actual));
	}

	void expect_address(uint32_t expected, char const *where)
	{
		uint32_t const actual = m_maincpu->dma_channel_memory_address(0);
		if (actual != expected)
			m_failures.push_back(util::string_format("%s: DMA1 MAC expected %08x, got %08x", where, expected, actual));
	}

	void program_dma(address_space &space, bool device_to_memory, uint32_t memory_address)
	{
		space.write_word(DMA1_STATUS, 0xb000);
		space.write_word(DMA1_CONTROL, device_to_memory ? 0x3090 : 0x3010);
		space.write_word(DMA1_SEQUENCE, 0x0400);
		space.write_word(DMA1_COUNTER, WORDS);
		space.write_word(DMA1_MAC_HI, uint16_t(memory_address >> 16));
		space.write_word(DMA1_MAC_LO, uint16_t(memory_address));

		expect(m_maincpu->dma_channel_memory_to_device(0) == !device_to_memory,
				device_to_memory ? "program D2M: wrong direction" : "program M2D: wrong direction");
		expect(m_maincpu->dma_channel_word_transfer(0), "program: DMA1 is not a word transfer");
		bool increment = false;
		expect(m_maincpu->dma_channel_memory_increment(0, increment) && increment,
				"program: DMA1 MAC increment mode was not accepted");
		expect_remaining(WORDS, "program");
		expect_address(memory_address, "program");
		space.write_word(DMA1_SEQUENCE, 0x0480);
		expect(m_maincpu->dma_channel_active(0), "start: SCC START did not assert DMA1 CA");
	}

	void expect_boundary_error(address_space &space, uint32_t expected_memory_address, char const *where)
	{
		uint16_t const status = space.read_word(DMA1_STATUS);
		expect((status & 0x9000U) == 0x9000U,
				util::string_format("%s: DMA1 did not report COC+ERR (%04x)", where, status));
		expect(!(status & 0x0800U),
				util::string_format("%s: DMA1 CA remained active (%04x)", where, status));
		expect((status & 0x00ffU) == DEVICE_BUS_ERROR,
				util::string_format("%s: expected device bus error 0a, got %02x", where, status & 0xff));
		expect_remaining(1, where);
		expect_address(expected_memory_address, where);
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		address_space &space = m_maincpu->space(AS_PROGRAM);
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);

		space.write_word(SOURCE + 0, 0x1234);
		space.write_word(SOURCE + 2, 0xabcd);
		space.write_word(SOURCE + 4, 0x55aa);
		space.write_word(DESTINATION + 0, 0xdead);
		space.write_word(DESTINATION + 2, 0xbeef);
		space.write_word(DESTINATION + 4, 0xcafe);

		// $3ffc and $3ffe are valid SRAM words; requested word 3 starts at $4000.
		program_dma(space, false, SOURCE);
		space.write_word(CDIC_DMACTL, DMACTL_LAST_TWO_WORDS);
		expect_boundary_error(space, SOURCE + 4, "memory-to-device");

		program_dma(space, true, DESTINATION);
		space.write_word(CDIC_DMACTL, DMACTL_LAST_TWO_WORDS);
		expect_boundary_error(space, DESTINATION + 4, "device-to-memory");

		expect(space.read_word(DESTINATION + 0) == 0x1234, "D2M: first final SRAM word did not round-trip");
		expect(space.read_word(DESTINATION + 2) == 0xabcd, "D2M: last SRAM word did not round-trip");
		expect(space.read_word(DESTINATION + 4) == 0xcafe, "D2M: out-of-range operand overwrote sentinel");

		m_completed = true;
		machine().schedule_exit();
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

static INPUT_PORTS_START(cdi_cdic_dma_bounds)
	PORT_START("MOUSEX")
	PORT_BIT(0xffff, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_START("MOUSEY")
	PORT_BIT(0xffff, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_START("MOUSEBTN")
	PORT_BIT(0xffff, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_START("TESTPLUG")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END

ROM_START(cdicdmab)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdicdmab, 0, cdi_cdic_dma_bounds, cdi_cdic_dma_bounds,
	cdi_cdic_dma_bounds_state, empty_init, ROT0, "MAME",
	"CD-i CDIC DMA SRAM bounds integration fixture", 0)

} // anonymous namespace

TEST_CASE(
	"CDIC DMA bounds final SRAM words and faults oversized transfers in both directions",
	"[emu][philips][cdi][cdic][dma][integration]")
{
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdicdmab).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdicdmab), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_cdic_dma_bounds_state &>(machine.root_device());
	manager.set_machine(nullptr);

	std::string diagnostics;
	for (std::string const &failure : state.failures())
	{
		if (!diagnostics.empty()) diagnostics.append("\n");
		diagnostics.append(failure);
	}

	INFO(diagnostics);
	REQUIRE(error == EMU_ERR_NONE);
	REQUIRE(state.completed());
	REQUIRE(state.failures().empty());
}