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
 * Partial architectural execution state.
 *
 * Only state required by implemented instruction families is present. Add
 * architectural state with the corresponding instruction semantics rather
 * than filling unimplemented registers with guessed behavior.
 */
struct core_state
{
	std::uint16_t r[8]{};
	std::uint32_t x0 = 0;

	std::uint16_t la = 0;
	std::uint16_t lc = 0;
	std::uint16_t loop_start = 0;
	bool loop_active = false;
};

inline void finish_loop(
		std::uint16_t instruction_pc,
		std::uint16_t &pc,
		core_state &state)
{
	if (!state.loop_active || instruction_pc != state.la)
		return;

	if (state.lc > 1)
	{
		state.lc--;
		pc = state.loop_start;
	}
	else
	{
		state.lc = 0;
		state.loop_active = false;
	}
}

/*
 * Focused execution entry point retained for short-absolute JMP tests:
 *
 *   0000 1100 0000 aaaa aaaa aaaa
 */
template <typename ProgramRead>
step_result execute_one(
		std::uint16_t &pc,
		std::uint32_t &opcode,
		ProgramRead &&read_program)
{
	opcode = read_program(pc) & 0x00ffffffU;

	if ((opcode & 0x00fff000U) == 0x000c0000U)
	{
		pc = std::uint16_t(opcode & 0x00000fffU);
		return step_result::executed;
	}

	return step_result::unsupported;
}

/*
 * Current partial interpreter.
 *
 * This implements only architecture-defined forms needed by the present
 * bootstrap execution path. Unsupported forms stop rather than being
 * approximated.
 */
template <
		typename ProgramRead,
		typename ProgramWrite,
		typename PeripheralRead,
		typename PeripheralWrite>
step_result execute_one(
		std::uint16_t &pc,
		std::uint32_t &opcode,
		core_state &state,
		ProgramRead &&read_program,
		ProgramWrite &&write_program,
		PeripheralRead &&read_peripheral,
		PeripheralWrite &&write_peripheral)
{
	std::uint16_t const instruction_pc = pc;

	opcode = read_program(pc) & 0x00ffffffU;
	std::uint32_t const ew =
		read_program(std::uint16_t(pc + 1)) & 0x00ffffffU;

	/*
	 * JMP <absolute-short>
	 */
	if ((opcode & 0x00fff000U) == 0x000c0000U)
	{
		pc = std::uint16_t(opcode & 0x00000fffU);
		finish_loop(instruction_pc, pc, state);
		return step_result::executed;
	}

	unsigned const top = (opcode >> 16) & 0x0fU;

	/*
	 * MOVEP #immediate,X/Y:<<pp
	 *
	 * Effective-address MOVEP with MMMRRR=$34 denotes a 24-bit
	 * immediate extension word.
	 */
	if ((top == 0x8U || top == 0x9U) &&
		(opcode & 0x000080U) &&
		(opcode & 0x008000U) &&
		(((opcode >> 8) & 0x3fU) == 0x34U))
	{
		bool const y_space = (opcode & 0x010000U) != 0;
		std::uint16_t const address =
			std::uint16_t(0xffc0U | ((opcode >> 8) & 0x3fU));

		write_peripheral(y_space, address, ew);

		pc = std::uint16_t(pc + 2);
		finish_loop(instruction_pc, pc, state);
		return step_result::executed;
	}

	/*
	 * MOVE #immediate,Rn
	 *
	 * DSP-B2 implements the long-immediate data-move form only when
	 * the destination encoding names R0-R7.
	 */
	if ((opcode & 0x0000ffU) == 0 &&
		(opcode & 0x400000U) &&
		(opcode & 0x340000U) &&
		(opcode & 0x004000U) &&
		(opcode & 0x008000U) &&
		(((opcode >> 8) & 0x3fU) == 0x34U))
	{
		unsigned const reg =
			((opcode >> 17) & 0x18U) |
			((opcode >> 16) & 0x07U);

		if (reg >= 16 && reg < 24)
		{
			state.r[reg - 16] = std::uint16_t(ew);

			pc = std::uint16_t(pc + 2);
			finish_loop(instruction_pc, pc, state);
			return step_result::executed;
		}
	}

	/*
	 * DO #immediate,expr
	 *
	 * DSP-B2 requires one active hardware loop.  Zero-count and nested
	 * loop behavior remains unsupported until implemented explicitly.
	 */
	if (top == 0x6U &&
		!(opcode & 0x000020U) &&
		(opcode & 0x000080U))
	{
		std::uint16_t const count =
			std::uint16_t(
				((opcode & 0x0fU) << 8) |
				((opcode >> 8) & 0xffU));

		if (count == 0 || state.loop_active)
			return step_result::unsupported;

		state.lc = count;
		state.la = std::uint16_t(ew);
		state.loop_start = std::uint16_t(pc + 2);
		state.loop_active = true;

		pc = std::uint16_t(pc + 2);
		return step_result::executed;
	}

	/*
	 * MOVEM P:(Rn)+,X0
	 * MOVEM X0,P:(Rn)+
	 *
	 * Only the postincrement/X0 forms needed by the bootstrap copier
	 * are enabled in this gate.
	 */
	if (top == 0x7U && (opcode & 0x000080U))
	{
		unsigned const mmmrrr = (opcode >> 8) & 0x3fU;
		unsigned const mode = mmmrrr >> 3;
		unsigned const rr = mmmrrr & 7U;
		unsigned const reg = opcode & 0x3fU;

		// MMM=011 -> (Rn)+ ; register code 4 -> X0.
		if (mode == 3U && reg == 4U)
		{
			std::uint16_t const address = state.r[rr];

			if (opcode & 0x008000U)
				state.x0 = read_program(address) & 0x00ffffffU;
			else
				write_program(address, state.x0 & 0x00ffffffU);

			state.r[rr] = std::uint16_t(state.r[rr] + 1);

			pc = std::uint16_t(pc + 1);
			finish_loop(instruction_pc, pc, state);
			return step_result::executed;
		}
	}

	/*
	 * JCLR #n,X/Y:<<pp,target
	 *
	 * Peripheral-memory form only.
	 */
	if ((top == 0xaU || top == 0xbU) &&
		((opcode & 0x00c000U) != 0x00c000U) &&
		((opcode & 0x0100a0U) == 0x000080U) &&
		((opcode & 0x00c000U) == 0x008000U))
	{
		bool const y_space = (opcode & 0x000040U) != 0;
		unsigned const bit = opcode & 31U;

		std::uint16_t const address =
			std::uint16_t(
				0xffc0U |
				((opcode >> 8) & 0x3fU));

		std::uint32_t const value =
			read_peripheral(y_space, address) & 0x00ffffffU;

		if (!(value & (std::uint32_t(1) << bit)))
			pc = std::uint16_t(ew);
		else
			pc = std::uint16_t(pc + 2);

		finish_loop(instruction_pc, pc, state);
		return step_result::executed;
	}

	/*
	 * JMP >absolute
	 *
	 * Effective-address JMP with MMM=110 consumes the extension word
	 * as the absolute target.
	 */
	if ((top == 0xaU || top == 0xbU) &&
		((opcode & 0x00c000U) == 0x00c000U) &&
		(opcode & 0x000080U) &&
		((opcode & 0x010020U) == 0) &&
		((((opcode >> 8) & 0x3fU) >> 3) == 6U))
	{
		pc = std::uint16_t(ew);
		finish_loop(instruction_pc, pc, state);
		return step_result::executed;
	}

	return step_result::unsupported;
}

} // namespace dsp56000_execution

#endif // MAME_CPU_DSP56000_DSP56000EXECUTE_H
