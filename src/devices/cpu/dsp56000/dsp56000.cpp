// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * An emulation of the Motorola DSP56000/DSP56001.
 *
 * Sources:
 *   - http://www.bitsavers.org/components/motorola/56000/1990_DSP56000_DSP56001_Users_Manual.pdf
 *
 * TODO:
 *   - emulation
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
	m_host.reset();
}

void dsp56000_device_base::execute_run()
{
	while (m_icount > 0)
	{
		/*
		 * Before the bootstrap START condition, the DSP host interface is
		 * active but instruction execution remains held.
		 */
		if (!m_host.running() || m_execution_stopped)
		{
			m_icount = 0;
			break;
		}

		debugger_instruction_hook(m_pc);

		auto const result = dsp56000_execution::execute_one(
			m_pc,
			m_current_opcode,
			[this](std::uint16_t address)
			{
				/*
				 * DSP-B1 executes directly from the on-chip bootstrap RAM
				 * populated by the host interface.  A complete P-space
				 * implementation follows when the loader begins relocating
				 * code outside this 512-word region.
				 */
				return m_host.bootstrap_word(address);
			});

		if (result == dsp56000_execution::step_result::unsupported)
		{
			/*
			 * Never invent instruction behavior.  Stop on the exact
			 * unsupported opcode so the next implementation gate is
			 * observable and deterministic.
			 */
			m_execution_stopped = true;
			m_icount = 0;
			break;
		}

		/*
		 * Instruction timing is intentionally provisional at this stage.
		 * DSP-B1 validates instruction semantics and PC flow, not
		 * cycle-accurate timing.
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
