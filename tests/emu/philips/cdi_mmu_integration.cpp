// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// This file is included by cdi_dvc_dma_test_support.cpp after the shared
// full-machine test OSD/manager definitions from cdi_dvc_dma_integration.cpp.

namespace
{

class cdi_mmu_integration_state : public cdi_state
{
public:
	cdi_mmu_integration_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_mmu_integration(machine_config &config)
	{
		cdimono1(config);
	}

	bool completed() const { return m_completed; }
	std::vector<std::string> const &failures() const { return m_failures; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_mmu_integration_state::test_step), this);
	}

	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_failures.clear();
		m_completed = false;
		m_test_timer->adjust(attotime::zero);
	}

private:
	static constexpr uint32_t MMU_STATUS_CONTROL = 0x80008000U;
	static constexpr uint32_t MMU_DESC0_ATTR = 0x80008040U;
	static constexpr uint32_t MMU_DESC0_LENGTH = 0x80008042U;
	static constexpr uint32_t MMU_DESC0_SEGMENT = 0x80008044U;
	static constexpr uint32_t MMU_DESC0_BASE = 0x80008046U;
	static constexpr uint32_t LOGICAL = 0x001234U;
	static constexpr uint16_t ATTR_ERW = 0x3800U;
	static constexpr uint16_t LENGTH_MAX_MODE1 = 0x07ffU;
	static constexpr uint16_t VALID_SEGMENT_0 = 0x0080U;
	static constexpr uint16_t BASE_A = 0x1400U; // 0x500000 physical RAM
	static constexpr uint16_t BASE_B = 0x1500U; // 0x540000 physical RAM
	static constexpr uint32_t PHYSICAL_A = 0x501234U;
	static constexpr uint32_t PHYSICAL_B = 0x541234U;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	bool translate(int intention, uint32_t logical, uint32_t expected)
	{
		offs_t address = logical;
		address_space *target = nullptr;
		if (!m_maincpu->memory().translate(AS_PROGRAM, intention, address, target))
			return false;
		return target == &m_maincpu->space(AS_PROGRAM) && address == expected;
	}

	void program_mapping(address_space &space, uint16_t base)
	{
		// Program through the real supervisor-only internal register aperture.  The
		// descriptor maps logical segment 0 to Mono-I RAM and permits all CPU access
		// classes; MCR.EN is written last so no partially programmed map can become
		// visible to the translation hook.
		space.write_word(MMU_DESC0_ATTR, ATTR_ERW);
		space.write_word(MMU_DESC0_LENGTH, LENGTH_MAX_MODE1);
		space.write_word(MMU_DESC0_SEGMENT, VALID_SEGMENT_0);
		space.write_word(MMU_DESC0_BASE, base);
		space.write_word(MMU_STATUS_CONTROL, 0x0080);
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		address_space &space = m_maincpu->space(AS_PROGRAM);

		program_mapping(space, BASE_A);
		expect(translate(device_memory_interface::TR_FETCH, LOGICAL, PHYSICAL_A),
			"active: instruction translation did not use descriptor 0");
		expect(translate(device_memory_interface::TR_READ, LOGICAL, PHYSICAL_A),
			"active: data-read translation did not use descriptor 0");
		expect(translate(device_memory_interface::TR_WRITE, LOGICAL, PHYSICAL_A),
			"active: data-write translation did not use descriptor 0");

		// Capture a real MAME in-memory save state while the MMU is enabled.  The
		// translation engine has no hidden cache; control/status and descriptor RAM
		// are the complete persistent state and are registered by scc68070_device.
		ram_state snapshot(machine().save());
		expect(snapshot.save() == STATERR_NONE,
			"save: active-MMU ram_state capture failed");

		space.write_word(MMU_DESC0_BASE, BASE_B);
		expect(translate(device_memory_interface::TR_READ, LOGICAL, PHYSICAL_B),
			"mutate: live translation did not observe the changed descriptor base");
		space.write_word(MMU_STATUS_CONTROL, 0x0000);
		expect(translate(device_memory_interface::TR_READ, LOGICAL, LOGICAL),
			"mutate: disabling MCR.EN did not restore identity translation");

		expect(snapshot.load() == STATERR_NONE,
			"load: active-MMU ram_state restore failed");
		expect(translate(device_memory_interface::TR_FETCH, LOGICAL, PHYSICAL_A),
			"restore: instruction translation did not reconstruct saved mapping");
		expect(translate(device_memory_interface::TR_READ, LOGICAL, PHYSICAL_A),
			"restore: data-read translation did not reconstruct saved mapping");
		expect(translate(device_memory_interface::TR_WRITE, LOGICAL, PHYSICAL_A),
			"restore: data-write translation did not reconstruct saved mapping");

		expect(space.read_word(MMU_STATUS_CONTROL) == 0x0080,
			"restore: MMU status/control register did not round-trip");
		expect(space.read_word(MMU_DESC0_ATTR) == ATTR_ERW,
			"restore: descriptor attributes did not round-trip");
		expect(space.read_word(MMU_DESC0_LENGTH) == LENGTH_MAX_MODE1,
			"restore: descriptor length did not round-trip");
		expect((space.read_word(MMU_DESC0_SEGMENT) & 0x00ffU) == VALID_SEGMENT_0,
			"restore: descriptor segment/valid byte did not round-trip");
		expect(space.read_word(MMU_DESC0_BASE) == BASE_A,
			"restore: descriptor base did not round-trip");

		m_completed = true;
		machine().schedule_exit();
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

ROM_START(cdimmaint)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdimmaint, 0, cdi_mmu_integration, cdi_dma_integration,
	cdi_mmu_integration_state, empty_init, ROT0, "MAME",
	"CD-i SCC68070 MMU save-state integration fixture", 0)

void run_mmu_integration_fixture()
{
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdimmaint).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdimmaint), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_mmu_integration_state &>(machine.root_device());
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
	"SCC68070 active MMU translation survives a real MAME save-state round trip",
	"[emu][philips][cdi][scc68070][mmu][integration][save]")
{
	run_mmu_integration_fixture();
}
