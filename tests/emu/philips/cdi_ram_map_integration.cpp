// license:BSD-3-Clause
// copyright-holders:MAMEdev Team

// Board-level Mono-I memory-map regression.  This is deliberately separate from
// the SCC68070 MMU fixture: the MMU translates CPU logical addresses, while this
// test verifies what the physical CD-i board actually decodes after translation.

namespace
{

class cdi_ram_map_integration_state : public cdi_state
{
public:
	cdi_ram_map_integration_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_ram_map_integration(machine_config &config)
	{
		cdimono1(config);
	}

	bool completed() const { return m_completed; }
	std::vector<std::string> const &failures() const { return m_failures; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_ram_map_integration_state::test_step), this);
	}

	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_failures.clear();
		m_completed = false;

		address_space &space = m_maincpu->space(AS_PROGRAM);
		space.write_dword(0x000000, STACK_TOP);
		space.write_dword(0x000004, BOOT_PC);
		space.write_word(BOOT_PC, 0x60fe);
		m_test_timer->adjust(attotime::from_msec(1), 0);
	}

private:
	static constexpr uint32_t STACK_TOP = 0x070000U;
	static constexpr uint32_t BOOT_PC = 0x000800U;
	static constexpr uint32_t PROBE_CODE = 0x001000U;
	static constexpr uint32_t BUS_ERROR_HANDLER = 0x001100U;
	static constexpr uint32_t PHANTOM_RAM_START = 0x500000U;
	static constexpr uint32_t HANDLER_MARKER = 0x2aU;
	static constexpr uint32_t SUCCESS_MARKER = 0x11U;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	void start_probe(address_space &space, uint32_t address)
	{
		space.write_dword(0x000008, BUS_ERROR_HANDLER);
		space.write_word(BUS_ERROR_HANDLER + 0, 0x722a);
		space.write_word(BUS_ERROR_HANDLER + 2, 0x60fe);

		space.write_word(PROBE_CODE + 0, 0x3010);
		space.write_word(PROBE_CODE + 2, 0x7211);
		space.write_word(PROBE_CODE + 4, 0x60fe);

		m_maincpu->set_state_int(M68K_SR, 0x2700);
		m_maincpu->set_state_int(M68K_SP, STACK_TOP);
		m_maincpu->set_state_int(M68K_A0, address);
		m_maincpu->set_state_int(M68K_D0, 0);
		m_maincpu->set_state_int(M68K_D1, 0);
		m_maincpu->set_state_int(M68K_PC, PROBE_CODE);
		m_maincpu->resume(SUSPEND_REASON_DISABLE);
		m_test_timer->adjust(attotime::from_msec(1), 1);
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		address_space &space = m_maincpu->space(AS_PROGRAM);

		if (param == 0)
		{
			m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
			space.write_word(0x040000, 0x5a3c);
			expect(space.read_word(0x040000) == 0x5a3c,
				"base RAM: Mono-I plane RAM was not writable at 0x040000");
			start_probe(space, PHANTOM_RAM_START);
			return;
		}

		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		uint32_t const marker = uint32_t(m_maincpu->state_int(M68K_D1)) & 0xffU;
		expect(marker == HANDLER_MARKER,
			"base RAM hole: CPU read at 0x500000 did not take the external bus-error vector");
		expect(marker != SUCCESS_MARKER,
			"base RAM hole: CPU incorrectly completed a read from phantom 0x500000 RAM");

		m_completed = true;
		machine().schedule_exit();
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

ROM_START(cdiramap)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdiramap, 0, cdi_ram_map_integration, cdi_dma_integration,
	cdi_ram_map_integration_state, empty_init, ROT0, "MAME",
	"CD-i Mono-I physical RAM-map integration fixture", 0)

void run_ram_map_integration_fixture()
{
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdiramap).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdiramap), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_ram_map_integration_state &>(machine.root_device());
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

} // anonymous namespace

TEST_CASE(
	"CD-i Mono-I rejects the absent 0x500000 expansion RAM aperture",
	"[emu][philips][cdi][ram][memory-map][integration]")
{
	run_ram_map_integration_fixture();
}
