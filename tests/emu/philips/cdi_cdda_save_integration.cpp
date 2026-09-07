// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Included after the generated Q-disc and full-machine test support.
namespace
{

class cdi_cdda_save_state;
cdi_cdda_save_state *cdi_cdda_save_capture = nullptr;

class cdi_cdda_save_state : public cdi_state
{
public:
	cdi_cdda_save_state(machine_config const &config, device_type type, char const *tag)
		: cdi_state(config, type, tag) { }
	void cdi_cdda_save(machine_config &config) { cdimono1(config); }
	std::string image_path, snapshot_path;
	unsigned first_lba = 450, checkpoint_ms = 143;
	bool pending_irq = false, emphasis = false, completed = false;
	std::vector<std::string> failures;
	struct observation
	{
		std::vector<uint16_t> registers;
		std::vector<unsigned> positions;
		std::vector<std::pair<int64_t, unsigned>> sound_times;
		std::array<std::vector<float>, 2> pcm;
	};
	std::array<observation, 2> runs;
	unsigned save_count = 0, load_count = 0;
	void capture(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
	{
		for (unsigned channel = 0; channel < 2; ++channel)
		{
			auto const found = sound.find(channel ? ":dac2" : ":dac1");
			if (found == sound.end() || found->second.empty()) continue;
			auto const &buffer = found->second[0];
			if (!m_collect)
			{
				if (!channel && !m_saved)
					m_initial_audio += std::count_if(buffer.first, buffer.first + buffer.second, [](float s) { return s != 0; });
				continue;
			}
			if (!channel) runs[m_pass].sound_times.emplace_back(machine().time().as_ticks(44100), buffer.second);
			auto &pcm = runs[m_pass].pcm[channel];
			pcm.insert(pcm.end(), buffer.first, buffer.first + buffer.second);
		}
	}

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_timer = timer_alloc(FUNC(cdi_cdda_save_state::step), this);
		// Save only the emulated test consumer's polling position. Comparison
		// buffers and run number deliberately stay outside the machine snapshot.
		save_item(NAME(m_elapsed));
		save_item(NAME(m_packets));
		machine().save().register_presave(save_prepost_delegate(FUNC(cdi_cdda_save_state::presave), this));
		machine().save().register_postload(save_prepost_delegate(FUNC(cdi_cdda_save_state::postload), this));
		for (auto &dac : m_dmadac) dac->set_sound_hook(true);
	}
	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		m_timer->adjust(attotime::zero);
	}

private:
	void expect(bool ok, std::string const &message)
	{
		if (!ok) failures.push_back(message);
	}
	void presave()
	{
		++save_count;
		expect(m_elapsed == checkpoint_ms, "save was deferred beyond the requested checkpoint");
		expect(m_cdic_irq_state == pending_irq, "snapshot has the wrong pending IRQ state");
		expect(m_initial_audio > 588, "snapshot did not follow active DAC output");
	}
	void postload()
	{
		++load_count;
		expect(m_elapsed == checkpoint_ms, "load did not restore the consumer clock");
		expect(m_cdic_irq_state == pending_irq, "load did not restore the driver's CDIC IRQ line");
		expect(machine().time() == m_checkpoint_time, "load did not restore machine time");
	}
	void poll(address_space &space)
	{
		bool const before = m_cdic_irq_state;
		uint16_t const xbuf = space.read_word(0x303ff6);
		uint16_t const dbuf = space.read_word(0x303ffe);
		expect(!m_cdic_irq_state, "XBUF read failed to acknowledge IRQ");
		expect(bool(xbuf & 0x8000) == before, "XBUF readiness disagrees with IRQ");
		expect((dbuf & 0xc000) == 0x4000, "active CDDA lost DBUF enable");
		unsigned lba = 0;
		if (xbuf & 0x8000)
		{
			unsigned const q = 0x300000 + (dbuf & 1) * 0xa00 + 0x924;
			auto unbcd = [](uint16_t value) { return (value >> 4) * 10 + (value & 15); };
			lba = unbcd(space.read_word(q + 14)) * 4500
				+ unbcd(space.read_word(q + 16)) * 75 + unbcd(space.read_word(q + 18)) - 150;
			expect(lba == first_lba + m_packets, "Q position skipped or repeated a sector");
			expect((space.read_word(q) & 15) == 1, "Q packet lost position ADR");
			expect(bool(space.read_word(q) & 0x10) == emphasis, "Q emphasis flag disagrees with generated track");
			expect(unbcd(space.read_word(q + 2)) == (lba < 600 ? 2 : 3), "Q track disagrees with generated layout");
			++m_packets;
		}
		if (m_collect)
		{
			auto &registers = runs[m_pass].registers;
			registers.insert(registers.end(), {uint16_t(m_elapsed), xbuf, dbuf, uint16_t(before), uint16_t(m_cdic_irq_state), m_irq4_owner});
			if (xbuf & 0x8000)
			{
				runs[m_pass].positions.push_back(lba);
				unsigned const q = 0x300000 + (dbuf & 1) * 0xa00 + 0x924;
				for (unsigned i = 0; i < 12; ++i) registers.push_back(space.read_word(q + i * 2));
			}
		}
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		auto &space = m_maincpu->space(AS_PROGRAM);
		m_timer->adjust(attotime::from_msec(1));
		if (!m_loaded)
		{
			auto const result = m_cdrom->load(image_path);
			expect(!result.first, "generated disc load failed: " + result.second);
			if (result.first) { machine().schedule_exit(); return; }
			m_loaded = true;
			unsigned const absolute = first_lba + 150;
			space.write_word(0x303c00, 0x28);
			space.write_dword(0x303c02, (uint32_t(cdi_q_disc::bcd(absolute / 4500)) << 24)
				| (uint32_t(cdi_q_disc::bcd(absolute / 75 % 60)) << 16)
				| (uint32_t(cdi_q_disc::bcd(absolute % 75)) << 8));
			space.write_word(0x303ffe, 0xc000);
			space.write_word(0x303ffa, 0x0800);
			return;
		}
		++m_elapsed;
		if (m_elapsed == checkpoint_ms + 1)
		{
			expect(save_count == 1 && load_count == m_pass, "scheduled save/load did not complete");
			m_collect = true;
		}
		// Snapshot either after acknowledgement, or with the packet still pending.
		if (!(pending_irq && m_elapsed == checkpoint_ms)) poll(space);
		if (!m_saved && m_elapsed == checkpoint_ms)
		{
			m_saved = true;
			m_checkpoint_time = machine().time();
			machine().schedule_save(std::string(snapshot_path));
		}
		if (m_elapsed == 400)
		{
			m_collect = false;
			if (!m_pass)
			{
				m_pass = 1;
				machine().schedule_load(std::string(snapshot_path));
			}
			else
			{
				completed = true;
				machine().schedule_exit();
			}
		}
		if (m_elapsed > 405)
		{
			failures.push_back("timed out waiting for scheduled restore");
			machine().schedule_exit();
		}
	}
	emu_timer *m_timer = nullptr;
	unsigned m_elapsed = 0, m_packets = 0, m_pass = 0, m_initial_audio = 0;
	bool m_loaded = false, m_saved = false, m_collect = false;
	attotime m_checkpoint_time;
};

