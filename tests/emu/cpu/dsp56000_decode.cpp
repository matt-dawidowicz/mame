// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "dsp56000execute.h"

namespace
{

struct decoder_probe
{
	std::array<std::uint32_t, 4> program{};
	std::array<std::uint32_t, 0x40> x_peripheral{};
	std::array<std::uint32_t, 0x40> y_peripheral{};

	unsigned program_reads = 0;
	unsigned program_writes = 0;
	unsigned peripheral_reads = 0;
	unsigned peripheral_writes = 0;

	dsp56000_execution::core_state state;
	std::uint16_t pc = 0;
	std::uint32_t opcode = 0;

	dsp56000_execution::step_result run(std::uint32_t instruction)
	{
		program[0] = instruction;
		program[1] = 0x000123;

		return dsp56000_execution::execute_one(
			pc,
			opcode,
			state,
			[this](std::uint16_t address)
			{
				program_reads++;
				return program[address < program.size() ? address : 0];
			},
			[this](std::uint16_t address, std::uint32_t value)
			{
				program_writes++;
				if (address < program.size())
					program[address] = value;
			},
			[this](bool y_space, std::uint16_t address)
			{
				peripheral_reads++;
				auto const &space = y_space ? y_peripheral : x_peripheral;
				return space[address & 0x3fU];
			},
			[this](bool y_space, std::uint16_t address, std::uint32_t value)
			{
				peripheral_writes++;
				auto &space = y_space ? y_peripheral : x_peripheral;
				space[address & 0x3fU] = value;
			});
	}
};

} // anonymous namespace


TEST_CASE(
	"DSP56000 partial decoder rejects neighboring instruction encodings",
	"[emu][cpu][dsp56000][execute][decode]")
{
	struct negative_case
	{
		char const *name;
		std::uint32_t opcode;
	};

	constexpr std::array<negative_case, 14> CASES =
	{{
		{ "short JMP parallel-prefix neighbor", 0x1c0002 },
		{ "MOVEP parallel-prefix neighbor",     0x18f4be },
		{ "MOVEP fixed-bit neighbor",           0x08b4be },
		{ "MOVE XY-class neighbor",             0xe0f400 },
		{ "DO parallel-prefix neighbor",        0x160380 },
		{ "DO low-nibble neighbor",             0x060390 },
		{ "DO fixed-bit neighbor",              0x0603c0 },
		{ "MOVEM parallel-prefix neighbor",     0x17d984 },
		{ "MOVEM addressing neighbor",          0x079984 },
		{ "MOVEM low-field neighbor",           0x07d9c4 },
		{ "JCLR parallel-prefix neighbor",      0x1aa983 },
		{ "JMP EA parallel-prefix neighbor",    0x1af080 },
		{ "JMP EA low-field neighbor 1",        0x0af081 },
		{ "JMP EA low-field neighbor 2",        0x0af0c0 },
	}};

	for (auto const &test : CASES)
	{
		decoder_probe probe;
		auto const result = probe.run(test.opcode);

		INFO("case=" << test.name << " opcode=" << test.opcode);
		REQUIRE(result == dsp56000_execution::step_result::unsupported);
		REQUIRE(probe.pc == 0);
		REQUIRE(probe.opcode == test.opcode);

		// Unsupported neighbors must not cause speculative extension reads or
		// any architectural side effects.
		REQUIRE(probe.program_reads == 1);
		REQUIRE(probe.program_writes == 0);
		REQUIRE(probe.peripheral_reads == 0);
		REQUIRE(probe.peripheral_writes == 0);
	}
}


TEST_CASE(
	"DSP56000 partial decoder fetches extension words only when required",
	"[emu][cpu][dsp56000][execute][decode][fetch]")
{
	SECTION("one-word short JMP does not read PC plus one")
	{
		decoder_probe probe;
		auto const result = probe.run(0x0c0002);

		REQUIRE(result == dsp56000_execution::step_result::executed);
		REQUIRE(probe.pc == 2);
		REQUIRE(probe.program_reads == 1);
	}

	SECTION("unsupported opcode does not read PC plus one")
	{
		decoder_probe probe;
		auto const result = probe.run(0x000001);

		REQUIRE(result == dsp56000_execution::step_result::unsupported);
		REQUIRE(probe.pc == 0);
		REQUIRE(probe.program_reads == 1);
	}

	SECTION("two-word immediate MOVEP reads its extension")
	{
		decoder_probe probe;
		auto const result = probe.run(0x08f480);

		REQUIRE(result == dsp56000_execution::step_result::executed);
		REQUIRE(probe.pc == 2);
		REQUIRE(probe.program_reads == 2);
		REQUIRE(probe.peripheral_writes == 1);
	}
}


TEST_CASE(
	"DSP56000 unsupported DO edge does not partially enter a loop",
	"[emu][cpu][dsp56000][execute][decode][loop]")
{
	decoder_probe probe;

	// The partial core does not yet claim the architectural zero-count edge
	// behavior. It must stop cleanly rather than creating guessed loop state.
	auto const result = probe.run(0x060080);

	REQUIRE(result == dsp56000_execution::step_result::unsupported);
	REQUIRE(probe.pc == 0);
	REQUIRE(probe.program_reads == 1);
	REQUIRE(probe.state.lc == 0);
	REQUIRE(probe.state.la == 0);
	REQUIRE(probe.state.loop_start == 0);
	REQUIRE_FALSE(probe.state.loop_active);
}
