// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Included after the generated Q-disc and full-machine test support.
namespace
{

class cdi_transport_state;
cdi_transport_state *cdi_transport_capture = nullptr;

class cdi_transport_state : public cdi_state
{
public:
	cdi_transport_state(machine_config const &config, device_type type, char const *tag)
		: cdi_state(config, type, tag) { }
	void cdi_transport(machine_config &config)
	{
		cdimono1(config);
		m_cdic->intreq_callback().append(FUNC(cdi_transport_state::irq_w));
	}
	void irq_w(int state) { irq = bool(state); }
	std::string image_path;
	unsigned scenario = 0; // seek, audio/data, data direct, data pre-start, data/audio, audio only
	bool completed = false;
	bool irq = false;
	std::vector<std::string> failures;
	std::vector<unsigned> positions;
	std::array<std::vector<float>, 2> pcm;
	void capture(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
	{
		for (unsigned channel = 0; channel < 2; ++channel)
		{
			auto const found = sound.find(channel ? ":dac2" : ":dac1");
			if (found == sound.end() || found->second.empty()) continue;
			auto const &buffer = found->second[0];
			pcm[channel].insert(pcm[channel].end(), buffer.first, buffer.first + buffer.second);
		}
	}

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_timer = timer_alloc(FUNC(cdi_transport_state::step), this);
		for (auto &dac : m_dmadac) dac->set_sound_hook(true);
	}
	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		m_timer->adjust(attotime::zero);
	}

private:
	struct command_step { uint16_t command; unsigned target, first, packets, abort_ms; bool whole_second; };
	// The supplied cdapdriv accepts position up to 40 frames before its target.
	// A rounded seek to 673 must receive 600..633, not just one packet.
	static constexpr command_step SEEKS[] = {
		{0x2c, 673, 600, 34, 0, true}, {0x2c, 475, 450, 1, 0, true},
		{0x2c, 900, 900, 0, 20, false}, {0x29, 603, 603, 2, 0, false}
	};
	command_step current() const
	{
		unsigned const first = scenario == 1 ? 598 : scenario == 4 ? 673 : scenario == 5 ? 450 : 600;
		return scenario ? command_step{0x28, first, first, 8, 0, false} : SEEKS[m_command_index];
	}
	void expect(bool ok, std::string const &message)
	{
		if (!ok) failures.push_back(message);
	}
	void begin(address_space &space)
	{
		auto const command = current();
		unsigned const absolute = command.target + 150;
		uint32_t time = (uint32_t(cdi_q_disc::bcd(absolute / 4500)) << 24)
			| (uint32_t(cdi_q_disc::bcd(absolute / 75 % 60)) << 16)
			| (uint32_t(cdi_q_disc::bcd(absolute % 75)) << 8);
		if (command.whole_second) time |= 0x8000;
		space.write_word(0x303ffe, 0);
		space.read_word(0x303ff6);
		space.write_word(0x303c00, command.command);
		space.write_dword(0x303c02, time);
		space.write_word(0x303ffe, 0xc000);
		expect((space.read_word(0x303ffe) & 0xc000) == 0x4000, "command request was not acknowledged");
		expect(!irq, "IRQ asserted before sector acquisition");
		if (scenario && scenario != 3) space.write_word(0x303ffa, 0x0800);
		m_packets = m_elapsed = 0;
	}
	void stop(address_space &space)
	{
		// Driver completion masks further disc IRQs and stops acquisition.
		space.write_word(0x303ffe, space.read_word(0x303ffe) & ~0x4000);
		expect(!irq, "IRQ stayed asserted after DBUF disable");
		m_quiet = true;
		m_elapsed = 0;
	}
	static unsigned unbcd(uint16_t value) { return (value >> 4) * 10 + (value & 15); }
	TIMER_CALLBACK_MEMBER(step)
	{
		auto &space = m_maincpu->space(AS_PROGRAM);
		if (!m_loaded)
		{
			auto const result = m_cdrom->load(image_path);
			expect(!result.first, "generated disc failed to load: " + result.second);
			if (result.first) { machine().schedule_exit(); return; }
			m_loaded = true;
			begin(space);
		}
		++m_elapsed;
		bool const irq_before_ack = irq;
		uint16_t const xbuf = space.read_word(0x303ff6);
		if (xbuf & 0x8000)
		{
			expect(!m_quiet, "sector delivered after software completion/abort");
			expect(irq_before_ack, "XBUF event did not assert the CDIC IRQ");
			expect(!irq, "XBUF read did not acknowledge the CDIC IRQ");
			auto const command = current();
			expect(!command.abort_ms, "aborted seek delivered a sector");
			uint16_t const dbuf = space.read_word(0x303ffe);
			expect((dbuf & 0xc000) == 0x4000, "sector completion lost DBUF enable or reasserted request");
			unsigned const q = 0x300000 + (dbuf & 1) * 0xa00 + 0x924;
			unsigned const absolute = unbcd(space.read_word(q + 14)) * 4500
				+ unbcd(space.read_word(q + 16)) * 75 + unbcd(space.read_word(q + 18));
			unsigned const lba = absolute - 150;
			positions.push_back(lba);
			expect(lba == command.first + m_packets, "unexpected seek/read Q position: " + std::to_string(lba));
			expect((space.read_word(q) & 15) == 1, "position Q is not ADR 1");
			++m_packets;
			if (scenario == 3 && m_packets == 1) space.write_word(0x303ffa, 0x0800);
			if (m_packets == command.packets)
			{
				if (!scenario && command.command == 0x2c)
					expect(lba <= command.target && command.target - lba <= 40, "driver seek window not reached");
				stop(space);
			}
		}
		if (!m_quiet && current().abort_ms && m_elapsed == current().abort_ms)
			stop(space);
		if (m_quiet && m_elapsed >= 150)
		{
			if (scenario || ++m_command_index == std::size(SEEKS))
			{
				completed = true;
				machine().schedule_exit();
				return;
			}
			m_quiet = false;
			begin(space);
		}
		if (!m_quiet && m_elapsed > 1000)
		{
			failures.push_back("timed out awaiting transport completion");
			machine().schedule_exit();
			return;
		}
		m_timer->adjust(attotime::from_msec(1));
	}
	emu_timer *m_timer = nullptr;
	bool m_loaded = false, m_quiet = false;
	unsigned m_command_index = 0, m_packets = 0, m_elapsed = 0;
};

