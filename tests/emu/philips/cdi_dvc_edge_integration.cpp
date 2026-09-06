// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// This file is included by cdi_dvc_dma_test_support.cpp after the shared
// full-machine test OSD/manager definitions from cdi_dvc_dma_integration.cpp.

namespace
{

class cdi_dma_edge_state : public cdi_state
{
public:
	cdi_dma_edge_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_dma_edges(machine_config &config)
	{
		cdimono1dvc(config);
	}

	bool completed() const { return m_completed; }
	std::vector<std::string> const &failures() const { return m_failures; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_dma_edge_state::test_step), this);
	}

	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_failures.clear();
		m_completed = false;
		m_test_timer->adjust(attotime::zero, 0);
	}

private:
	static constexpr uint32_t DMA2_STATUS = 0x80004040U;
	static constexpr uint32_t DMA2_CONTROL = 0x80004044U;
	static constexpr uint32_t DMA2_SEQUENCE = 0x80004046U;
	static constexpr uint32_t DMA2_COUNTER = 0x8000404aU;
	static constexpr uint32_t DMA2_MAC_HI = 0x8000404cU;
	static constexpr uint32_t DMA2_MAC_LO = 0x8000404eU;
	static constexpr uint32_t DVC_FMA_COMMAND = 0x00e03000U;
	static constexpr uint32_t SOURCE = 0x00d00200U;
	static constexpr uint8_t DMA_IRQ_LEVEL = 3;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	void expect_remaining(uint16_t expected, char const *where)
	{
		uint16_t const actual = m_maincpu->dma_channel_remaining(1);
		if (actual != expected)
			m_failures.push_back(util::string_format(
				"%s: DMA2 remaining expected %u, got %u", where, expected, actual));
	}

	void expect_address(uint32_t expected, char const *where)
	{
		uint32_t const actual = m_maincpu->dma_channel_memory_address(1);
		if (actual != expected)
			m_failures.push_back(util::string_format(
				"%s: DMA2 MAC expected %08x, got %08x", where, expected, actual));
	}

	void expect_events(uint32_t expected, char const *where)
	{
		if (m_dvc_dma_service_events != expected)
			m_failures.push_back(util::string_format(
				"%s: DVC service events expected %u, got %u",
				where, expected, m_dvc_dma_service_events));
	}

	bool fma_dma_requested(address_space &space)
	{
		return bool(space.read_word(DVC_FMA_COMMAND) & 0x8000U);
	}

	void program_dma(address_space &space, uint16_t count, uint32_t address)
	{
		space.write_word(DMA2_CONTROL, 0x3010); // M2D, word operand
		space.write_word(DMA2_SEQUENCE, 0x040b); // increment MAC, INE, IPL3
		space.write_word(DMA2_COUNTER, count);
		space.write_word(DMA2_MAC_HI, uint16_t(address >> 16));
		space.write_word(DMA2_MAC_LO, uint16_t(address));
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		address_space &space = m_maincpu->space(AS_PROGRAM);

		switch (param)
		{
		case 0:
		{
			m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
			space.write_word(SOURCE + 0, 0x1111);
			space.write_word(SOURCE + 2, 0x2222);
			space.write_word(SOURCE + 4, 0x3333);
			space.write_word(SOURCE + 6, 0x4444);

			// A zero transfer count is an unresolved SCC68070 hardware edge.  The
			// current compatibility model must be deterministic: DVC may request,
			// but the driver must not arm SCC DMA or fabricate a completion.
			program_dma(space, 0, SOURCE);
			space.write_word(DVC_FMA_COMMAND, 0x8000);
			expect(fma_dma_requested(space), "zero: DVC request bit did not latch");
			expect(!m_dvc_dma_service_active, "zero: driver armed a zero-count DMA");
			expect(!m_maincpu->dma_channel_active(1), "zero: SCC CA asserted");
			expect_remaining(0, "zero");
			expect_address(SOURCE, "zero");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == CLEAR_LINE,
				"zero: completion IRQ was fabricated");
			m_test_timer->adjust(attotime::from_ticks(3, m_maincpu->clock()), 1);
			break;
		}

		case 1:
		{
			expect(!m_dvc_dma_service_active, "zero-wait: service became active");
			expect_remaining(0, "zero-wait");
			expect_address(SOURCE, "zero-wait");
			expect(fma_dma_requested(space),
				"zero-wait: DVC request cleared without a completed transfer");

			// Reprogramming a non-zero count and restrobing the DVC request must
			// recover cleanly from the refused zero-count request.
			program_dma(space, 3, SOURCE);
			space.write_word(DVC_FMA_COMMAND, 0x8000);
			expect(m_dvc_dma_service_active, "restart: service did not arm");
			expect(m_maincpu->dma_channel_active(1), "restart: SCC CA did not assert");
			expect_events(0, "restart");
			m_test_timer->adjust(attotime::from_ticks(1, m_maincpu->clock()), 2);
			break;
		}

		case 2:
		{
			// First word has crossed the real SCC->DVC boundary.  Soft-abort the
			// SCC operation before the second production service tick.
			expect_events(1, "partial");
			expect_remaining(2, "partial");
			expect_address(SOURCE + 2, "partial");
			expect(fma_dma_requested(space),
				"partial: DVC request cleared before DMA completion");
			space.write_word(DMA2_SEQUENCE, 0x041b); // SA + INE + IPL3
			expect(!m_maincpu->dma_channel_active(1),
				"abort: SCC CA remained active after software abort");
			expect(m_dvc_dma_service_active,
				"abort: driver service disappeared before observing SCC abort");
			uint16_t const status = space.read_word(DMA2_STATUS);
			expect((status & 0x9000U) == 0x9000U,
				"abort: SCC did not report COC+ERR");

			// CPU input lines are scheduler-synchronized.  The SCC state above is
			// already committed, but the visible IPL transition is checked in the
			// next test state after synchronization rather than in this callback.
			m_test_timer->adjust(attotime::from_ticks(2, m_maincpu->clock()), 3);
			break;
		}

		case 3:
		{
			// The scheduled service callback must observe the SCC abort without
			// consuming another word or falsely calling DVC dma_done().
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == ASSERT_LINE,
				"abort: COC+ERR with INE did not assert IPL3 after synchronization");
			expect(!m_dvc_dma_service_active,
				"abort-observe: driver service remained armed");
			expect_events(1, "abort-observe");
			expect_remaining(2, "abort-observe");
			expect_address(SOURCE + 2, "abort-observe");
			expect(fma_dma_requested(space),
				"abort-observe: DVC request cleared as if transfer completed");

			// Clear SCC completion/error status.  As with assertion, the CPU input
			// line deassertion is scheduler-synchronized and is checked one tick
			// later before the channel is reprogrammed.
			space.write_word(DMA2_STATUS, 0x9000);
			m_test_timer->adjust(attotime::from_ticks(1, m_maincpu->clock()), 4);
			break;
		}

		case 4:
		{
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == CLEAR_LINE,
				"abort-ack: IPL3 remained asserted after synchronization");

			// Restart exactly at the first untransferred word.  This pins
			// partial-transfer conservation independently of interrupt delivery.
			program_dma(space, 2, SOURCE + 2);
			space.write_word(DVC_FMA_COMMAND, 0x8000);
			expect(m_dvc_dma_service_active, "resume: service did not re-arm");
			expect_events(0, "resume");
			m_test_timer->adjust(attotime::from_ticks(1, m_maincpu->clock()), 5);
			break;
		}

		case 5:
			expect_events(1, "resume-word1");
			expect_remaining(1, "resume-word1");
			expect_address(SOURCE + 4, "resume-word1");
			expect(fma_dma_requested(space),
				"resume-word1: DVC request cleared before final word");
			m_test_timer->adjust(attotime::from_ticks(2, m_maincpu->clock()), 6);
			break;

		case 6:
		{
			expect_events(2, "resume-complete");
			expect_remaining(0, "resume-complete");
			expect_address(SOURCE + 6, "resume-complete");
			expect(!m_maincpu->dma_channel_active(1),
				"resume-complete: SCC CA remained active");
			expect(!m_dvc_dma_service_active,
				"resume-complete: service remained active");
			expect(!fma_dma_requested(space),
				"resume-complete: DVC request did not clear on exact final word");
			uint16_t const status = space.read_word(DMA2_STATUS);
			expect(bool(status & 0x8000U), "resume-complete: COC missing");
			expect(!(status & 0x0800U), "resume-complete: CA still visible");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == ASSERT_LINE,
				"resume-complete: completion IRQ missing");
			space.write_word(DMA2_STATUS, 0x8000);

			m_completed = true;
			machine().schedule_exit();
			break;
		}
		}
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

