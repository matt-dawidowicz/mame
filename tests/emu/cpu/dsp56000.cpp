// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz

#include <array>
#include <cstdint>

#include "catch.hpp"

#include "dsp56000execute.h"
#include "dsp56000host.h"

namespace
{

using host_interface = dsp56000_host_interface;

void host_write_word(host_interface &host, std::uint32_t value)
{
	host.write(host_interface::TRXH, std::uint8_t(value >> 16));
	host.write(host_interface::TRXM, std::uint8_t(value >> 8));
	host.write(host_interface::TRXL, std::uint8_t(value));
}

} // anonymous namespace


TEST_CASE(
	"DSP56000 host interface reset and command state are deterministic",
	"[emu][cpu][dsp56000][host][reset]")
{
	host_interface host;
	host.reset();

	REQUIRE(host.read(host_interface::ICR) == 0x00);
	REQUIRE(host.read(host_interface::CVR) == 0x12);
	REQUIRE(host.read(host_interface::ISR) == 0x06);
	REQUIRE(host.read(host_interface::IVR) == 0x0f);
	REQUIRE(host.hsr() == host_interface::HSR_HTDE);

	REQUIRE_FALSE(host.running());
	REQUIRE_FALSE(host.host_command_pending());
	REQUIRE_FALSE(host.dsp_receive_pending());
	REQUIRE(host.dsp_transmit_empty());
	REQUIRE(host.bootstrap_pos() == 0);

	// Host ISR is read-only.
	host.write(host_interface::ISR, 0xff);
	REQUIRE(host.read(host_interface::ISR) == 0x06);

	// Host register 4 is reserved in normal 24-bit transfers.
	host.write(host_interface::TRX0, 0xaa);
	REQUIRE(host.read(host_interface::TRX0) == 0x00);

	host.write(host_interface::CVR, 0x93);

	REQUIRE(host.read(host_interface::CVR) == 0x93);
	REQUIRE(host.host_command_pending());
	REQUIRE(host.host_command_vector() == 0x13);
	REQUIRE((host.hsr() & host_interface::HSR_HCP) != 0);

	host.acknowledge_host_command();

	REQUIRE(host.read(host_interface::CVR) == 0x13);
	REQUIRE_FALSE(host.host_command_pending());
	REQUIRE((host.hsr() & host_interface::HSR_HCP) == 0);
}


TEST_CASE(
	"DSP56000 bootstrap loading and INIT host flags are deterministic",
	"[emu][cpu][dsp56000][host][bootstrap]")
{
	host_interface host;
	host.reset();

	host_write_word(host, 0x123456);
	host_write_word(host, 0xabcdef);
	host_write_word(host, 0x010203);

	REQUIRE(host.bootstrap_pos() == 3);
	REQUIRE(host.bootstrap_word(0) == 0x123456);
	REQUIRE(host.bootstrap_word(1) == 0xabcdef);
	REQUIRE(host.bootstrap_word(2) == 0x010203);
	REQUIRE_FALSE(host.running());

	host.write(host_interface::ICR, 0x88);

	REQUIRE(host.running());
	REQUIRE(host.read(host_interface::ICR) == 0x08);
	REQUIRE(host.hsr() ==
		(host_interface::HSR_HTDE | host_interface::HSR_HF0));

	host.write(host_interface::ICR, 0x80);

	REQUIRE(host.running());
	REQUIRE(host.read(host_interface::ICR) == 0x00);
	REQUIRE(host.hsr() == host_interface::HSR_HTDE);
}


TEST_CASE(
	"DSP56000 host to DSP transport queues complete 24-bit words",
	"[emu][cpu][dsp56000][host][transport]")
{
	host_interface host;
	host.reset();
	host.write(host_interface::ICR, 0x88);

	host_write_word(host, 0x123456);

	REQUIRE(host.dsp_receive_pending());
	REQUIRE((host.read(host_interface::ISR) & host_interface::ISR_TRDY) == 0);

	std::uint32_t value = 0;
	REQUIRE(host.dsp_read_rx(value));
	REQUIRE(value == 0x123456);
	REQUIRE_FALSE(host.dsp_receive_pending());
	REQUIRE(host.read(host_interface::ISR) ==
		(host_interface::ISR_TRDY | host_interface::ISR_TXDE));

	// One DSP word can be active while a second host word remains queued.
	host_write_word(host, 0x111111);
	host_write_word(host, 0x222222);

	REQUIRE(host.dsp_receive_pending());
	REQUIRE((host.read(host_interface::ISR) & host_interface::ISR_TXDE) == 0);

	REQUIRE(host.dsp_read_rx(value));
	REQUIRE(value == 0x111111);
	REQUIRE(host.dsp_receive_pending());

	REQUIRE(host.dsp_read_rx(value));
	REQUIRE(value == 0x222222);
	REQUIRE_FALSE(host.dsp_receive_pending());
	REQUIRE(host.read(host_interface::ISR) ==
		(host_interface::ISR_TRDY | host_interface::ISR_TXDE));
}