ROM_START(cditrans)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cditrans, 0, cdi_transport, cdi_dma_integration,
	cdi_transport_state, empty_init, ROT0, "MAME", "CD-i synthetic transport fixture", 0)

void run_cdi_transport(unsigned scenario)
{
	cdi_q_disc disc(0, 0, true);
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cditrans).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cditrans), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);
	auto &state = downcast<cdi_transport_state &>(machine.root_device());
	state.image_path = disc.path();
	state.scenario = scenario;
	cdi_transport_capture = &state;
	int const error = machine.run(true);
	cdi_transport_capture = nullptr;
	manager.set_machine(nullptr);
	REQUIRE(error == EMU_ERR_NONE);
	for (auto const &failure : state.failures) { INFO(failure); CHECK(false); }
	REQUIRE(state.completed);
	CHECK(state.positions.size() == (scenario ? 8 : 37));
	for (unsigned channel = 0; channel < 2; ++channel)
	{
		CAPTURE(scenario);
		CAPTURE(channel);
		REQUIRE_FALSE(state.pcm[channel].empty());
		unsigned audio_samples = 0, data_samples = 0;
		for (float sample : state.pcm[channel])
		{
			if (sample > 0.12f && sample < 0.15f) ++audio_samples;
			if (sample > 0.3f || sample < -0.3f) ++data_samples;
		}
		CAPTURE(audio_samples);
		CAPTURE(data_samples);
		CHECK(data_samples == 0);
		unsigned const expected_audio = scenario == 1 ? 2 * 588 : scenario == 4 ? 6 * 588 : scenario == 5 ? 8 * 588 : 0;
		CHECK(audio_samples == expected_audio);
		if (!expected_audio)
			CHECK(std::all_of(state.pcm[channel].begin(), state.pcm[channel].end(), [](float sample) { return sample == 0.0f; }));
	}
}

TEST_CASE("CDIC completes driver-controlled seek via Q IRQ acknowledgement and DBUF disable", "[emu][philips][cdic][seek][transport][integration]")
{
	run_cdi_transport(0);
}

TEST_CASE("CDIC CDDA transport excludes data-track payload from DAC output", "[emu][philips][cdic][pcm][transport][integration]")
{
	for (unsigned scenario = 1; scenario <= 5; ++scenario) run_cdi_transport(scenario);
}

void cdi_transport_sound_hook(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
{
	if (cdi_transport_capture) cdi_transport_capture->capture(sound);
}

} // anonymous namespace