class cdi_dvc_presence_state : public cdi_state
{
public:
	cdi_dvc_presence_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_without_dvc(machine_config &config) { cdimono1(config); }
	void cdi_with_dvc(machine_config &config) { cdimono1dvc(config); }
	bool dvc_present() const { return bool(m_dvc); }
	bool completed() const { return m_completed; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_dvc_presence_state::finish), this);
	}

	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_completed = false;
		m_test_timer->adjust(attotime::zero);
	}

private:
	TIMER_CALLBACK_MEMBER(finish)
	{
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		m_completed = true;
		machine().schedule_exit();
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
};

ROM_START(cdidmaedge)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

ROM_START(cdinodvct)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

ROM_START(cdihasdvct)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdidmaedge, 0, cdi_dma_edges, cdi_dma_integration,
	cdi_dma_edge_state, empty_init, ROT0, "MAME",
	"CD-i DVC DMA edge integration fixture", 0)
GAME(2026, cdinodvct, 0, cdi_without_dvc, cdi_dma_integration,
	cdi_dvc_presence_state, empty_init, ROT0, "MAME",
	"CD-i without Digital Video Cartridge fixture", 0)
GAME(2026, cdihasdvct, 0, cdi_with_dvc, cdi_dma_integration,
	cdi_dvc_presence_state, empty_init, ROT0, "MAME",
	"CD-i with Digital Video Cartridge fixture", 0)