TEST_CASE(
	"DSP56000 DSP to host transport preserves a pending second word",
	"[emu][cpu][dsp56000][host][transport]")
{
	host_interface host;
	host.reset();
	host.write(host_interface::ICR, 0x88);

	REQUIRE(host.dsp_write_tx(0xabcdef));
	REQUIRE((host.read(host_interface::ISR) & host_interface::ISR_RXDF) != 0);

	// RXDF is occupied, so this stays pending on the DSP side.
	REQUIRE(host.dsp_write_tx(0x102030));
	REQUIRE_FALSE(host.dsp_transmit_empty());

	REQUIRE(host.read(host_interface::TRXH) == 0xab);
	REQUIRE(host.read(host_interface::TRXM) == 0xcd);
	REQUIRE(host.read(host_interface::TRXL) == 0xef);

	// Reading the first low byte makes room and transfers the pending word.
	REQUIRE((host.read(host_interface::ISR) & host_interface::ISR_RXDF) != 0);
	REQUIRE(host.dsp_transmit_empty());

	REQUIRE(host.read(host_interface::TRXH) == 0x10);
	REQUIRE(host.read(host_interface::TRXM) == 0x20);
	REQUIRE(host.read(host_interface::TRXL) == 0x30);

	REQUIRE((host.read(host_interface::ISR) & host_interface::ISR_RXDF) == 0);
}


TEST_CASE(
	"DSP56000 short absolute JMP changes PC generically",
	"[emu][cpu][dsp56000][execute][jump]")
{
	std::array<std::uint32_t, 0x20> program{};

	program[0] = 0x0c0010;

	std::uint16_t pc = 0;
	std::uint32_t opcode = 0;

	auto result = dsp56000_execution::execute_one(
		pc,
		opcode,
		[&program](std::uint16_t address)
		{
			return program[address];
		});

	REQUIRE(result == dsp56000_execution::step_result::executed);
	REQUIRE(opcode == 0x0c0010);
	REQUIRE(pc == 0x0010);

	program[0x10] = 0x000001;

	result = dsp56000_execution::execute_one(
		pc,
		opcode,
		[&program](std::uint16_t address)
		{
			return program[address];
		});

	REQUIRE(result == dsp56000_execution::step_result::unsupported);
	REQUIRE(opcode == 0x000001);
	REQUIRE(pc == 0x0010);
}


TEST_CASE(
	"DSP56000 long immediate MOVE addresses all R registers",
	"[emu][cpu][dsp56000][execute][move]")
{
	for (unsigned reg = 0; reg < 8; ++reg)
	{
		std::array<std::uint32_t, 4> program{};
		std::array<std::uint32_t, 0x40> x_peripheral{};
		std::array<std::uint32_t, 0x40> y_peripheral{};

		program[0] = 0x60f400U | (reg << 16);
		program[1] = 0x100U + reg;

		dsp56000_execution::core_state state;
		std::uint16_t pc = 0;
		std::uint32_t opcode = 0;

		auto result = dsp56000_execution::execute_one(
			pc,
			opcode,
			state,
			[&program](std::uint16_t address)
			{
				return program[address];
			},
			[&program](std::uint16_t address, std::uint32_t value)
			{
				program[address] = value;
			},
			[&](bool y_space, std::uint16_t address)
			{
				auto const &space = y_space ? y_peripheral : x_peripheral;
				return space[address & 0x3fU];
			},
			[&](bool y_space, std::uint16_t address, std::uint32_t value)
			{
				auto &space = y_space ? y_peripheral : x_peripheral;
				space[address & 0x3fU] = value;
			});

		INFO("register=" << reg);
		REQUIRE(result == dsp56000_execution::step_result::executed);
		REQUIRE(pc == 2);
		REQUIRE(state.r[reg] == (0x100U + reg));
	}
}


