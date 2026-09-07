// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Included after the shared full-machine OSD/manager fixture definitions.
#include <filesystem>
#include <fstream>

namespace
{

// No retail assets: twelve tracks in a generated shared BIN/CUE. Track 2 has
// a stored 150-sector index-0 pregap. Tracks 3-12 are Mode 1, copy permitted.
class cdi_q_disc
{
public:
	cdi_q_disc(int subcode = 0)
	{
		m_dir = std::filesystem::temp_directory_path() / ("mame-cdi-q-" + std::to_string(osd_ticks()));
		REQUIRE(std::filesystem::create_directory(m_dir));
		std::ofstream bin(m_dir / "disc.bin", std::ios::binary);
		for (unsigned lba = 0; lba < 1350; ++lba)
		{
			std::array<uint8_t, 2352> sector{};
			if (lba >= 600)
			{
				std::fill(sector.begin() + 1, sector.begin() + 11, 0xff);
				unsigned const absolute = lba + 150;
				sector[12] = bcd(absolute / 4500);
				sector[13] = bcd(absolute / 75 % 60);
				sector[14] = bcd(absolute % 75);
				sector[15] = 1;
			}
			bin.write(reinterpret_cast<char const *>(sector.data()), sector.size());
			if (subcode)
			{
				unsigned const track = lba < 300 ? 1 : lba < 600 ? 2 : 3 + (lba - 600) / 75;
				unsigned const start = track == 1 ? 0 : track == 2 ? 450 : 600 + (track - 3) * 75;
				unsigned const relative = lba < start ? start - lba : lba - start;
				std::array<uint8_t, 12> q{
					uint8_t(track <= 2 ? 0x01 : 0x61), bcd(track),
					uint8_t(lba < start ? 0 : track == 1 && lba >= 10 ? 2 : 1),
					bcd(relative / 4500), bcd(relative / 75 % 60), bcd(relative % 75), 0,
					bcd((lba + 150) / 4500), bcd((lba + 150) / 75 % 60), bcd((lba + 150) % 75), 0, 0 };
				uint16_t crc = 0;
				for (unsigned i = 0; i < 10; ++i)
				{
					crc ^= q[i] << 8;
					for (unsigned bit = 0; bit < 8; ++bit)
						crc = (crc << 1) ^ (crc & 0x8000 ? 0x1021 : 0);
				}
				q[10] = uint8_t(~crc >> 8);
				q[11] = uint8_t(~crc);
				std::array<uint8_t, 96> sub{};
				if (subcode == 1) // cooked R-W is not P-W
					std::fill(sub.begin(), sub.end(), 0x2d); // packed R-W, no Q
				else // one PQRSTUVW symbol per byte; Q is bit 6
					for (unsigned bit = 0; bit < 96; ++bit)
						sub[bit] = 0xbf | (BIT(q[bit / 8], 7 - bit % 8) << 6);
				if (subcode == 3) sub[95] ^= 0x40; // bad CRC
				if (subcode == 4) for (auto &symbol : sub) symbol &= ~0x40; // Q absent
				bin.write(reinterpret_cast<char const *>(sub.data()), sub.size());
			}
		}
		REQUIRE(bin.good());
		bin.close();
		std::ofstream cue(m_dir / "disc.cue");
		char const *format = subcode == 1 ? " RW" : subcode >= 2 ? " RW_RAW" : "";
		cue << "FILE \"disc.bin\" BINARY\n"
			<< "  TRACK 01 AUDIO" << format << "\n    INDEX 01 00:00:00\n"
			<< "  TRACK 02 AUDIO" << format << "\n    INDEX 00 00:04:00\n    INDEX 01 00:06:00\n";
		for (unsigned track = 3; track <= 12; ++track)
			cue << string_format("  TRACK %02u MODE1/2352%s\n    FLAGS DCP\n    INDEX 01 00:%02u:00\n", track, format, track + 5);
		REQUIRE(cue.good());
	}

	~cdi_q_disc()
	{
		std::error_code error;
		std::filesystem::remove(m_dir / "disc.cue", error);
		std::filesystem::remove(m_dir / "disc.bin", error);
		std::filesystem::remove(m_dir, error);
	}

	std::string path() const { return (m_dir / "disc.cue").string(); }
	static uint8_t bcd(unsigned value) { return (value / 10) * 16 + value % 10; }

private:
	std::filesystem::path m_dir;
};

struct cdi_q_observation
{
	unsigned lba;
	unsigned generic_track;
	unsigned generic_index;
	std::array<uint16_t, 12> q;
};

class cdi_q_integration_state : public cdi_state
{
public:
	cdi_q_integration_state(machine_config const &config, device_type type, char const *tag)
		: cdi_state(config, type, tag) { }
	void cdi_q_integration(machine_config &config) { cdimono1(config); }
	std::string image_path;
	bool toc_test = false;
	std::vector<cdi_q_observation> observations;
	std::vector<std::string> failures;
	bool completed = false;

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_timer = timer_alloc(FUNC(cdi_q_integration_state::step), this);
	}
	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		m_timer->adjust(attotime::zero);
	}

