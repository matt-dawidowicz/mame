// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "dsp56000execute.h"


TEST_CASE(
	"DSP56000 implemented two-word forms wrap program addresses cleanly",
	"[emu][cpu][dsp56000][execute][wrap]")
{
	std::array<std::uint32_t, 0x10000> program{};
	std::array<std::uint32_t, 0x40> x_peripheral{};
	std::array<std::uint32_t, 0x40> y_peripheral{};

	auto read_program = [&program](std::uint16_t address)
	{
		return program[address];
	};
	auto write_program = [&program](std::uint16_t address, std::uint32_t value)
	{
		program[address] = value & 0x00ffffffU;
	};
	auto read_peripheral = [&](bool y_space, std::uint16_t address)
	{
		auto const &space = y_space ? y_peripheral : x_peripheral;
		return space[address & 0x3fU];
	};
	auto write_peripheral = [&](bool y_space, std::uint16_t address, std::uint32_t value)
	{
		auto &space = y_space ? y_peripheral : x_peripheral;
		space[address & 0x3fU] = value & 0x00ffffffU;
	};

	SECTION("MOVE immediate fetches its extension across FFFF to 0000")
	{
		program[0xffff] = 0x60f400; // MOVE #immediate,R0
		program[0x0000] = 0x123456;

		dsp56000_execution::core_state state;
		std::uint16_t pc = 0xffff;
		std::uint32_t opcode = 0;

		REQUIRE(dsp56000_execution::execute_one(
			pc, opcode, state,
			read_program, write_program,
			read_peripheral, write_peripheral) ==
			dsp56000_execution::step_result::executed);
		REQUIRE(opcode == 0x60f400);
		REQUIRE(pc == 0x0001);
		REQUIRE(state.r[0] == 0x3456);
	}

	SECTION("long absolute JMP fetches its extension across FFFF to 0000")
	{
		program[0xffff] = 0x0af080;
		program[0x0000] = 0xab1234;

		dsp56000_execution::core_state state;
		std::uint16_t pc = 0xffff;
		std::uint32_t opcode = 0;

		REQUIRE(dsp56000_execution::execute_one(
			pc, opcode, state,
			read_program, write_program,
			read_peripheral, write_peripheral) ==
			dsp56000_execution::step_result::executed);
		REQUIRE(pc == 0x1234);
	}
}


TEST_CASE(
	"DSP56000 implemented address-register and loop arithmetic wraps at 16 bits",
	"[emu][cpu][dsp56000][execute][wrap]")
{
	std::array<std::uint32_t, 0x10000> program{};
	std::array<std::uint32_t, 0x40> x_peripheral{};
	std::array<std::uint32_t, 0x40> y_peripheral{};

	auto read_program = [&program](std::uint16_t address)
	{
		return program[address];
	};
	auto write_program = [&program](std::uint16_t address, std::uint32_t value)
	{
		program[address] = value & 0x00ffffffU;
	};
	auto read_peripheral = [&](bool y_space, std::uint16_t address)
	{
		auto const &space = y_space ? y_peripheral : x_peripheral;
		return space[address & 0x3fU];
	};
	auto write_peripheral = [&](bool y_space, std::uint16_t address, std::uint32_t value)
	{
		auto &space = y_space ? y_peripheral : x_peripheral;
		space[address & 0x3fU] = value & 0x00ffffffU;
	};

	SECTION("MOVEM postincrement wraps R0 from FFFF to 0000")
	{
		program[0x0000] = 0x07d884; // MOVEM P:(R0)+,X0
		program[0xffff] = 0xabcdef;

		dsp56000_execution::core_state state;
		state.r[0] = 0xffff;
		std::uint16_t pc = 0;
		std::uint32_t opcode = 0;

		REQUIRE(dsp56000_execution::execute_one(
			pc, opcode, state,
			read_program, write_program,
			read_peripheral, write_peripheral) ==
			dsp56000_execution::step_result::executed);
		REQUIRE(state.x0 == 0xabcdef);
		REQUIRE(state.r[0] == 0x0000);
		REQUIRE(pc == 0x0001);
	}

	SECTION("a two-word loop end recognizes LA when the final word wraps to 0000")
	{
		program[0xffff] = 0x60f400; // MOVE #immediate,R0
		program[0x0000] = 0x000055;

		dsp56000_execution::core_state state;
		state.loop_active = true;
		state.loop_start = 0x1234;
		state.la = 0x0000;
		state.lc = 2;
		std::uint16_t pc = 0xffff;
		std::uint32_t opcode = 0;

		REQUIRE(dsp56000_execution::execute_one(
			pc, opcode, state,
			read_program, write_program,
			read_peripheral, write_peripheral) ==
			dsp56000_execution::step_result::executed);
		REQUIRE(state.r[0] == 0x0055);
		REQUIRE(state.lc == 1);
		REQUIRE(state.loop_active);
		REQUIRE(pc == 0x1234);
	}
}