void run_dma_edge_fixture()
{
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdidmaedge).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdidmaedge), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_dma_edge_state &>(machine.root_device());
	manager.set_machine(nullptr);

	std::string diagnostics;
	for (std::string const &failure : state.failures())
	{
		if (!diagnostics.empty())
			diagnostics.append("\n");
		diagnostics.append(failure);
	}

	INFO(diagnostics);
	REQUIRE(error == EMU_ERR_NONE);
	REQUIRE(state.completed());
	REQUIRE(state.failures().empty());
}

bool run_presence_fixture(game_driver const &driver)
{
	emu_options options;
	options.set_system_name(std::string(driver.name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(driver, options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_dvc_presence_state &>(machine.root_device());
	bool const present = state.dvc_present();
	bool const completed = state.completed();
	manager.set_machine(nullptr);

	REQUIRE(error == EMU_ERR_NONE);
	REQUIRE(completed);
	return present;
}

} // anonymous namespace

TEST_CASE(
	"CD-i DVC DMA zero-count abort partial and restart edges stay deterministic",
	"[emu][philips][cdi][dvc][dma][integration][edge]")
{
	run_dma_edge_fixture();
}

TEST_CASE(
	"CD-i machine configuration represents the Digital Video Cartridge as truly optional",
	"[emu][philips][cdi][dvc][configuration][integration]")
{
	REQUIRE_FALSE(run_presence_fixture(GAME_NAME(cdinodvct)));
	REQUIRE(run_presence_fixture(GAME_NAME(cdihasdvct)));
}