private:
	struct span { unsigned start, count; };
	static constexpr span SPANS[] = { {0, 14}, {298, 5}, {448, 4}, {598, 4}, {1348, 2}, {463, 2}, {600, 2} };

	void start_span(address_space &space)
	{
		m_received = 0;
		m_wait = 0;
		unsigned const absolute = SPANS[m_span].start + 150;
		uint32_t const time = (uint32_t(cdi_q_disc::bcd(absolute / 4500)) << 24)
			| (uint32_t(cdi_q_disc::bcd(absolute / 75 % 60)) << 16)
			| (uint32_t(cdi_q_disc::bcd(absolute % 75)) << 8);
		space.write_word(0x303ffe, 0); // cancel/reset buffer ownership
		space.read_word(0x303ff6); // acknowledge old XBUF
		space.write_word(0x303c00, toc_test ? 0x27 : m_span == 6 ? 0x29 : 0x28);
		space.write_dword(0x303c02, time);
		space.write_word(0x303ffe, 0xc000); // execute, keep disc engine enabled
	}

	TIMER_CALLBACK_MEMBER(step)
	{
		address_space &space = m_maincpu->space(AS_PROGRAM);
		if (!m_loaded)
		{
			auto const result = m_cdrom->load(image_path);
			if (result.first)
			{
				failures.push_back("image load: " + result.second);
				machine().schedule_exit();
				return;
			}
			m_loaded = true;
			if (m_cdrom->get_last_track() != 12 || m_cdrom->get_track_start(1) != 450
				|| m_cdrom->get_toc().tracks[1].pregap != 150 || m_cdrom->get_track_start(0xaa) != 1350)
				failures.push_back("generic CUE metadata does not match the constructed disc");
			start_span(space);
		}
		else if (m_end_wait)
		{
			// No readable lead-out sector exists in the image. Preserve the last
			// delivered Q, and never expose a fabricated track-1 packet after EOF.
			if (space.read_word(0x303ff6) & 0x8000)
				failures.push_back("unexpected Q delivery at/beyond the lead-out boundary");
			if (++m_wait == 45)
			{
				m_end_wait = false;
				start_span(space);
			}
			m_timer->adjust(attotime::from_msec(1));
			return;
		}
		else if (space.read_word(0x303ff6) & 0x8000)
		{
			unsigned const lba = toc_test ? m_received : SPANS[m_span].start + m_received;
			cdi_q_observation result{ lba, m_cdrom->get_track(lba), m_cdrom->get_track_index(lba), {} };
			unsigned const base = (space.read_word(0x303ffe) & 1) * 0xa00 + 0x924;
			for (unsigned i = 0; i < result.q.size(); ++i)
				result.q[i] = space.read_word(0x300000 + base + 2 * i);
			observations.push_back(result);
			if (++m_received == (toc_test ? 45 : SPANS[m_span].count))
			{
				if (toc_test || ++m_span == std::size(SPANS))
				{
					completed = true;
					machine().schedule_exit();
					return;
				}
				if (m_span == 5)
				{
					m_end_wait = true;
					m_wait = 0;
				}
				else
					start_span(space);
			}
			else
				m_wait = 0;
		}
		if (++m_wait > 300)
		{
			failures.push_back(string_format("no sector delivery: span %u sector %u", m_span, m_received));
			machine().schedule_exit();
			return;
		}
		m_timer->adjust(attotime::from_msec(1));
	}

	emu_timer *m_timer = nullptr;
	bool m_loaded = false;
	bool m_end_wait = false;
	unsigned m_span = 0, m_received = 0, m_wait = 0;
};

ROM_START(cdiqtest)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdiqtest, 0, cdi_q_integration, cdi_dma_integration,
	cdi_q_integration_state, empty_init, ROT0, "MAME", "CD-i synthetic Q transport fixture", 0)

