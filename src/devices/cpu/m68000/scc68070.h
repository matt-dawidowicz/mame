// license:BSD-3-Clause
// copyright-holders:Karl Stenerud
#ifndef MAME_CPU_M68000_SCC68070_H
#define MAME_CPU_M68000_SCC68070_H

#pragma once

#include "m68kmusashi.h"

enum class scc68070_access_type : u8
{
	execute,
	read,
	write
};

class scc68070_base_device : public m68000_musashi_device
{
protected:
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;

	virtual u32 execute_min_cycles() const noexcept override { return 4; }
	virtual u32 execute_max_cycles() const noexcept override { return 158; }

	// device-level overrides
	virtual void device_start() override ATTR_COLD;

	virtual bool translate_address(offs_t address, scc68070_access_type access, bool side_effects, offs_t &translated);
	bool translate_span(offs_t address, scc68070_access_type access, bool side_effects, offs_t *translated, unsigned width);

	virtual bool memory_translate(int spacenum, int intention, offs_t &address, address_space *&target_space) override;

	scc68070_base_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock,
						const device_type type, address_map_constructor internal_map);
};

#endif
