// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * An emulation of the Motorola DSP56000/DSP56001.
 *
 * Sources:
 *   - http://www.bitsavers.org/components/motorola/56000/1990_DSP56000_DSP56001_Users_Manual.pdf
 *
 * STATUS:
 *   - standard host interface and bootstrap transport implemented
 *   - partial interpreter executes the current bootstrap relocation subset
 *
 * TODO:
 *   - complete the architectural register file, AGU/ALU, parallel moves,
 *     interrupts, and peripherals
 *   - replace bring-up P/X/Y backing with the device address spaces
 *   - implement instruction-accurate cycle timing
 */

#include "emu.h"
#include "dsp56000.h"
#include "dsp56000d.h"
#include "dsp56000execute.h"

//#define VERBOSE (LOG_GENERAL)

#include "logmacro.h"

DEFINE_DEVICE_TYPE(DSP56000, dsp56000_device, "dsp56000", "Motorola DSP56000")
DEFINE_DEVICE_TYPE(DSP56001, dsp56001_device, "dsp56001", "Motorola DSP56001")

dsp56000_device_base::dsp56000_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock)
	: cpu_device(mconfig, type, tag, owner, clock)
	, m_p_config("p", ENDIANNESS_BIG, 32, 16, -2)
	, m_x_config("x", ENDIANNESS_BIG, 32, 16, -2)
	, m_y_config("y", ENDIANNESS_BIG, 32, 16, -2)
	, m_icount(0)
{
}

dsp56000_device::dsp56000_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: dsp56000_device_base(mconfig, DSP56000, tag, owner, clock)
{
}

dsp56001_device::dsp56001_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: dsp56000_device_base(mconfig, DSP56001, tag, owner, clock)
{
}

void dsp56000_device_base::device_start()
{
	// program-visible cpu state
	save_item(NAME(m_pc));
	save_item(NAME(m_current_opcode));
	save_item(NAME(m_execution_stopped));

	save_item(NAME(m_core.r));
	save_item(NAME(m_core.x0));
	save_item(NAME(m_core.la));
	save_item(NAME(m_core.lc));
	save_item(NAME(m_core.loop_start));
	save_item(NAME(m_core.loop_active));

	save_item(NAME(m_program));
	save_item(NAME(m_x_peripheral));
	save_item(NAME(m_y_peripheral));
	save_item(NAME(m_program_bootstrap_loaded));

	// Standard DSP56000/56001 host interface and bootstrap state.
	save_item(NAME(m_host.m_hostport));
	save_item(NAME(m_host.m_tx));
	save_item(NAME(m_host.m_rx));
	save_item(NAME(m_host.m_bootstrap));
	save_item(NAME(m_host.m_hsr));
	save_item(NAME(m_host.m_dsp_host_rtx));
	save_item(NAME(m_host.m_dsp_host_htx));
	save_item(NAME(m_host.m_bootstrap_pos));
	save_item(NAME(m_host.m_running));

	state_add(STATE_GENPC, "GENPC", m_pc).noshow();
	state_add(STATE_GENPCBASE, "CURPC", m_pc).noshow();

	set_icountptr(m_icount);
}

void dsp56000_device_base::device_reset()
{
	m_pc = 0;
	m_current_opcode = 0;
	m_execution_stopped = false;
	m_program_bootstrap_loaded = false;
	m_core = {};

	for (u32 &word : m_program)
		word = 0;

	for (u32 &word : m_x_peripheral)
		word = 0;

	for (u32 &word : m_y_peripheral)
		word = 0;

	m_host.reset();
}

void dsp56000_device_base::execute_run()
{
	while (m_icount > 0)
	{
		if (!m_host.running() || m_execution_stopped)
		{
			m_icount = 0;
			break;
		}

		/*
		 * The host bootstrap path has populated the real 24-bit words
		 * before execution begins.  Mirror that initial image into the
		 * B2 program backing exactly once.
		 */
		if (!m_program_bootstrap_loaded)
		{
			for (unsigned address = 0; address < m_host.bootstrap_pos(); address++)
				m_program[address] = m_host.bootstrap_word(address) & 0x00ffffffU;

			m_program_bootstrap_loaded = true;
		}

		debugger_instruction_hook(m_pc);

		auto const result = dsp56000_execution::execute_one(
			m_pc,
			m_current_opcode,
			m_core,
			[this](std::uint16_t address)
			{
				return m_program[address] & 0x00ffffffU;
			},
			[this](std::uint16_t address, std::uint32_t value)
			{
				m_program[address] = value & 0x00ffffffU;
			},
			[this](bool y_space, std::uint16_t address)
			{
				/*
				 * X:$FFE9 is the standard DSP-side Host Status
				 * Register.  This is the real host-interface state,
				 * not a CD-i-specific fabricated value.
				 */
				if (!y_space && address == 0xffe9)
					return std::uint32_t(m_host.hsr());

				unsigned const offset = address & 0x3fU;

				return y_space
					? m_y_peripheral[offset]
					: m_x_peripheral[offset];
			},
			[this](bool y_space, std::uint16_t address, std::uint32_t value)
			{
				unsigned const offset = address & 0x3fU;

				if (y_space)
					m_y_peripheral[offset] = value & 0x00ffffffU;
				else
					m_x_peripheral[offset] = value & 0x00ffffffU;
			});

		if (result == dsp56000_execution::step_result::unsupported)
		{
			m_execution_stopped = true;
			m_icount = 0;
			break;
		}

		/*
		 * Timing remains provisional.  DSP-B2 validates architectural
		 * state transitions and bootstrap relocation, separately from
		 * cycle-level hardware fidelity.
		 */
		m_icount--;
	}
}

void dsp56000_device_base::execute_set_input(int inputnum, int state)
{
}

device_memory_interface::space_config_vector dsp56000_device_base::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(0, &m_p_config),
		std::make_pair(1, &m_x_config),
		std::make_pair(2, &m_y_config)
	};
}

std::unique_ptr<util::disasm_interface> dsp56000_device_base::create_disassembler()
{
	return std::make_unique<dsp56000_disassembler>();
}
