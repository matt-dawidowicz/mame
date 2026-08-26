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
		unsigned instruction_words,
		std::uint16_t &pc,
		core_state &state)
{
	std::uint16_t const last_word =
		std::uint16_t(instruction_pc + instruction_words - 1U);

	if (!state.loop_active || last_word != state.la)
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

	// Do not speculatively read PC+1. Some implemented forms are one word and
	// unsupported instructions must stop without causing an extra program-space
	// access. Extension words are fetched only after a two-word form is decoded.
	auto const read_extension = [&read_program, instruction_pc]()
	{
		return read_program(std::uint16_t(instruction_pc + 1)) & 0x00ffffffU;
	};

	/*
	 * JMP <absolute-short>
	 *
	 *   0000 1100 0000 aaaa aaaa aaaa
	 */
	if ((opcode & 0x00fff000U) == 0x000c0000U)
	{
		pc = std::uint16_t(opcode & 0x00000fffU);
		finish_loop(instruction_pc, 1, pc, state);
		return step_result::executed;
	}

	/*
	 * MOVEP #immediate,X/Y:<<pp
	 *
	 *   0000 100s W1MM MRRR 1Spp pppp
	 *
	 * The currently implemented immediate form has W=1 and MMMRRR=$34.
	 * All fixed opcode fields are included in the mask so neighboring parallel
	 * move classes cannot be accepted accidentally.
	 */
	if ((opcode & 0x00feff80U) == 0x0008f480U)
	{
		bool const y_space = (opcode & 0x010000U) != 0;
		std::uint16_t const address =
			std::uint16_t(0xffc0U | ((opcode >> 8) & 0x3fU));

		write_peripheral(y_space, address, read_extension());

		pc = std::uint16_t(pc + 2);
		finish_loop(instruction_pc, 2, pc, state);
		return step_result::executed;
	}

	/*
	 * MOVE #immediate,Rn
	 *
	 * This is the immediate effective-address form of the X/Y memory data-move
	 * class with an R0-R7 destination. Bit 19 selects X/Y for memory forms but
	 * is don't-care for the immediate source, so both encodings are accepted.
	 */
	if ((opcode & 0x00f0ffffU) == 0x0060f400U)
	{
		unsigned const reg =
			((opcode >> 17) & 0x18U) |
			((opcode >> 16) & 0x07U);

		if (reg >= 16 && reg < 24)
		{
			state.r[reg - 16] = std::uint16_t(read_extension());

			pc = std::uint16_t(pc + 2);
			finish_loop(instruction_pc, 2, pc, state);
			return step_result::executed;
		}
	}

	/*
	 * DO #immediate,encoded-loop-end
	 *
	 *   0000 0110 iiii iiii 1000 hhhh
	 *
	 * The extension word is the encoded loop-end address used by the hardware
	 * loop state. The current partial core supports one active loop and treats a
	 * zero count as unsupported rather than inventing unverified edge behavior.
	 */
	if ((opcode & 0x00ff00f0U) == 0x00060080U)
	{
		std::uint16_t const count =
			std::uint16_t(
				((opcode & 0x0fU) << 8) |
				((opcode >> 8) & 0xffU));

		if (count == 0 || state.loop_active)
			return step_result::unsupported;

		state.lc = count;
		state.la = std::uint16_t(read_extension());
		state.loop_start = std::uint16_t(pc + 2);
		state.loop_active = true;

		pc = std::uint16_t(pc + 2);
		return step_result::executed;
	}

	/*
	 * MOVEM P:(Rn)+,X0
	 * MOVEM X0,P:(Rn)+
	 *
	 *   0000 0111 W1MM MRRR 10dd dddd
	 *
	 * Only the postincrement/X0 forms needed by the bootstrap copier are
	 * enabled in this partial interpreter.
	 */
	if ((opcode & 0x00ff40c0U) == 0x00074080U)
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
			finish_loop(instruction_pc, 1, pc, state);
			return step_result::executed;
		}
	}

	/*
	 * JCLR #n,X/Y:<<pp,target
	 *
	 *   0000 1010 10pp pppp 1S0b bbbb
	 *
	 * Peripheral-memory form only. The architectural bit number is 0-23;
	 * encodings 24-31 are reserved and remain unsupported.
	 */
	if ((opcode & 0x00ffc0a0U) == 0x000a8080U)
	{
		bool const y_space = (opcode & 0x000040U) != 0;
		unsigned const bit = opcode & 31U;

		if (bit >= 24U)
			return step_result::unsupported;

		std::uint16_t const address =
			std::uint16_t(
				0xffc0U |
				((opcode >> 8) & 0x3fU));

		std::uint32_t const value =
			read_peripheral(y_space, address) & 0x00ffffffU;
		std::uint16_t const target = std::uint16_t(read_extension());

		if (!(value & (std::uint32_t(1) << bit)))
			pc = target;
		else
			pc = std::uint16_t(pc + 2);

		finish_loop(instruction_pc, 2, pc, state);
		return step_result::executed;
	}

	/*
	 * JMP >absolute
	 *
	 *   0000 1010 11MM MRRR 1000 0000
	 *
	 * MMM=110 consumes the extension word as the absolute target; the RRR bits
	 * are not part of the absolute address and therefore remain don't-care.
	 */
	if ((opcode & 0x00fff8ffU) == 0x000af080U)
	{
		pc = std::uint16_t(read_extension());
		finish_loop(instruction_pc, 2, pc, state);
		return step_result::executed;
	}

	return step_result::unsupported;
}

} // namespace dsp56000_execution

#endif // MAME_CPU_DSP56000_DSP56000EXECUTE_H