ROM_START(cdicdasave)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdicdasave, 0, cdi_cdda_save, cdi_dma_integration,
	cdi_cdda_save_state, empty_init, ROT0, "MAME", "CD-i active CDDA save continuity fixture", MACHINE_SUPPORTS_SAVE)

TEST_CASE("CDIC active CDDA PCM Q and IRQ continuation survives scheduled save/load", "[emu][philips][cdic][pcm][transport][save][integration]")
{
	for (unsigned scenario = 0; scenario < 4; ++scenario)
	{
		CAPTURE(scenario);
		cdi_q_disc disc(0, 0, scenario == 3 ? 3 : 2);
		emu_options options;
		options.set_system_name(std::string(GAME_NAME(cdicdasave).name));
		cdi_dma_test_osd osd;
		cdi_dma_test_manager manager(options, osd);
		machine_config config(GAME_NAME(cdicdasave), options);
		running_machine machine(config, manager);
		manager.set_machine(&machine);
		auto &state = downcast<cdi_cdda_save_state &>(machine.root_device());
		state.image_path = disc.path();
		state.snapshot_path = (std::filesystem::path(disc.path()).parent_path() / "continuity.sta").string();
		state.first_lba = scenario == 2 ? 590 : 450;
		state.emphasis = scenario == 3;
		state.pending_irq = scenario == 1;
		state.checkpoint_ms = state.pending_irq ? 134 : 143;
		cdi_cdda_save_capture = &state;
		int const error = machine.run(true);
		cdi_cdda_save_capture = nullptr;
		manager.set_machine(nullptr);
		REQUIRE(error == EMU_ERR_NONE);
		for (auto const &failure : state.failures) { INFO(failure); CHECK(false); }
		REQUIRE(state.completed);
		CHECK(state.save_count == 1);
		CHECK(state.load_count == 1);
		REQUIRE(state.runs[0].registers.size() > 1500);
		CHECK(state.runs[0].registers == state.runs[1].registers);
		REQUIRE(state.runs[0].positions.size() >= 19);
		CHECK(state.runs[0].positions == state.runs[1].positions);
		REQUIRE(state.runs[0].sound_times.size() > 10);
		CHECK(state.runs[0].sound_times == state.runs[1].sound_times);
		for (unsigned channel = 0; channel < 2; ++channel)
		{
			CAPTURE(channel);
			auto const &live = state.runs[0].pcm[channel];
			auto const &restored = state.runs[1].pcm[channel];
			REQUIRE(live.size() > 10000);
			auto const nonzero = std::count_if(live.begin(), live.end(), [](float s) { return s != 0; });
			auto const mismatch = std::mismatch(live.begin(), live.end(), restored.begin(), restored.end());
			auto const first_difference = std::distance(live.begin(), mismatch.first);
			CAPTURE(live.size());
			CAPTURE(restored.size());
			CAPTURE(nonzero);
			CAPTURE(first_difference);
			CHECK(live.size() == restored.size());
			CHECK(nonzero > 1000);
			CHECK(std::adjacent_find(live.begin(), live.end(), std::not_equal_to<float>()) != live.end());
			CHECK((mismatch.first == live.end() && mismatch.second == restored.end()));
		}
		CHECK(state.runs[0].pcm[0] != state.runs[0].pcm[1]);
	}
}

void cdi_cdda_save_sound_hook(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
{
	if (cdi_cdda_save_capture) cdi_cdda_save_capture->capture(sound);
}

} // anonymous namespace
