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
		m_phase = phase::initial;

		// Give the SCC68070 a valid reset vector and a harmless bootstrap loop so
		// the CPU completes its real reset sequence before the host starts testing.
		address_space &space = m_maincpu->space(AS_PROGRAM);
		space.write_dword(0x000000, STACK_TOP);
		space.write_dword(0x000004, BOOT_PC);
		space.write_word(BOOT_PC, 0x60fe); // bra.s BOOT_PC
		m_test_timer->adjust(attotime::from_msec(1));
	}

private:
	enum class phase
	{
		initial,
		read_frame,
		read_rte,
		write_result,
		boundary_result,
		fetch_result
	};

	static constexpr uint32_t MMU_STATUS_CONTROL = 0x80008000U;
	static constexpr uint32_t MMU_DESC0_ATTR = 0x80008040U;
	static constexpr uint32_t MMU_DESC0_LENGTH = 0x80008042U;
	static constexpr uint32_t MMU_DESC0_SEGMENT = 0x80008044U;
	static constexpr uint32_t MMU_DESC0_BASE = 0x80008046U;
	static constexpr uint32_t MMU_DESC1_ATTR = 0x80008048U;
	static constexpr uint32_t MMU_DESC1_LENGTH = 0x8000804aU;
	static constexpr uint32_t MMU_DESC1_SEGMENT = 0x8000804cU;
	static constexpr uint32_t MMU_DESC1_BASE = 0x8000804eU;

	static constexpr uint16_t ATTR_EXECUTE = 0x2000U;
	static constexpr uint16_t ATTR_READ = 0x1000U;
	static constexpr uint16_t ATTR_WRITE = 0x0800U;
	static constexpr uint16_t ATTR_ERW = ATTR_EXECUTE | ATTR_READ | ATTR_WRITE;
	static constexpr uint16_t LENGTH_MAX_MODE1 = 0x07ffU;
	static constexpr uint16_t VALID_SEGMENT_0 = 0x0080U;
	static constexpr uint16_t VALID_SEGMENT_1 = 0x0081U;
	static constexpr uint16_t BASE_IDENTITY = 0x0000U;
	static constexpr uint16_t BASE_RAM = 0x0100U; // 0x040000 physical plane RAM

	static constexpr uint32_t LOGICAL_QUERY = 0x001234U;
	static constexpr uint16_t BASE_A = 0x0040U;
	static constexpr uint16_t BASE_B = 0x0080U;
	static constexpr uint32_t PHYSICAL_A = 0x011234U;
	static constexpr uint32_t PHYSICAL_B = 0x021234U;

	static constexpr uint32_t BOOT_PC = 0x000800U;
	static constexpr uint32_t STACK_TOP = 0x070000U;
	static constexpr uint32_t FRAME_BYTES = 34U;
	static constexpr uint32_t HANDLER_SPIN = 0x002000U;
	static constexpr uint32_t RTE_STUB = 0x002100U;
	static constexpr uint32_t WRITE_HANDLER = 0x002200U;
	static constexpr uint32_t BOUNDARY_HANDLER = 0x002300U;
	static constexpr uint32_t FETCH_HANDLER = 0x002400U;
	static constexpr uint32_t READ_CODE = 0x001000U;
	static constexpr uint32_t WRITE_CODE = 0x001100U;
	static constexpr uint32_t BOUNDARY_CODE = 0x001200U;
	static constexpr uint32_t FETCH_CODE = 0x001300U;
	static constexpr uint32_t LOGICAL_DATA = 0x200100U;
	static constexpr uint32_t LOGICAL_WRITE = 0x200120U;
	static constexpr uint32_t LOGICAL_BOUNDARY = 0x2003feU;
	static constexpr uint32_t LOGICAL_FETCH = 0x200000U;
	static constexpr uint32_t PHYSICAL_DATA = 0x040100U;
	static constexpr uint32_t PHYSICAL_WRITE = 0x040120U;
	static constexpr uint32_t PHYSICAL_BOUNDARY = 0x0403feU;
	static constexpr uint32_t PHYSICAL_FETCH = 0x040000U;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	uint32_t cpu_state(int index) const
	{
		return uint32_t(m_maincpu->state_int(index));
	}

	bool translate(int intention, uint32_t logical, uint32_t expected)
	{
		offs_t address = logical;
		address_space *target = nullptr;
		device_memory_interface &memory = *m_maincpu;
		if (!memory.translate(AS_PROGRAM, intention, address, target))
			return false;
		return target == &m_maincpu->space(AS_PROGRAM) && address == expected;
	}

	void program_query_mapping(address_space &space, uint16_t base)
	{
		space.write_word(MMU_DESC0_ATTR, ATTR_ERW);
		space.write_word(MMU_DESC0_LENGTH, LENGTH_MAX_MODE1);
		space.write_word(MMU_DESC0_SEGMENT, VALID_SEGMENT_0);
		space.write_word(MMU_DESC0_BASE, base);
		space.write_word(MMU_STATUS_CONTROL, 0x0080);
	}

	void program_executed_mapping(address_space &space, uint16_t attr1, uint16_t length1)
	{
		// Disable translation while the host rewrites the complete descriptor set.
		space.write_word(MMU_STATUS_CONTROL, 0x0000);
		space.write_word(MMU_DESC0_ATTR, ATTR_ERW);
		space.write_word(MMU_DESC0_LENGTH, LENGTH_MAX_MODE1);
		space.write_word(MMU_DESC0_SEGMENT, VALID_SEGMENT_0);
		space.write_word(MMU_DESC0_BASE, BASE_IDENTITY);
		space.write_word(MMU_DESC1_ATTR, attr1);
		space.write_word(MMU_DESC1_LENGTH, length1);
		space.write_word(MMU_DESC1_SEGMENT, VALID_SEGMENT_1);
		space.write_word(MMU_DESC1_BASE, BASE_RAM);
		space.write_word(MMU_STATUS_CONTROL, 0x0080);
	}

	void install_vector(address_space &space, uint32_t handler)
	{
		space.write_dword(0x000008, handler); // vector 2: bus error
	}

	void install_repair_handler(address_space &space, uint32_t pc, uint16_t value, uint32_t reg)
	{
		// move.w #value,(reg).l ; rte
		space.write_word(pc + 0, 0x33fc);
		space.write_word(pc + 2, value);
		space.write_word(pc + 4, uint16_t(reg >> 16));
		space.write_word(pc + 6, uint16_t(reg));
		space.write_word(pc + 8, 0x4e73);
	}

	void set_cpu(uint32_t pc, uint32_t a0, uint32_t d0)
	{
		m_maincpu->set_state_int(M68K_SR, 0x2700);
		m_maincpu->set_state_int(M68K_SP, STACK_TOP);
		m_maincpu->set_state_int(M68K_A0, a0);
		m_maincpu->set_state_int(M68K_D0, d0);
		m_maincpu->set_state_int(M68K_PC, pc);
	}

	void run_phase(phase next)
	{
		m_phase = next;
		m_maincpu->resume(SUSPEND_REASON_DISABLE);
		m_test_timer->adjust(attotime::from_msec(1));
	}

	void start_read_fault(address_space &space)
	{
		program_executed_mapping(space, ATTR_WRITE, LENGTH_MAX_MODE1);
		space.write_word(PHYSICAL_DATA, 0x5a3c);
		space.write_word(READ_CODE + 0, 0x3010); // move.w (a0),d0
		space.write_word(READ_CODE + 2, 0x60fe); // success loop
		space.write_word(HANDLER_SPIN, 0x60fe);  // preserve frame for host inspection
		space.write_word(RTE_STUB, 0x4e73);
		install_vector(space, HANDLER_SPIN);
		set_cpu(READ_CODE, LOGICAL_DATA, 0xdead0000U);
		run_phase(phase::read_frame);
	}

	void start_write_fault(address_space &space)
	{
		program_executed_mapping(space, ATTR_READ, LENGTH_MAX_MODE1);
		space.write_word(PHYSICAL_WRITE, 0x1234);
		space.write_word(WRITE_CODE + 0, 0x3080); // move.w d0,(a0)
		space.write_word(WRITE_CODE + 2, 0x60fe);
		install_repair_handler(space, WRITE_HANDLER, ATTR_ERW, MMU_DESC1_ATTR);
		install_vector(space, WRITE_HANDLER);
		set_cpu(WRITE_CODE, LOGICAL_WRITE, 0x0000beefU);
		run_phase(phase::write_result);
	}

	void start_boundary_fault(address_space &space)
	{
		program_executed_mapping(space, ATTR_ERW, 0x0000);
		for (unsigned byte = 0; byte < 4; ++byte)
			space.write_byte(PHYSICAL_BOUNDARY + byte, 0xa0 + byte);
		space.write_word(BOUNDARY_CODE + 0, 0x2080); // move.l d0,(a0)
		space.write_word(BOUNDARY_CODE + 2, 0x60fe);
		install_repair_handler(space, BOUNDARY_HANDLER, 0x0001, MMU_DESC1_LENGTH);
		install_vector(space, BOUNDARY_HANDLER);
		set_cpu(BOUNDARY_CODE, LOGICAL_BOUNDARY, 0x11223344U);
		run_phase(phase::boundary_result);
	}

	void start_fetch_fault(address_space &space)
	{
		program_executed_mapping(space, ATTR_READ | ATTR_WRITE, LENGTH_MAX_MODE1);
		space.write_word(FETCH_CODE + 0, 0x4ed0); // jmp (a0)
		space.write_word(FETCH_CODE + 2, 0x60fe);
		space.write_word(PHYSICAL_FETCH + 0, 0x702a); // moveq #42,d0
		space.write_word(PHYSICAL_FETCH + 2, 0x60fe);
		install_repair_handler(space, FETCH_HANDLER, ATTR_ERW, MMU_DESC1_ATTR);
		install_vector(space, FETCH_HANDLER);
		set_cpu(FETCH_CODE, LOGICAL_FETCH, 0);
		run_phase(phase::fetch_result);
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		address_space &space = m_maincpu->space(AS_PROGRAM);

		switch (m_phase)
		{
		case phase::initial:
		{
			// Retain the existing query/save-state coverage, but now do it after a
			// real CPU reset has completed.  The executed corpus below runs after
			// this active-MMU snapshot is restored, covering postload ownership too.
			program_query_mapping(space, BASE_A);
			expect(translate(device_memory_interface::TR_FETCH, LOGICAL_QUERY, PHYSICAL_A),
				"active: instruction translation did not use descriptor 0");
			expect(translate(device_memory_interface::TR_READ, LOGICAL_QUERY, PHYSICAL_A),
				"active: data-read translation did not use descriptor 0");
			expect(translate(device_memory_interface::TR_WRITE, LOGICAL_QUERY, PHYSICAL_A),
				"active: data-write translation did not use descriptor 0");

			ram_state snapshot(machine().save());
			expect(snapshot.save() == STATERR_NONE,
				"save: active-MMU ram_state capture failed");
			space.write_word(MMU_DESC0_BASE, BASE_B);
			expect(translate(device_memory_interface::TR_READ, LOGICAL_QUERY, PHYSICAL_B),
				"mutate: live translation did not observe the changed descriptor base");
			space.write_word(MMU_STATUS_CONTROL, 0x0000);
			expect(translate(device_memory_interface::TR_READ, LOGICAL_QUERY, LOGICAL_QUERY),
				"mutate: disabling MCR.EN did not restore identity translation");
			expect(snapshot.load() == STATERR_NONE,
				"load: active-MMU ram_state restore failed");
			expect(translate(device_memory_interface::TR_FETCH, LOGICAL_QUERY, PHYSICAL_A),
				"restore: instruction translation did not reconstruct saved mapping");
			expect(translate(device_memory_interface::TR_READ, LOGICAL_QUERY, PHYSICAL_A),
				"restore: data-read translation did not reconstruct saved mapping");
			expect(translate(device_memory_interface::TR_WRITE, LOGICAL_QUERY, PHYSICAL_A),
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

			start_read_fault(space);
			break;
		}

		case phase::read_frame:
		{
			uint32_t const sp = cpu_state(M68K_SP);
			expect(sp == STACK_TOP - FRAME_BYTES,
				"read fault: SCC68070 did not build a 17-word exception frame");
			expect(space.read_word(sp + 6) == 0xf008,
				"read fault: format/vector word is not SCC68070 format F / vector 2");
			expect(space.read_word(sp + 8) == 0x1125,
				"read fault: SSW does not report DF/RW/BM/supervisor-data FC with RR clear");
			expect(space.read_dword(sp + 20) == LOGICAL_DATA,
				"read fault: stacked logical fault address is wrong");
			expect((space.read_word(MMU_STATUS_CONTROL) >> 8) & 0x10,
				"read fault: MSR did not report an access violation");
			expect((cpu_state(M68K_D0) & 0xffffU) == 0x0000,
				"read fault: D0 changed before the denied instruction was retried");

			// Repair the descriptor externally while preserving the real frame, then
			// execute an actual RTE.  Success requires format-F consumption, stack
			// restoration, return to PPC, and rerun of the original MOVE.W.
			space.write_word(MMU_DESC1_ATTR, ATTR_ERW);
			// Keep exception control flow entirely guest-driven: replace the handler's
			// spin instruction with RTE and resume from the vector target, rather than
			// externally rewriting the CPU PC while it owns a live exception frame.
			space.write_word(HANDLER_SPIN, 0x4e73);
			run_phase(phase::read_rte);
			break;
		}

		case phase::read_rte:
			expect((cpu_state(M68K_D0) & 0xffffU) == 0x5a3c,
				string_format("read RTE: denied MOVE.W did not rerun after descriptor repair (PC=%08x SP=%08x D0=%08x)",
					cpu_state(M68K_PC), cpu_state(M68K_SP), cpu_state(M68K_D0)));
			expect(cpu_state(M68K_SP) == STACK_TOP,
				string_format("read RTE: format-F frame was not fully removed from the supervisor stack (PC=%08x SP=%08x)",
					cpu_state(M68K_PC), cpu_state(M68K_SP)));
			start_write_fault(space);
			break;

		case phase::write_result:
			expect(space.read_word(PHYSICAL_WRITE) == 0xbeef,
				"write fault: protected MOVE.W did not complete after handler repair/RTE");
			expect(space.read_word(MMU_DESC1_ATTR) == ATTR_ERW,
				"write fault: exception handler did not execute its MMU repair");
			expect((space.read_word(MMU_STATUS_CONTROL) >> 8) & 0x10,
				"write fault: MSR did not report an access violation");
			expect(cpu_state(M68K_SP) == STACK_TOP,
				"write fault: RTE did not restore the supervisor stack");
			start_boundary_fault(space);
			break;

		case phase::boundary_result:
			expect(space.read_byte(PHYSICAL_BOUNDARY + 0) == 0x11
				&& space.read_byte(PHYSICAL_BOUNDARY + 1) == 0x22
				&& space.read_byte(PHYSICAL_BOUNDARY + 2) == 0x33
				&& space.read_byte(PHYSICAL_BOUNDARY + 3) == 0x44,
				"boundary fault: cross-block MOVE.L did not rerun as one complete operand");
			expect(space.read_word(MMU_DESC1_LENGTH) == 0x0001,
				"boundary fault: handler did not extend the segment before RTE");
			expect((space.read_word(MMU_STATUS_CONTROL) >> 8) & 0x20,
				"boundary fault: MSR did not report the length violation");
			expect(cpu_state(M68K_SP) == STACK_TOP,
				"boundary fault: RTE did not restore the supervisor stack");
			start_fetch_fault(space);
			break;

		case phase::fetch_result:
			expect((cpu_state(M68K_D0) & 0xffU) == 42,
				"fetch fault: protected target opcode did not execute after handler repair/RTE");
			expect(space.read_word(MMU_DESC1_ATTR) == ATTR_ERW,
				"fetch fault: exception handler did not enable execute permission");
			expect((space.read_word(MMU_STATUS_CONTROL) >> 8) & 0x10,
				"fetch fault: MSR did not report an access violation");
			expect(cpu_state(M68K_SP) == STACK_TOP,
				"fetch fault: RTE did not restore the supervisor stack");

			m_completed = true;
			machine().schedule_exit();
			break;
		}
	}

	emu_timer *m_test_timer = nullptr;
	phase m_phase = phase::initial;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

ROM_START(cdimmaint)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdimmaint, 0, cdi_mmu_integration, cdi_dma_integration,
	cdi_mmu_integration_state, empty_init, ROT0, "MAME",
	"CD-i SCC68070 executed MMU exception integration fixture", 0)

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
	"SCC68070 executes restartable MMU fetch/read/write/boundary faults through format-F RTE",
	"[emu][philips][cdi][scc68070][mmu][integration][exception][rte][save]")
{
	run_mmu_integration_fixture();
}
