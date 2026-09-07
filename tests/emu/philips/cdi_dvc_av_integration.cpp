// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Included after the full-machine, disc and sound-hook test support.
#include "cdi_dvc_av_reference_data.h"
#include <zlib.h>

namespace
{
class cdi_decoded_av_state;
cdi_decoded_av_state *cdi_decoded_av_capture = nullptr;

template <std::size_t N>
std::vector<uint8_t> cdi_av_inflate(std::array<uint8_t, N> const &data, unsigned size)
{
	std::vector<uint8_t> result(size);
	uLongf length = size;
	REQUIRE(uncompress(result.data(), &length, data.data(), data.size()) == Z_OK);
	REQUIRE(length == size);
	return result;
}

class cdi_decoded_av_state : public cdi_state
{
public:
	cdi_decoded_av_state(machine_config const &config, device_type type, char const *tag)
		: cdi_state(config, type, tag) { }
	void decoded_av(machine_config &config) { cdimono1dvc(config); }
	std::string snapshot_path;
	std::array<std::vector<uint8_t>, 2> rgb;
	std::vector<uint8_t> reference_pcm;
	bool completed = false;
	std::vector<std::string> failures;
	unsigned saves = 0, loads = 0, frames[2]{}, pcm_compared = 0;
	unsigned saved_frames = 0, saved_chunks = 0, restored_chunks = 0;
	unsigned max_rgb_error = 0, max_pcm_error = 0, max_lateness_ms = 0;
	uint64_t pcm_square_error = 0, reference_samples = 0;
	std::vector<std::array<int16_t, 2>> baseline_pcm;
	std::vector<std::array<uint32_t, 5>> baseline_events;
	std::vector<std::pair<int64_t, unsigned>> sound_times;
	std::vector<std::array<uint32_t, 3>> baseline_frames;