TEST_CASE(
	"DSP56000 synthetic bootstrap path relocates program words",
	"[emu][cpu][dsp56000][execute][bootstrap]")
{
	std::array<std::uint32_t, 0x400> program{};
	std::array<std::uint32_t, 0x40> x_peripheral{};
	std::array<std::uint32_t, 0x40> y_peripheral{};

	// Generic short JMP to the synthetic loader.
	program[0x0000] = 0x0c0010;

	// MOVEP #$123456,X:<<$fff4
	program[0x0010] = 0x08f4b4;
	program[0x0011] = 0x123456;

	// MOVE #$20,R1
	program[0x0012] = 0x61f400;
	program[0x0013] = 0x000020;

	// MOVE #$100,R0
	program[0x0014] = 0x60f400;
	program[0x0015] = 0x000100;

	// DO #3,$19
	program[0x0016] = 0x060380;
	program[0x0017] = 0x000019;

	// MOVEM P:(R1)+,X0
	program[0x0018] = 0x07d984;

	// MOVEM X0,P:(R0)+
	program[0x0019] = 0x075884;

	// JCLR #3,X:<<$ffe9,$1a
	program[0x001a] = 0x0aa983;
	program[0x001b] = 0x00001a;

	// JMP >$300
	program[0x001c] = 0x0af080;
	program[0x001d] = 0x000300;

	program[0x0020] = 0x112233;
	program[0x0021] = 0x445566;
	program[0x0022] = 0x778899;

	// Deliberately unsupported next instruction.
	program[0x0300] = 0x000001;

	host_interface host;
	host.reset();
	host.write(host_interface::ICR, 0x88);

	dsp56000_execution::core_state state;
	std::uint16_t pc = 0;
	std::uint32_t opcode = 0;

	auto read_program =
		[&program](std::uint16_t address)
		{
			return program[address];
		};

	auto write_program =
		[&program](std::uint16_t address, std::uint32_t value)
		{
			program[address] = value & 0x00ffffffU;
		};

	auto read_peripheral =
		[&](bool y_space, std::uint16_t address)
		{
			if (!y_space && address == 0xffe9)
				return std::uint32_t(host.hsr());

			auto const &space = y_space ? y_peripheral : x_peripheral;
			return space[address & 0x3fU];
		};

	auto write_peripheral =
		[&](bool y_space, std::uint16_t address, std::uint32_t value)
		{
			auto &space = y_space ? y_peripheral : x_peripheral;
			space[address & 0x3fU] = value & 0x00ffffffU;
		};

	unsigned steps = 0;

	while (pc != 0x0300 && steps < 32)
	{
		auto const result = dsp56000_execution::execute_one(
			pc,
			opcode,
			state,
			read_program,
			write_program,
			read_peripheral,
			write_peripheral);

		REQUIRE(result == dsp56000_execution::step_result::executed);
		steps++;
	}

	REQUIRE(steps == 13);
	REQUIRE(pc == 0x0300);

	REQUIRE(state.r[0] == 0x0103);
	REQUIRE(state.r[1] == 0x0023);
	REQUIRE(state.x0 == 0x778899);
	REQUIRE(state.lc == 0);
	REQUIRE_FALSE(state.loop_active);

	REQUIRE(x_peripheral[0x34] == 0x123456);

	REQUIRE(program[0x0100] == 0x112233);
	REQUIRE(program[0x0101] == 0x445566);
	REQUIRE(program[0x0102] == 0x778899);

	auto const result = dsp56000_execution::execute_one(
		pc,
		opcode,
		state,
		read_program,
		write_program,
		read_peripheral,
		write_peripheral);

	REQUIRE(result == dsp56000_execution::step_result::unsupported);
	REQUIRE(pc == 0x0300);
	REQUIRE(opcode == 0x000001);
}


TEST_CASE(
	"DSP56000 JCLR peripheral form follows the tested bit",
	"[emu][cpu][dsp56000][execute][branch]")
{
	std::array<std::uint32_t, 0x200> program{};
	std::array<std::uint32_t, 0x40> peripheral{};

	// JCLR #3,X:<<$ffe9,$123
	program[0] = 0x0aa983;
	program[1] = 0x000123;

	auto read_program =
		[&program](std::uint16_t address)
		{
			return program[address];
		};

	auto write_program =
		[&program](std::uint16_t address, std::uint32_t value)
		{
			program[address] = value;
		};

	auto read_peripheral =
		[&peripheral](bool, std::uint16_t address)
		{
			return peripheral[address & 0x3fU];
		};

	auto write_peripheral =
		[&peripheral](bool, std::uint16_t address, std::uint32_t value)
		{
			peripheral[address & 0x3fU] = value;
		};

	dsp56000_execution::core_state state;
	std::uint16_t pc = 0;
	std::uint32_t opcode = 0;

	// Bit 3 clear: branch is taken.
	auto result = dsp56000_execution::execute_one(
		pc,
		opcode,
		state,
		read_program,
		write_program,
		read_peripheral,
		write_peripheral);

	REQUIRE(result == dsp56000_execution::step_result::executed);
	REQUIRE(pc == 0x0123);

	// Bit 3 set: branch falls through.
	peripheral[0x29] = 0x08;
	pc = 0;

	result = dsp56000_execution::execute_one(
		pc,
		opcode,
		state,
		read_program,
		write_program,
		read_peripheral,
		write_peripheral);

	REQUIRE(result == dsp56000_execution::step_result::executed);
	REQUIRE(pc == 2);
}
