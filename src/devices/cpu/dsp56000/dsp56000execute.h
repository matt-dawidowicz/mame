// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay,Matt Dawidowicz

#ifndef MAME_CPU_DSP56000_DSP56000EXECUTE_H
#define MAME_CPU_DSP56000_DSP56000EXECUTE_H

#pragma once

#include <cstdint>

namespace dsp56000_execution
{

enum class step_result : std::uint8_t
{
	executed,
	unsupported
};

/*
 * Execute one DSP56000/DSP56001 instruction.
 *
 * DSP-B1 intentionally implements only the short-absolute JMP family:
 *
 *   0000 1100 0000 aaaa aaaa aaaa
 *
 * Additional instruction families are added as subsequent execution gates.
 */
template <typename ProgramRead>
step_result execute_one(
		std::uint16_t &pc,
		std::uint32_t &opcode,
		ProgramRead &&read_program)
{
	opcode = read_program(pc) & 0x00ffffffU;

	// JMP <absolute-short>
	if ((opcode & 0x00fff000U) == 0x000c0000U)
	{
		pc = std::uint16_t(opcode & 0x00000fffU);
		return step_result::executed;
	}

	return step_result::unsupported;
}

} // namespace dsp56000_execution

#endif // MAME_CPU_DSP56000_DSP56000EXECUTE_H