void run_cdi_q_fixture(int subcode, bool toc = false)
{
	cdi_q_disc disc(subcode);
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdiqtest).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdiqtest), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);
	auto &state = downcast<cdi_q_integration_state &>(machine.root_device());
	state.image_path = disc.path();
	state.toc_test = toc;
	int const error = machine.run(true);
	manager.set_machine(nullptr);
	REQUIRE(error == EMU_ERR_NONE);
	for (auto const &failure : state.failures) { INFO(failure); CHECK(false); }
	REQUIRE(state.completed);
	REQUIRE(state.observations.size() == (toc ? 45 : 33));
	for (auto const &result : state.observations)
	{
		CAPTURE(result.lba);
		CAPTURE(result.generic_index);
		if (toc)
		{
			unsigned const entry = result.lba / 3;
			unsigned const start = entry == 0 ? 0 : entry == 1 ? 450 : 600 + (entry - 2) * 75;
			unsigned const position = (entry < 12 ? start : 1350) + 150;
			std::array<uint8_t, 10> expected{
				uint8_t(entry < 2 || entry == 12 ? 0x01 : 0x61), 0,
				uint8_t(entry < 12 ? cdi_q_disc::bcd(entry + 1) : 0xa0 + entry - 12),
				0, cdi_q_disc::bcd((result.lba + 150) / 75), cdi_q_disc::bcd(result.lba % 75), 0,
				cdi_q_disc::bcd(position / 4500), cdi_q_disc::bcd(position / 75 % 60), cdi_q_disc::bcd(position % 75) };
			if (entry == 12) { expected[7] = 1; expected[8] = 0x10; expected[9] = 0; }
			if (entry == 13) { expected[7] = 0x12; expected[8] = 0; expected[9] = 0; }
			for (unsigned i = 0; i < expected.size(); ++i)
			{
				CAPTURE(i);
				CHECK(result.q[i] == expected[i]);
			}
		}
		else
		{
			unsigned const track = result.lba < 300 ? 1 : result.lba < 600 ? 2 : 3 + (result.lba - 600) / 75;
			unsigned const start = track == 1 ? 0 : track == 2 ? 450 : 600 + (track - 3) * 75;
			bool const pregap = result.lba < start;
			unsigned const relative = pregap ? start - result.lba : result.lba - start;
			unsigned const absolute = result.lba + 150;
			// Generic get_track assigns the next track's pregap to the preceding
			// track for sector access. Q ownership is instead specified by the TOC gap.
			CHECK(result.generic_track == (pregap ? 0 : track - 1));
			CHECK(result.q[0] == (track <= 2 ? 0x01 : 0x61));
			CHECK(result.q[1] == cdi_q_disc::bcd(track));
			CHECK(result.q[2] == (pregap ? 0 : subcode == 2 && track == 1 && result.lba >= 10 ? 2 : 1));
			CHECK(result.q[3] == cdi_q_disc::bcd(relative / 4500));
			CHECK(result.q[4] == cdi_q_disc::bcd(relative / 75 % 60));
			CHECK(result.q[5] == cdi_q_disc::bcd(relative % 75));
			CHECK(result.q[6] == 0);
			CHECK(result.q[7] == cdi_q_disc::bcd(absolute / 4500));
			CHECK(result.q[8] == cdi_q_disc::bcd(absolute / 75 % 60));
			CHECK(result.q[9] == cdi_q_disc::bcd(absolute % 75));
		}
		// Independent bitwise ECMA-130 CRC oracle, ten payload bytes, inverted.
		uint16_t crc = 0;
		for (unsigned i = 0; i < 10; ++i)
		{
			crc ^= result.q[i] << 8;
			for (unsigned bit = 0; bit < 8; ++bit)
				crc = (crc << 1) ^ (crc & 0x8000 ? 0x1021 : 0);
		}
		CHECK(result.q[10] == uint8_t(~crc >> 8));
		CHECK(result.q[11] == uint8_t(~crc));
	}
}

TEST_CASE("CDIC reports disc track/index and relative/absolute Q through live SRAM", "[emu][philips][cdic][q][integration]")
{
	run_cdi_q_fixture(0);
}

TEST_CASE("CDIC does not interpret cooked R-W bytes as Q", "[emu][philips][cdic][q][integration]")
{
	run_cdi_q_fixture(1);
}

TEST_CASE("CDIC extracts raw interleaved stored Q and higher indexes", "[emu][philips][cdic][q][integration]")
{
	run_cdi_q_fixture(2);
}

TEST_CASE("CDIC falls back to metadata on corrupt raw Q CRC", "[emu][philips][cdic][q][integration]")
{
	run_cdi_q_fixture(3);
}

TEST_CASE("CDIC falls back to metadata when raw subcode omits Q", "[emu][philips][cdic][q][integration]")
{
	run_cdi_q_fixture(4);
}

TEST_CASE("CDIC TOC includes every track and the complete absolute lead-out", "[emu][philips][cdic][q][toc][integration]")
{
	run_cdi_q_fixture(0, true);
}

} // anonymous namespace
