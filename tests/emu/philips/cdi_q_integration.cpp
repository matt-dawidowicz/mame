// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Included after the shared full-machine OSD/manager fixture definitions.
#include <filesystem>
#include <fstream>

namespace
{

// No retail assets: twelve tracks in shared/separate BINs, with stored/virtual
// pregaps. Track 2 INDEX 01 is always LBA 450; tracks 3-12 are Mode 1.
// Layout: 0 shared/stored, 1 separate/stored, 2 separate/virtual, 3 shared/virtual.
class cdi_q_disc
{
public:
	cdi_q_disc(int subcode = 0, unsigned layout = 0, unsigned pcm = 0)
	{
		m_dir = std::filesystem::temp_directory_path() / ("mame-cdi-q-" + std::to_string(osd_ticks()));
		REQUIRE(std::filesystem::create_directory(m_dir));
		std::ofstream bin(m_dir / "disc.bin", std::ios::binary);
		for (unsigned lba = 0; lba < 1350; ++lba)
		{
			if ((layout == 1 && (lba == 300 || lba == 600)) || (layout == 2 && (lba == 450 || lba == 600)))
			{
				REQUIRE(bin.good());
				bin.close();
				bin.open(m_dir / (lba == 600 ? "data.bin" : "audio.bin"), std::ios::binary);
			}
			if (layout >= 2 && lba >= 300 && lba < 450)
				continue; // virtual pregap consumes logical time, no file bytes
			std::array<uint8_t, 2352> sector{};
			bool const pcm_audio = lba < 600 || (lba >= 675 && lba < 750);
			if (pcm) sector.fill(pcm_audio ? 0x11 : 0x55);
			if (pcm >= 2 && pcm_audio)
				for (unsigned frame = 0; frame < 588; ++frame)
				{
					// Changing, distinct stereo words; paired bytes are endian-independent.
					sector[frame * 4] = sector[frame * 4 + 1] = 1 + (lba * 7 + frame) % 63;
					sector[frame * 4 + 2] = sector[frame * 4 + 3] = 0x81 + (lba * 11 + frame * 3) % 63;
				}
			// Paired bytes survive the audio sample endian conversion.
			if (!pcm)
			{
				sector[32] = sector[33] = uint8_t(lba >> 8);
				sector[34] = sector[35] = uint8_t(lba);
			}
			if (lba >= 600 && !(pcm && pcm_audio))
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
		auto index = [&cue](unsigned number, unsigned frame)
		{
			cue << string_format("    INDEX %02u %02u:%02u:%02u\n", number, frame / 4500, frame / 75 % 60, frame % 75);
		};
		cue << "FILE \"disc.bin\" BINARY\n"
			<< "  TRACK 01 AUDIO" << format << "\n";
		index(1, 0);
		index(2, 10);
		if (layout == 1 || layout == 2) cue << "FILE \"audio.bin\" BINARY\n";
		cue << "  TRACK 02 AUDIO" << format << "\n";
		if (pcm == 3) cue << "    FLAGS PRE\n";
		if (layout >= 2) cue << "    PREGAP 00:02:00\n";
		else index(0, layout == 1 ? 0 : 300);
		unsigned const origin = layout == 0 ? 450 : layout == 1 ? 150 : layout == 2 ? 0 : 300;
		index(1, origin);
		for (unsigned i = 2; i <= 12; ++i) index(i, origin + i + 2);
		if (layout == 1 || layout == 2) cue << "FILE \"data.bin\" BINARY\n";
		for (unsigned track = 3; track <= 12; ++track)
		{
			if (pcm && track == 4)
				cue << "  TRACK 04 AUDIO\n";
			else
				cue << string_format("  TRACK %02u MODE1/2352%s\n    FLAGS DCP\n", track, format);
			unsigned const frame = (layout == 0 ? 600 : layout == 3 ? 450 : 0) + 75 * (track - 3);
			index(1, frame);
			if (track == 3) index(2, frame + 2);
		}
		REQUIRE(cue.good());
	}

	~cdi_q_disc()
	{
		std::error_code error;
		std::filesystem::remove(m_dir / "disc.cue", error);
		std::filesystem::remove(m_dir / "disc.bin", error);
		std::filesystem::remove(m_dir / "audio.bin", error);
		std::filesystem::remove(m_dir / "data.bin", error);
		std::filesystem::remove(m_dir / "disc.chd", error);
		std::filesystem::remove(m_dir / "continuity.sta", error);
		std::filesystem::remove(m_dir, error);
	}

	std::string path() const { return (m_dir / "disc.cue").string(); }
	static unsigned expected_index(unsigned lba)
	{
		if (lba < 300) return lba < 10 ? 1 : 2;
		if (lba < 450) return 0;
		if (lba < 454) return 1;
		if (lba < 600) return std::min(12U, lba - 452);
		return lba >= 602 && lba < 675 ? 2 : 1;
	}
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

void run_cdi_q_fixture(int subcode, bool toc = false, unsigned layout = 0)
{
	cdi_q_disc disc(subcode, layout);
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
			CHECK(result.generic_track == track - 1);
			CHECK(result.generic_index == cdi_q_disc::expected_index(result.lba));
			CHECK(result.q[0] == (track <= 2 ? 0x01 : 0x61));
			CHECK(result.q[1] == cdi_q_disc::bcd(track));
			bool const stored_q = subcode == 2 && !(layout >= 2 && pregap);
			unsigned const index = stored_q ? (pregap ? 0 : track == 1 && result.lba >= 10 ? 2 : 1)
				: cdi_q_disc::expected_index(result.lba);
			CHECK(result.q[2] == cdi_q_disc::bcd(index));
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

TEST_CASE("Generic CUE track/index and payload mapping across file and pregap layouts", "[cdrom][cue][integration]")
{
	for (unsigned layout = 0; layout < 4; ++layout)
	{
		CAPTURE(layout);
		cdi_q_disc fixture(2, layout);
		cdrom_file disc(fixture.path());
		REQUIRE(disc.get_track_start(1) == 450);
		REQUIRE(disc.get_track_start(2) == 600);
		REQUIRE(disc.get_track_start(0xaa) == 1350);
		for (unsigned lba : {0U, 9U, 10U, 299U, 300U, 449U, 450U, 453U, 454U, 463U, 464U, 599U, 600U, 601U, 602U, 674U, 675U, 1349U})
		{
			CAPTURE(lba);
			unsigned const track = lba < 300 ? 0 : lba < 600 ? 1 : 2 + (lba - 600) / 75;
			CHECK(disc.get_track(lba) == track);
			CHECK(disc.get_track_index(lba) == cdi_q_disc::expected_index(lba));
			std::array<uint8_t, 2352> data;
			data.fill(0xcd);
			REQUIRE(disc.read_data(lba, data.data(), cdrom_file::CD_TRACK_RAW_DONTCARE));
			bool const virtual_gap = layout >= 2 && lba >= 300 && lba < 450;
			if (virtual_gap)
				CHECK(std::all_of(data.begin(), data.end(), [](uint8_t v) { return v == 0; }));
			else
			{
				CHECK(data[32] == uint8_t(lba >> 8));
				CHECK(data[34] == uint8_t(lba));
			}
			std::array<uint8_t, 96> sub;
			sub.fill(0xcd);
			REQUIRE(disc.read_subcode(lba, sub.data()));
			if (virtual_gap)
				CHECK(std::all_of(sub.begin(), sub.end(), [](uint8_t v) { return v == 0; }));
			else
			{
				// Q absolute frame byte proves subcode and data use the same sector.
				uint8_t frame = 0;
				for (unsigned bit = 72; bit < 80; ++bit) frame = (frame << 1) | ((sub[bit] >> 6) & 1);
				CHECK(frame == cdi_q_disc::bcd(lba % 75));
			}
		}
		// Physical extraction must include stored gaps and omit virtual gaps.
		for (unsigned physical : {299U, 300U, 449U, 450U, 599U, 600U, 1199U})
		{
			CAPTURE(physical);
			unsigned const logical = physical + (layout >= 2 && physical >= 300 ? 150 : 0);
			std::array<uint8_t, 2352> data{};
			REQUIRE(disc.read_data(physical, data.data(), cdrom_file::CD_TRACK_RAW_DONTCARE, true));
			CHECK(data[32] == uint8_t(logical >> 8));
			CHECK(data[34] == uint8_t(logical));
		}
	}
}

TEST_CASE("CDIC carries CUE higher indexes and pregaps across image layouts", "[emu][philips][cdic][q][cue][integration]")
{
	for (unsigned layout = 1; layout < 4; ++layout)
		for (int subcode : {0, 2})
		{
			CAPTURE(layout);
			CAPTURE(subcode);
			run_cdi_q_fixture(subcode, false, layout);
		}
}

TEST_CASE("Generic CD-ROM rejects truncated payload and subcode reads", "[cdrom][cue][integration]")
{
	for (bool subcode : {false, true})
	{
		CAPTURE(subcode);
		cdi_q_disc fixture(2);
		cdrom_file disc(fixture.path());
		// Truncate after opening so the TOC still declares the final sector.
		std::filesystem::resize_file(std::filesystem::path(fixture.path()).parent_path() / "disc.bin",
			uint64_t(1349) * 2448 + (subcode ? 2352 + 32 : 32));
		std::array<uint8_t, 2352> data{};
		if (subcode) CHECK_FALSE(disc.read_subcode(1349, data.data()));
		else CHECK_FALSE(disc.read_data(1349, data.data(), cdrom_file::CD_TRACK_RAW_DONTCARE));
	}
}

TEST_CASE("Generic CHD mapping preserves stored gaps and skips per-track padding", "[cdrom][chd][integration]")
{
	for (bool virtual_gap : {false, true})
	{
		CAPTURE(virtual_gap);
		cdi_q_disc fixture(2, virtual_gap ? 3 : 0);
		auto const directory = std::filesystem::path(fixture.path()).parent_path();
		std::ifstream bin(directory / "disc.bin", std::ios::binary);
		chd_file chd;
		chd_codec_type const compression[4] = { CHD_CODEC_NONE };
		unsigned const storage_frames = virtual_gap ? 1212 : 1360;
		REQUIRE_FALSE(chd.create((directory / "disc.chd").string(), uint64_t(storage_frames) * 2448, 4 * 2448, 2448, compression));
		unsigned storage = 0;
		for (unsigned track = 0; track < 12; ++track)
		{
			unsigned const frames = track == 0 ? 300 : track == 1 ? (virtual_gap ? 150 : 300) : 75;
			std::string const metadata = string_format(
				"TRACK:%u TYPE:%s SUBTYPE:RW_RAW FRAMES:%u PREGAP:%u PGTYPE:%s PGSUB:RW_RAW POSTGAP:0",
				track + 1, track < 2 ? "AUDIO" : "MODE1_RAW", frames,
				track == 1 ? 150 : 0, virtual_gap ? "AUDIO" : "VAUDIO");
			REQUIRE_FALSE(chd.write_metadata(CDROM_TRACK_METADATA2_TAG, track, metadata));
			for (unsigned frame = 0; frame < (frames + 3) / 4 * 4; ++frame)
			{
				std::array<uint8_t, 2448> data;
				data.fill(0xd7); // padding must never be returned as track data
				if (frame < frames)
				{
					bin.read(reinterpret_cast<char *>(data.data()), data.size());
					REQUIRE(bin.good());
				}
				REQUIRE_FALSE(chd.write_bytes(uint64_t(storage++) * data.size(), data.data(), data.size()));
			}
		}
		REQUIRE(storage == storage_frames);
		cdrom_file disc(&chd);
		REQUIRE(disc.get_track_start(1) == 450);
		REQUIRE(disc.get_track_start(0xaa) == 1350);
		for (unsigned logical : {299U, 300U, 449U, 450U, 599U, 600U, 674U, 675U, 1275U, 1349U})
		{
			CAPTURE(logical);
			unsigned const track = logical < 300 ? 0 : logical < 600 ? 1 : 2 + (logical - 600) / 75;
			CHECK(disc.get_track(logical) == track);
			bool const pregap = logical >= 300 && logical < 450;
			// CHD track metadata stores INDEX 00/01 only; raw Q is separate.
			CHECK(disc.get_track_index(logical) == (pregap ? 0 : 1));
			std::array<uint8_t, 2352> data{};
			REQUIRE(disc.read_data(logical, data.data(), cdrom_file::CD_TRACK_RAW_DONTCARE));
			CHECK(data[32] == (virtual_gap && pregap ? 0 : uint8_t(logical >> 8)));
			CHECK(data[34] == (virtual_gap && pregap ? 0 : uint8_t(logical)));
			std::array<uint8_t, 96> sub{};
			REQUIRE(disc.read_subcode(logical, sub.data()));
			uint8_t frame = 0;
			for (unsigned bit = 72; bit < 80; ++bit) frame = (frame << 1) | ((sub[bit] >> 6) & 1);
			CHECK(frame == (virtual_gap && pregap ? 0 : cdi_q_disc::bcd(logical % 75)));
			if (!(virtual_gap && pregap))
			{
				unsigned const physical = logical - (virtual_gap && logical >= 450 ? 150 : 0);
				std::array<uint8_t, 2352> physical_data{};
				REQUIRE(disc.read_data(physical, physical_data.data(), cdrom_file::CD_TRACK_RAW_DONTCARE, true));
				CHECK(data == physical_data);
				std::array<uint8_t, 96> physical_sub{};
				REQUIRE(disc.read_subcode(physical, physical_sub.data(), true));
				CHECK(sub == physical_sub);
			}
		}
	}
}

} // anonymous namespace
