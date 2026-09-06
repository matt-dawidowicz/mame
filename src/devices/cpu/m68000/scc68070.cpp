// license:BSD-3-Clause
// copyright-holders:Karl Stenerud

#include "emu.h"
#include "scc68070.h"
#include "m68kdasm.h"

std::unique_ptr<util::disasm_interface> scc68070_base_device::create_disassembler()
{
	return std::make_unique<m68k_disassembler>(m68k_disassembler::TYPE_68000);
}


scc68070_base_device::scc68070_base_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock,
						const device_type type, address_map_constructor internal_map)
	: m68000_musashi_device(mconfig, tag, owner, clock, type, 16,32, internal_map)
{
}

void scc68070_base_device::device_start()
{
	m68000_musashi_device::device_start();
	init_cpu_scc68070();

	m_readimm16 = [this](offs_t address) -> u16
	{
		offs_t translated[2];
		if (!translate_span(address, scc68070_access_type::execute, true, translated, 2))
			return 0xffff;

		if (translated[1] == translated[0] + 1)
			return m_oprogram16.read_word(translated[0]);

		return (u16(m_oprogram16.read_byte(translated[0])) << 8)
			| u16(m_oprogram16.read_byte(translated[1]));
	};
	m_read8 = [this](offs_t address) -> u8
	{
		offs_t translated;
		return translate_address(address, scc68070_access_type::read, true, translated)
			? m_program16.read_byte(translated)
			: 0xff;
	};
	m_read16 = [this](offs_t address) -> u16
	{
		offs_t translated[2];
		if (!translate_span(address, scc68070_access_type::read, true, translated, 2))
			return 0xffff;

		if (translated[1] == translated[0] + 1)
			return m_program16.read_word(translated[0]);

		return (u16(m_program16.read_byte(translated[0])) << 8)
			| u16(m_program16.read_byte(translated[1]));
	};
	m_read32 = [this](offs_t address) -> u32
	{
		offs_t translated[4];
		if (!translate_span(address, scc68070_access_type::read, true, translated, 4))
			return 0xffffffff;

		if (translated[1] == translated[0] + 1
			&& translated[2] == translated[0] + 2
			&& translated[3] == translated[0] + 3)
		{
			return m_program16.read_dword(translated[0]);
		}

		return (u32(m_program16.read_byte(translated[0])) << 24)
			| (u32(m_program16.read_byte(translated[1])) << 16)
			| (u32(m_program16.read_byte(translated[2])) << 8)
			| u32(m_program16.read_byte(translated[3]));
	};
	m_write8 = [this](offs_t address, u8 data)
	{
		offs_t translated;
		if (!translate_address(address, scc68070_access_type::write, true, translated))
			return;

		m_program16.write_word(translated & ~1, data | (data << 8), translated & 1 ? 0x00ff : 0xff00);
	};
	m_write16 = [this](offs_t address, u16 data)
	{
		offs_t translated[2];
		if (!translate_span(address, scc68070_access_type::write, true, translated, 2))
			return;

		if (translated[1] == translated[0] + 1)
		{
			m_program16.write_word(translated[0], data);
			return;
		}

		m_program16.write_word(translated[0] & ~1, (data >> 8) * 0x0101, translated[0] & 1 ? 0x00ff : 0xff00);
		m_program16.write_word(translated[1] & ~1, (data & 0xff) * 0x0101, translated[1] & 1 ? 0x00ff : 0xff00);
	};
	m_write32 = [this](offs_t address, u32 data)
	{
		offs_t translated[4];
		if (!translate_span(address, scc68070_access_type::write, true, translated, 4))
			return;

		if (translated[1] == translated[0] + 1
			&& translated[2] == translated[0] + 2
			&& translated[3] == translated[0] + 3)
		{
			m_program16.write_dword(translated[0], data);
			return;
		}

		for (unsigned byte = 0; byte < 4; ++byte)
		{
			const u8 value = data >> (24 - byte * 8);
			const offs_t physical = translated[byte];
			m_program16.write_word(physical & ~1, value | (value << 8), physical & 1 ? 0x00ff : 0xff00);
		}
	};
}

bool scc68070_base_device::translate_address(offs_t address, scc68070_access_type, bool, offs_t &translated)
{
	// Internal addresses (0x80000000-bfffffff) are only accessible in supervisor mode;
	// all other accesses use the external 24-bit address bus.  The base device has no
	// translation/protection layer; the full SCC68070 device overrides this hook.
	translated = address;
	if (!(supervisor_mode() && (address >> 30) == 0x2))
		translated &= 0xffffff;
	return true;
}

bool scc68070_base_device::translate_span(
		offs_t address,
		scc68070_access_type access,
		bool side_effects,
		offs_t *translated,
		unsigned width)
{
	// Validate the complete operand before touching the physical address space.  This
	// prevents a cross-segment word/long write from becoming partially visible before
	// a later byte raises an MMU fault.
	for (unsigned byte = 0; byte < width; ++byte)
	{
		if (!translate_address(address + byte, access, side_effects, translated[byte]))
			return false;
	}
	return true;
}

bool scc68070_base_device::memory_translate(int spacenum, int intention, offs_t &address, address_space *&target_space)
{
	target_space = &space(spacenum);
	if (spacenum == AS_PROGRAM)
	{
		const scc68070_access_type access =
			(intention == device_memory_interface::TR_FETCH) ? scc68070_access_type::execute :
			(intention == device_memory_interface::TR_WRITE) ? scc68070_access_type::write :
			scc68070_access_type::read;
		offs_t translated;
		// Debugger/disassembler translation must never mutate the MMU status register or
		// inject a CPU exception.  Actual fetch/read/write callbacks perform fault side
		// effects; this path only answers whether the requested translation is legal.
		if (!translate_address(address, access, false, translated))
			return false;
		address = translated;
	}
	return true;
}
