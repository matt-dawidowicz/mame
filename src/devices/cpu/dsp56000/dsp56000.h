// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#ifndef MAME_CPU_DSP56000_DSP56000_H
#define MAME_CPU_DSP56000_DSP56000_H

#pragma once

#include "dsp56000host.h"
#include "dsp56000execute.h"

class dsp56000_device_base : public cpu_device
{
public:
	u8 host_r(offs_t offset) { return m_host.read(unsigned(offset)); }
	void host_w(offs_t offset, u8 data) { m_host.write(unsigned(offset), data); }

	u16 host_bootstrap_pos() const noexcept { return m_host.bootstrap_pos(); }
	u32 host_bootstrap_word(u16 address) const noexcept { return m_host.bootstrap_word(address); }
	bool host_execution_started() const noexcept { return m_host.running(); }

	u16 execution_pc() const noexcept { return m_pc; }
	u32 execution_opcode() const noexcept { return m_current_opcode; }
	bool execution_stopped() const noexcept { return m_execution_stopped; }

protected:
	dsp56000_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock);

	// device_t overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// device_execute_interface overrides
	virtual u64 execute_clocks_to_cycles(u64 clocks) const noexcept override { return (clocks + 2 - 1) / 2; }
	virtual u64 execute_cycles_to_clocks(u64 cycles) const noexcept override { return (cycles * 2); }
	virtual u32 execute_min_cycles() const noexcept override { return 1; }
	virtual u32 execute_max_cycles() const noexcept override { return 16; }
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;

	// device_disasm_interface overrides
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;

	// emulation state
	address_space_config m_p_config;
	address_space_config m_x_config;
	address_space_config m_y_config;

	dsp56000_host_interface m_host;
	dsp56000_execution::core_state m_core;

	/*
	 * Temporary execution backing used by the current partial interpreter.
	 * This is functional execution state, not yet a claim of final
	 * DSP56001 external-memory timing/fidelity.
	 */
	u32 m_program[0x10000]{};
	u32 m_x_peripheral[0x40]{};
	u32 m_y_peripheral[0x40]{};
	bool m_program_bootstrap_loaded = false;

	int m_icount;

	// program-visible cpu state
	u16 m_pc;
	u32 m_current_opcode = 0;

	// Temporary execution gate.  An unsupported instruction stops the
	// interpreter at that instruction rather than fabricating behavior.
	bool m_execution_stopped = false;
};

class dsp56000_device : public dsp56000_device_base
{
public:
	dsp56000_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);
};

class dsp56001_device : public dsp56000_device_base
{
public:
	dsp56001_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);
};

DECLARE_DEVICE_TYPE(DSP56000, dsp56000_device)
DECLARE_DEVICE_TYPE(DSP56001, dsp56001_device)

#endif // MAME_CPU_DSP56000_DSP56000_H