	void capture(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
	{
		auto const found = sound.find(":dvc");
		if (found == sound.end() || found->second.size() != 2 || !m_started) return;
		int const count = found->second[0].second;
		expect(count == found->second[1].second, "stereo callback lengths differ");
		// MAME renders through the current sample (inclusive), see sound_stream::update.
		int64_t const end = machine().time().as_ticks(44100) + 1;
		int64_t const start = end - count;
		std::pair<int64_t, unsigned> const block{end, unsigned(count)};
		if (!m_pass) sound_times.push_back(block);
		else
		{
			unsigned const index = saved_chunks + restored_chunks++;
			expect(index < sound_times.size() && sound_times[index] == block, "restored sound callback time/count differs");
		}
		if (!m_pass && end > int64_t(baseline_pcm.size())) baseline_pcm.resize(end);
		for (int i = 0; i < count; ++i)
		{
			int64_t const pos = start + i;
			if (pos < 0) continue;
			for (unsigned ch = 0; ch < 2; ++ch)
			{
				int const sample = std::lround(found->second[ch].first[i] * 32768.0f);
				if (!m_pass) baseline_pcm[pos][ch] = sample;
				else
				{
					expect(pos < int64_t(baseline_pcm.size()) && sample == baseline_pcm[pos][ch], "restored PCM differs at " + std::to_string(pos));
					++pcm_compared;
				}
				if (pos == 8820 + 112896)
					expect(sample == 0, "first scene reset did not leave the already-rendered boundary sample silent");
				if (pos >= 8820 && pos < 8821 + 12 * 112896 && pos != 8820 + 112896)
				{
					// At the first scene reset, the current sample is already rendered.
					// Subsequent scenes therefore start one sample later, with no cumulative drift.
					unsigned const relative = pos - 8820 - (pos > 8820 + 112896 ? 1 : 0);
					unsigned const offset = (relative % 112896) * 4 + ch * 2;
					int const reference = int16_t(unsigned(reference_pcm[offset]) | (unsigned(reference_pcm[offset + 1]) << 8));
					unsigned const error = std::abs(sample - reference);
					max_pcm_error = std::max(max_pcm_error, error);
					pcm_square_error += uint64_t(error) * error;
					++reference_samples;
				}
			}
		}
	}

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_timer = timer_alloc(FUNC(cdi_decoded_av_state::step), this);
		save_item(NAME(m_ms));
		save_item(NAME(m_scene));
		save_item(NAME(m_picture));
		save_item(NAME(m_last_hash));
		machine().save().register_presave(save_prepost_delegate(FUNC(cdi_decoded_av_state::presave), this));
		machine().save().register_postload(save_prepost_delegate(FUNC(cdi_decoded_av_state::postload), this));
		m_dvc->set_sound_hook(true);
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
		if (!ok && failures.size() < 30) failures.push_back(message);
	}
	void presave()
	{
		++saves;
		saved_frames = frames[0];
		saved_chunks = sound_times.size();
		expect(m_picture > 10, "save has no decoded presentation history");
	}
	void postload() { ++loads; expect(m_ms == 1037, "scheduled load did not restore the clock"); }
	static std::array<uint8_t, 5> timestamp(unsigned value)
	{
		return {uint8_t(0x21 | ((uint64_t(value) >> 29) & 14)), uint8_t(value >> 22),
			uint8_t((value >> 14) | 1), uint8_t(value >> 7), uint8_t((value << 1) | 1)};
	}
	void feed(bool audio, std::vector<uint8_t> const &bytes)
	{
		m_maincpu->space(AS_PROGRAM).write_word(audio ? 0xe03000 : 0xe040c0, audio ? 0x8000 : 0x9008);
		for (unsigned i = 0; i < bytes.size(); i += 2)
			m_dvc->dma_w((unsigned(bytes[i]) << 8) | (i + 1 < bytes.size() ? bytes[i + 1] : 0));
		m_dvc->dma_done();
	}
	void packet(bool audio, uint8_t const *data, unsigned length, bool first)
	{
		std::vector<uint8_t> bytes;
		if (first)
		{
			bytes = {0, 0, 1, 0xba, 0x21, 0, 1, 0, 1, 0x80, 0, 1}; // SCR zero
		}
		unsigned const size = length + (first ? 5 : 1);
		bytes.insert(bytes.end(), {0, 0, 1, uint8_t(audio ? 0xc0 : 0xe0), uint8_t(size >> 8), uint8_t(size)});
		if (first)
		{
			auto const pts = timestamp(m_scene == 0 ? 18000 : 0);
			bytes.insert(bytes.end(), pts.begin(), pts.end());
		}
		else bytes.push_back(0x0f);
		bytes.insert(bytes.end(), data, data + length);
		feed(audio, bytes);
	}
	void begin_scene(address_space &space)
	{
		space.write_word(0xe0301c, 0xffff);
		space.write_word(0xe04060, 0xffff);
		space.write_word(0xe03000, 1); // audio reset for an independently encoded scene
		// FMA powers up muted. Program straight-through gain through the real port.
		space.write_word(0xe03022, 0); space.write_word(0xe03024, 0x80);
		space.write_word(0xe03022, 1); space.write_word(0xe03024, 0x93);
		space.write_word(0xe03022, 7);
		for (uint8_t value : {0, 0x80, 0x80, 0}) space.write_word(0xe03024, value);
		space.write_word(0xe040c0, 0x0100); // clear video FIFO/decoder
		space.write_word(0xe040c6, 4); // timestamp-controlled presentation
		space.write_word(0xe04078, 32);
		space.write_word(0xe0407a, 32);
		space.write_word(0xe040c2, 0x0228); // video on, show, latch geometry
		if (m_scene & 1)
			packet(false, cdi_av_reference::VIDEO_1.data(), cdi_av_reference::VIDEO_1.size(), true);
		else
			packet(false, cdi_av_reference::VIDEO_0.data(), cdi_av_reference::VIDEO_0.size(), true);
		unsigned offset = 0;
		for (unsigned i = 0; i < 98; ++i)
		{
			// Fixed fixture profile: MPEG-1 Layer II, 192 kbit/s, 44.1 kHz.
			unsigned const size = 144 * 192000 / 44100 + ((cdi_av_reference::AUDIO[offset + 2] >> 1) & 1);
			packet(true, cdi_av_reference::AUDIO.data() + offset, size, i == 0);
			offset += size;
		}
		expect(offset == cdi_av_reference::AUDIO.size(), "audio fixture frame count differs");
		feed(true, {0, 0, 1, 0xb9});
		m_picture = 0;
	}
	void observe_frame()
	{
		std::array<uint32_t, 1024> pixels;
		std::array<uint32_t, 64> row;
		bool external[64];
		std::fill_n(external, 64, true);
		uint32_t hash = 2166136261U;
		for (unsigned y = 0; y < 32; ++y)
		{
			row.fill(0x00123456);
			m_dvc->video_overlay_scanline(row.data(), 64, y * 2, 0, 0, 63, external, 64);
			for (unsigned x = 0; x < 32; ++x)
			{
				pixels[y * 32 + x] = row[x * 2];
				hash = (hash ^ row[x * 2]) * 16777619U;
			}
		}
		if (pixels[0] == 0x00123456 || hash == m_last_hash) return;
		m_last_hash = hash;
		if (m_picture >= 64) { expect(false, "extra presented frame"); return; }
		unsigned const due = 200 + m_scene * 2560 + m_picture * 40;
		expect(m_ms >= due && m_ms <= due + 21, "frame timing mismatch scene=" + std::to_string(m_scene) + " frame=" + std::to_string(m_picture) + " ms=" + std::to_string(m_ms));
		if (m_ms >= due) max_lateness_ms = std::max(max_lateness_ms, m_ms - due);
		unsigned frame_error = 0;
		for (unsigned i = 0; i < pixels.size(); ++i)
			for (unsigned ch = 0; ch < 3; ++ch)
			{
				int const actual = uint8_t(pixels[i] >> (16 - ch * 8));
				int const reference = rgb[m_scene & 1][(m_picture * 1024 + i) * 3 + ch];
				frame_error = std::max(frame_error, unsigned(std::abs(actual - reference)));
			}
		max_rgb_error = std::max(max_rgb_error, frame_error);
		expect(frame_error <= 3, "RGB mismatch scene=" + std::to_string(m_scene) + " frame=" + std::to_string(m_picture) + " error=" + std::to_string(frame_error));
		std::array<uint32_t, 3> const record{m_ms, m_scene * 64 + m_picture, hash};
		if (!m_pass) baseline_frames.push_back(record);
		else expect(std::find(baseline_frames.begin(), baseline_frames.end(), record) != baseline_frames.end(), "restored frame/time differs");
		++frames[m_pass];
		++m_picture;
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		m_timer->adjust(attotime::from_msec(1));
		auto &space = m_maincpu->space(AS_PROGRAM);
		if (!m_started) { m_started = true; begin_scene(space); return; }
		++m_ms;
		if (m_ms >= 2760 && m_ms < 200 + 12 * 2560 && (m_ms - 200) % 2560 == 0)
		{
			expect(m_picture == 64, "scene ended before all reference frames were presented: " + std::to_string(m_picture));
			++m_scene;
			begin_scene(space);
		}
		observe_frame();
		bool const irq_before = m_dvc_irq_state;
		std::array<uint32_t, 5> event{m_ms, space.read_word(0xe0301a), space.read_word(0xe04062), uint32_t(irq_before), uint32_t(m_dvc_irq_state)};
		expect(irq_before == bool(event[1] | event[2]) && !m_dvc_irq_state, "DVC IRQ assertion/acknowledgement disagrees with enabled status");
		if (!m_pass) baseline_events.push_back(event);
		else expect(event == baseline_events[m_ms - 1], string_format("restored IRQ events differ at %u: %04x/%04x expected %04x/%04x", m_ms, event[1], event[2], baseline_events[m_ms - 1][1], baseline_events[m_ms - 1][2]));
		if (!saves && m_ms == 1037) machine().schedule_save(std::string(snapshot_path));
		if (m_ms == 31000)
		{
			expect(m_scene == 11 && m_picture == 64, "last scene incomplete");
			if (!m_pass) { m_pass = 1; machine().schedule_load(std::string(snapshot_path)); }
			else { completed = true; machine().schedule_exit(); }
		}
		if (m_ms > 31005) { expect(false, "load did not complete"); machine().schedule_exit(); }
	}
	emu_timer *m_timer = nullptr;
	unsigned m_ms = 0, m_scene = 0, m_picture = 0, m_pass = 0;
	uint32_t m_last_hash = 0;
	bool m_started = false;
};

ROM_START(cdidecav)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END
GAME(2026, cdidecav, 0, decoded_av, cdi_dma_integration, cdi_decoded_av_state, empty_init,
	ROT0, "MAME", "CD-i decoded A/V reference continuation fixture", MACHINE_SUPPORTS_SAVE)

TEST_CASE("DVC decoded I P B frames and stereo PCM survive sustained scenes and scheduled save-load", "[emu][philips][dvc][decoded][save][integration]")
{
	cdi_q_disc temp;
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdidecav).name));
	options.set_value(OPTION_THROTTLE, 0, OPTION_PRIORITY_MAXIMUM);
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdidecav), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);
	auto &state = downcast<cdi_decoded_av_state &>(machine.root_device());
	state.snapshot_path = (std::filesystem::path(temp.path()).parent_path() / "continuity.sta").string();
	state.rgb[0] = cdi_av_inflate(cdi_av_reference::RGB_0_Z, 64 * 1024 * 3);
	state.rgb[1] = cdi_av_inflate(cdi_av_reference::RGB_1_Z, 64 * 1024 * 3);
	state.reference_pcm = cdi_av_inflate(cdi_av_reference::PCM_Z, 98 * 1152 * 4);
	cdi_decoded_av_capture = &state;
	int const error = machine.run(true);
	cdi_decoded_av_capture = nullptr;
	manager.set_machine(nullptr);
	REQUIRE(error == EMU_ERR_NONE);
	for (auto const &failure : state.failures) { INFO(failure); CHECK(false); }
	REQUIRE(state.completed);
	REQUIRE(state.reference_samples > 5'000'000);
	CAPTURE(state.max_rgb_error);
	CAPTURE(state.max_pcm_error);
	CAPTURE(state.max_lateness_ms);
	CAPTURE(state.frames[1]);
	CAPTURE(state.pcm_compared);
	CAPTURE(state.reference_samples);
	CAPTURE(state.pcm_square_error / state.reference_samples);
	CHECK(state.frames[0] == 768);
	CHECK(state.frames[1] == state.frames[0] - state.saved_frames);
	CHECK(state.restored_chunks == state.sound_times.size() - state.saved_chunks);
	CHECK(state.saves == 1);
	CHECK(state.loads == 1);
	CHECK(state.pcm_compared > 2'500'000);
	CHECK(state.max_pcm_error <= 600);
	CHECK(state.pcm_square_error / state.reference_samples <= 150 * 150);
}

void cdi_decoded_av_sound_hook(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
{
	if (cdi_decoded_av_capture) cdi_decoded_av_capture->capture(sound);
}
} // anonymous namespace
