// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Included after the existing decoded A/V and full-machine test support.
#include "cdi_dvc_motion_reference_data.h"
#include "cdi_dvc_full_reference_data.h"
#include "cdi_full_reference_decode.h"
#include "screen.h"
#include <cstdlib>

namespace
{
class cdi_motion_state;
cdi_motion_state *cdi_motion_capture = nullptr;

class cdi_motion_state : public cdi_state
{
  public:
	cdi_motion_state(machine_config const &config, device_type type, char const *tag) : cdi_state(config, type, tag)
	{
	}
	void motion(machine_config &config)
	{
		cdimono1dvc(config);
		auto &screen = *subdevice<screen_device>("screen");
		screen.set_video_attributes(VIDEO_UPDATE_SCANLINE | VIDEO_ALWAYS_UPDATE);
		screen.set_screen_update(FUNC(cdi_motion_state::render));
	}
	unsigned initial_profile = 0, long_seconds = 0;
	bool capacity_mode = false, synchronized_branches = false, deferred_video = false;
	unsigned display_mode = 0, ingress_mode = 0;
	struct delivery
	{
		unsigned ms;
		bool audio;
		std::vector<uint8_t> bytes;
	};
	std::vector<delivery> deliveries;
	std::vector<int64_t> presentation_times;
	void prepare_ingress()
	{
		// Timestamp origin is independent of delivery time. Mode 3 crosses 33-bit wrap.
		uint64_t const origin = ingress_mode == 3 ? (1ULL << 33) - 18000 : 0;
		auto timestamp = [](std::vector<uint8_t> &bytes, uint64_t value)
		{
			bytes.insert(bytes.end(), {uint8_t(0x21 | ((value >> 29) & 14)), uint8_t(value >> 22),
									   uint8_t((value >> 14) | 1), uint8_t(value >> 7), uint8_t((value << 1) | 1)});
		};
		for (bool audio : {false, true})
		{
			std::vector<uint8_t> pack{0, 0, 1, 0xba};
			timestamp(pack, origin);
			pack.insert(pack.end(), {0x80, 0, 1});
			deliveries.push_back({0, audio, pack});
		}
		auto pes = [&](bool audio, uint8_t const *data, unsigned size, int64_t pts, unsigned ms, bool fragmented)
		{
			unsigned const length = size + (pts >= 0 ? 5 : 1);
			std::vector<uint8_t> bytes{0, 0, 1, uint8_t(audio ? 0xc0 : 0xe0), uint8_t(length >> 8), uint8_t(length)};
			if (pts >= 0)
				timestamp(bytes, origin + pts);
			else
				bytes.push_back(0x0f);
			bytes.insert(bytes.end(), data, data + size);
			if (fragmented)
			{
				// DMA splits stay word aligned; only the complete PES may have scan padding.
				deliveries.push_back({ms, audio, {bytes.begin(), bytes.begin() + 8}});
				deliveries.push_back({ms + 100, audio, {bytes.begin() + 8, bytes.end()}});
			}
			else
				deliveries.push_back({ms, audio, bytes});
		};
		auto const &es = video[0];
		std::vector<unsigned> starts;
		for (unsigned i = 0; i + 6 < es.size(); ++i)
			if (!es[i] && !es[i + 1] && es[i + 2] == 1 && es[i + 3] == 0)
				starts.push_back(i);
		unsigned gop_base = 0;
		presentation_times.resize(formats[0].frames);
		for (unsigned i = 0; i < starts.size(); ++i)
		{
			unsigned const tr = (unsigned(es[starts[i] + 4]) << 2) | (es[starts[i] + 5] >> 6);
			if (i && tr == 0)
				gop_base = i;
			unsigned const frame = gop_base + tr;
			int64_t const pts = 27000 + frame * 3600 +
								(ingress_mode >= 2 ? (frame >= 24	? 27000
													  : frame >= 12 ? 45000
																	: 0)
												   : 0);
			presentation_times.at(frame) = pts;
			unsigned const begin = i ? starts[i] : 0;
			unsigned const end = i + 1 < starts.size() ? starts[i + 1] : es.size();
			// The first closed GOP contains ten pictures. The final B and reference
			// await the next picture delimiter/reference; resume before queue catch-up.
			unsigned const ms = ingress_mode == 1 && i >= 10 ? 1200 : 200;
			unsigned first = begin;
			if (!i)
			{
				// Save inside the PES timestamp, then after only the first byte of the
				// picture prefix. Its other three bytes begin a different PES at 200 ms.
				first = starts[0] + 1;
				pes(false, es.data(), first, pts, 0, true);
			}
			for (unsigned off = first; off < end; off += 8000)
				pes(false, es.data() + off, std::min(8000U, end - off),
					i && off == begin && (ingress_mode != 3 || frame == 12 || frame == 24) ? pts : -1, ms, false);
		}
		unsigned offset = 0;
		for (unsigned i = 0; i < 98; ++i)
		{
			unsigned const size = 144 * 192000 / 44100 + ((cdi_av_reference::AUDIO[offset + 2] >> 1) & 1);
			// A timestamped partial first frame must keep its 300 ms start after refill.
			if (!i)
			{
				pes(true, cdi_av_reference::AUDIO.data(), 200, 27000, 0, false);
				pes(true, cdi_av_reference::AUDIO.data() + 200, size - 200, -1, 100, false);
			}
			else
				pes(true, cdi_av_reference::AUDIO.data() + offset, size,
					i == 49 ? (ingress_mode == 1   ? -1
							   : ingress_mode == 2 ? 171000
												   : 216000)
							: -1,
					i < 49	  ? 100
					: i == 49 ? 1900
							  : 2000,
					i == 49);
			offset += size;
		}
		std::stable_sort(deliveries.begin(), deliveries.end(),
						 [](auto const &a, auto const &b) { return a.ms < b.ms; });
	}

	std::array<cdi_motion_reference::profile, 3> formats;
	std::vector<unsigned> branch_times;
	std::string directory;
	std::vector<unsigned> snapshots;
	std::array<std::vector<uint8_t>, 3> video, rgb, types;
	std::vector<uint8_t> pcm, steady_pcm;
	std::vector<std::string> failures;
	bool completed = false, queue_telemetry_seen = false;
	unsigned saves = 0, loads = 0, fields = 0, restored_fields = 0, max_rgb_error = 0, max_pcm_error = 0,
			 max_native_error = 0;
	uint64_t reference_pixels = 0, reference_samples = 0, restored_samples = 0, pcm_error_squared = 0,
			 decoded_values = 0, rgb_error_squared = 0;
	std::vector<char> saved_picture_types;

	void capture(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
	{
		auto const found = sound.find(":dvc");
		if (!m_started || found == sound.end() || found->second.size() != 2)
			return;
		int const count = found->second[0].second;
		expect(count == found->second[1].second, "stereo callback lengths differ");
		int64_t const end = machine().time().as_ticks(44100) + 1, start = end - count;
		if (!long_seconds || capacity_mode)
		{
			std::pair<int64_t, unsigned> const block{end, unsigned(count)};
			if (!m_pass)
				m_sound_blocks.push_back(block);
			else
			{
				unsigned const index = m_saved_chunks[m_pass - 1] + m_restored_chunks++;
				expect(index < m_sound_blocks.size() && m_sound_blocks[index] == block,
					   "restored sound callback time/count/order differs");
			}
		}
		if ((!long_seconds || capacity_mode) && !m_pass && end > int64_t(m_baseline_pcm.size()))
			m_baseline_pcm.resize(end);
		for (int i = 0; i < count; ++i)
		{
			int64_t const pos = start + i;
			if (pos < 0)
				continue;
			// Stream selection preserves already queued PCM; the new cold decoder
			// follows the old 98-frame queue without discarding or duplicating samples.
			int64_t relative = pos - 300 * 44100 / 1000;
			if (synchronized_branches)
				for (unsigned boundary : branch_times)
					if (pos > int64_t(boundary) * 44100 / 1000)
						relative = pos - (int64_t(boundary) * 44100 / 1000 + 1 + 4410);
			if (ingress_mode)
			{
				// Refill updates through the current sample before queuing a future wait.
				// Match the existing stream-boundary contract: the next sample is +1.
				--relative;
				int64_t const refill_start = (ingress_mode == 3 ? 2400 : 2000) * 44100 / 1000 + 1;
				if (pos >= refill_start)
					relative = pos - refill_start + 56448;
				else if (relative >= 56448)
					relative = -1;
			}
			bool const active =
				relative >= 0 && (long_seconds || relative < (synchronized_branches || ingress_mode ? 1 : 2) * 112896);
			auto const &reference = long_seconds && relative >= 112896 ? steady_pcm : pcm;
			for (unsigned ch = 0; ch < 2; ++ch)
			{
				int const sample = std::lround(found->second[ch].first[i] * 32768.0f);
				if (!long_seconds || capacity_mode)
				{
					if (!m_pass)
						m_baseline_pcm[pos][ch] = sample;
					else
					{
						expect(pos < int64_t(m_baseline_pcm.size()) && sample == m_baseline_pcm[pos][ch],
							   "restored PCM differs at " + std::to_string(pos));
						++restored_samples;
					}
				}
				int wanted = 0;
				if (active)
				{
					unsigned const offset = (relative % 112896) * 4 + ch * 2;
					wanted = int16_t(unsigned(reference[offset]) | (unsigned(reference[offset + 1]) << 8));
				}
				unsigned const error = std::abs(sample - wanted);
				max_pcm_error = std::max(max_pcm_error, error);
				pcm_error_squared += uint64_t(error) * error;
				++reference_samples;
			}
		}
	}

  protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		if (capacity_mode || ingress_mode)
			machine().add_logerror_callback(
				[this](char const *message)
				{
					if (ingress_mode && std::strstr(message, "DVC_PRESENTATION_QUEUE_TELEMETRY"))
					{
						queue_telemetry_seen = true;
						std::fputs(message, stdout);
						expect(std::strstr(message, "decoded=36 ") && std::strstr(message, "max_depth=26 ") &&
								   std::strstr(message, "fallback=0 ") && std::strstr(message, "queued=0"),
							   "bounded decode-ahead queue did not fill and drain with timestamped output");
					}
					if (capacity_mode && std::strstr(message, "DVC_SAVE_STATE"))
						std::fputs(message, stdout);
					if (capacity_mode && std::strstr(message, "DVC_SAVE_STATE_SNAPSHOT"))
					{
						unsigned const index = saves;
						expect(index < 4, "unexpected capacity snapshot");
						expect(bool(std::strstr(message, "audio_overflow=1")) == (index >= 1),
							   "audio capacity boundary was not crossed as scheduled");
						expect(bool(std::strstr(message, "video_overflow=1")) == (index >= 3),
							   "video capacity boundary was not crossed as scheduled");
					}
				});
		m_timer = timer_alloc(FUNC(cdi_motion_state::step), this);
		save_item(NAME(m_ms));
		save_item(NAME(m_next_delivery));
		save_item(NAME(m_branch_ms));
		save_item(NAME(m_branch));
		save_item(NAME(m_paused));
		save_item(NAME(m_previous_field_time));
		save_item(NAME(m_field_frame));
		save_item(NAME(m_field_profile));
		save_item(NAME(m_field_time));
		save_item(NAME(m_last_frame));
		save_item(NAME(m_video_chunks));
		save_item(NAME(m_audio_chunks));
		machine().save().register_presave(save_prepost_delegate(FUNC(cdi_motion_state::presave), this));
		machine().save().register_postload(save_prepost_delegate(FUNC(cdi_motion_state::postload), this));
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
		if (!ok && failures.size() < 20)
			failures.push_back(message);
	}
	void presave()
	{
		++saves;
		m_saved_ms.push_back(m_ms);
		m_saved_chunks.push_back(m_sound_blocks.size());
		saved_picture_types.push_back(
			m_last_frame < 0 ? '-' : char(types[m_field_profile][m_last_frame % types[m_field_profile].size()]));
	}
	void postload()
	{
		++loads;
		m_restored_chunks = 0;
		expect(m_ms == m_saved_ms[m_pass - 1], "snapshot time differs");
	}
	std::string path(unsigned index) const
	{
		return directory + "/motion-" + std::to_string(index) + ".sta";
	}
	void display_setup(address_space &space)
	{
		// Both field ICA pointers jump to an original display list.
		space.write_dword(0x400, 0x40001000);
		space.write_dword(0x404, 0x40001000);
		uint32_t address = 0x1000;
		auto command = [&](uint32_t value)
		{
			space.write_dword(address, value);
			address += 4;
		};
		command(0xc0040000 | (display_mode == 2 ? 3 : display_mode ? 0xb : 1));
		command(0x78000000 | (display_mode == 2 ? 2 : 0));	  // bitmap or RL7
		command(display_mode == 3 ? 0xd9800003 : 0xd9000000); // CLUT4 hold-three
		command(0xc1000003);								  // A transparent when matte flag 0 is set
		command(0x814488cc);
		command(0x82cc8844);
		command(0x8344cc88);
		command(0x84cc4488);
		command(0xdb00003f); // native color / weight
		command(0xd0900060);
		command(0xd1000000); // matte starts at x=96
		command(0xcd018068);
		command(0xce80000f); // white cursor at x=104,y=24
		for (unsigned y = 0; y < 16; ++y)
			command(0xcf00ffff | (y << 16));
		command(0x50010000); // reset bitmap VSR every field
		for (unsigned a = 0x10000; a < 0x30000; a += 2)
			space.write_word(a, display_mode == 1 || display_mode == 3 ? 0x1234 : 0x0101);
		if (display_mode == 2)
			for (unsigned line = 0; line < 300; ++line)
			{
				// Three 24-pixel runs and a final run extending to end of line.
				space.write_dword(0x10000 + line * 8, 0x810c820c);
				space.write_dword(0x10004 + line * 8, 0x830c8400);
			}
		space.write_word(0x4ffff2, 0xc200); // display, 30 MHz crystal, ICA
	}
	void feed(bool audio, std::vector<uint8_t> const &bytes)
	{
		m_maincpu->space(AS_PROGRAM).write_word(audio ? 0xe03000 : 0xe040c0, audio ? 0x8000 : 0x9000);
		for (unsigned i = 0; i < bytes.size(); i += 2)
			m_dvc->dma_w((unsigned(bytes[i]) << 8) | (i + 1 < bytes.size() ? bytes[i + 1] : 0));
		m_dvc->dma_done();
	}
	void packet(bool audio, uint8_t const *data, unsigned length, int pts, unsigned stream)
	{
		std::vector<uint8_t> bytes;
		if (pts >= 0)
			bytes = {0, 0, 1, 0xba, 0x21, 0, 1, 0, 1, 0x80, 0, 1};
		unsigned const size = length + (pts >= 0 ? 5 : 1);
		bytes.insert(bytes.end(),
					 {0, 0, 1, uint8_t((audio ? 0xc0 : 0xe0) | stream), uint8_t(size >> 8), uint8_t(size)});
		if (pts >= 0)
		{
			uint64_t const value = pts;
			bytes.insert(bytes.end(), {uint8_t(0x21 | ((value >> 29) & 14)), uint8_t(value >> 22),
									   uint8_t((value >> 14) | 1), uint8_t(value >> 7), uint8_t((value << 1) | 1)});
		}
		else
			bytes.push_back(0x0f);
		bytes.insert(bytes.end(), data, data + length);
		feed(audio, bytes);
	}
	void video_chunk(unsigned profile, int pts, unsigned stream, unsigned start = 0)
	{
		auto const &bytes = video[profile];
		for (unsigned offset = start; offset < bytes.size(); offset += 16000)
			packet(false, bytes.data() + offset, std::min<unsigned>(16000, bytes.size() - offset),
				   offset == start ? pts : -1, stream);
	}
	void audio_chunk(int pts, unsigned stream)
	{
		unsigned offset = 0;
		for (unsigned i = 0; i < 98; ++i)
		{
			unsigned const size = 144 * 192000 / 44100 + ((cdi_av_reference::AUDIO[offset + 2] >> 1) & 1);
			packet(true, cdi_av_reference::AUDIO.data() + offset, size, i == 0 ? pts : -1, stream);
			offset += size;
		}
	}
	void begin_scene(address_space &space)
	{
		unsigned const profile = (initial_profile + m_branch) % 3;
		auto const &p = formats[profile];
		space.write_word(0xe0301c, 0xffff);
		space.write_word(0xe04060, 0xffff);
		if (!m_branch || synchronized_branches)
		{
			space.write_word(0xe03000, 1);
			space.write_word(0xe03022, 0);
			space.write_word(0xe03024, 0x80);
			space.write_word(0xe03022, 1);
			space.write_word(0xe03024, 0x93);
			space.write_word(0xe03022, 7);
			for (uint8_t value : {0, 0x80, 0x80, 0})
				space.write_word(0xe03024, value);
		}
		unsigned const stream = m_branch % 2;
		space.write_word(0xe03008, stream);
		space.write_word(0xe040c0, 0x0100); // clear decoder before format branch
		space.write_word(0xe040c4, stream);
		space.write_word(0xe040c6, 4);
		space.write_word(0xe04074, 10);
		space.write_word(0xe04076, 16);
		space.write_word(0xe04078, p.height);
		space.write_word(0xe0407a, p.width);
		space.write_word(0xe040c2, 0x0228);
		if (m_branch)
		{
			// Deliberately deliver old IDs before the requested streams.
			video_chunk(initial_profile, -1, stream ^ 1);
			packet(true, cdi_av_reference::AUDIO.data(), 626, -1, stream ^ 1);
			expect(space.read_word(0xe0300a) != stream,
				   "current audio stream changed before requested header current=" +
					   std::to_string(space.read_word(0xe0300a)) + " requested=" + std::to_string(stream));
		}
		if (ingress_mode)
		{
			space.write_word(0xe040c0, 0x0008);
			return;
		}
		int const pts = m_branch ? (synchronized_branches ? 9000 : 0) : 27000;
		if (deferred_video && !m_branch)
			packet(false, video[profile].data(), 32, pts, stream);
		else
			video_chunk(profile, pts, stream);
		audio_chunk(pts, stream);
		space.write_word(0xe040c0, 0x0008);
		expect(space.read_word(0xe0300a) == stream, "requested audio stream did not commit");
		m_last_frame = -1;
	}
	uint32_t render(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &clip)
	{
		uint32_t const result = screen_update_cdimono1(screen, bitmap, clip);
		if (!m_started)
			return result;
		int const top = screen.visible_area().min_y, bottom = screen.visible_area().max_y;
		if (clip.min_y <= top && clip.max_y >= top)
		{
			m_field_time = machine().time().as_ticks(90000);
			m_field_profile = (initial_profile + m_branch) % 3;
			auto const &p = formats[m_field_profile];
			int64_t const elapsed =
				m_field_time - (m_branch ? int64_t(m_branch_ms) * 90 + (synchronized_branches ? 9000 : 0) : 27000);
			int const due = elapsed < 0 ? -1 : int(elapsed * p.rate_num / (90000ULL * p.rate_den));
			m_field_frame = m_paused ? m_last_frame : (long_seconds ? due : std::min<int>(due, p.frames - 1));
			if (ingress_mode)
			{
				int due_frame = -1;
				for (unsigned i = 0; i < presentation_times.size() && presentation_times[i] <= m_field_time; ++i)
					due_frame = i;
				if (ingress_mode == 1 && m_field_time < 1200 * 90)
					due_frame = std::min(due_frame, 7);
				m_field_frame = due_frame;
			}
			m_last_frame = m_field_frame;
		}
		if (clip.max_y < bottom || m_field_time < 9000)
			return result;
		if (m_previous_field_time)
			expect(std::abs(m_field_time - m_previous_field_time - int64_t(screen.frame_period().as_ticks(90000))) <= 1,
				   "composed field cadence skipped or repeated");
		m_previous_field_time = m_field_time;
		auto const &p = formats[m_field_profile];
		uint64_t hash = 1469598103934665603ULL;
		unsigned frame_error = 0;
		std::string worst_pixel;
		for (int y = top; y <= bottom; ++y)
			for (int x = 0; x < 768; ++x)
			{
				uint32_t const actual = bitmap.pix(y, x) & 0xffffff;
				constexpr uint32_t colors[] = {0x4386c9, 0xc98643, 0x43c986, 0xc94386};
				unsigned const native_index = display_mode == 2	  ? std::min(x / 24, 3)
											  : display_mode == 3 ? ((x / 3) * 3) % 4
											  : display_mode == 1 ? x % 4
																  : 0;
				uint32_t wanted = x < 96 ? colors[native_index] : 0;
				int const sy = y - top;
				bool decoded = false;
				if (m_field_frame >= 0 && x >= 96 && x >= 64 && x < 64 + int(p.width) * 2 && sy >= 20 &&
					sy < 20 + int(p.height) * 2)
				{
					decoded = true;
					unsigned const offset =
						((m_field_frame % p.frames) * p.width * p.height + ((sy - 20) / 2) * p.width + (x - 64) / 2) *
						3;
					wanted = (unsigned(rgb[m_field_profile][offset]) << 16) |
							 (unsigned(rgb[m_field_profile][offset + 1]) << 8) | rgb[m_field_profile][offset + 2];
				}
				if (x >= 104 && x < 136 && sy >= 48 && sy < 80)
				{
					wanted = 0xe6e6e6;
					decoded = false;
				}
				for (unsigned shift : {0, 8, 16})
				{
					unsigned const error = std::abs(int(uint8_t(actual >> shift)) - int(uint8_t(wanted >> shift)));
					if (error > frame_error)
					{
						frame_error = error;
						worst_pixel = " x=" + std::to_string(x) + " y=" + std::to_string(y) +
									  " actual=" + std::to_string(actual) + " wanted=" + std::to_string(wanted);
					}
					if (decoded)
					{
						rgb_error_squared += uint64_t(error) * error;
						++decoded_values;
					}
					else
						max_native_error = std::max(max_native_error, error);
				}
				hash = (hash ^ actual) * 1099511628211ULL;
				++reference_pixels;
			}
		max_rgb_error = std::max(max_rgb_error, frame_error);
		expect(frame_error <= 12, "composed RGB mismatch t=" + std::to_string(m_field_time) +
									  " frame=" + std::to_string(m_field_frame) +
									  " error=" + std::to_string(frame_error) + worst_pixel);
		if (!long_seconds || capacity_mode)
		{
			if (!m_pass)
				m_baseline_fields[m_field_time] = hash;
			else
			{
				auto const it = m_baseline_fields.find(m_field_time);
				expect(it != m_baseline_fields.end() && it->second == hash, "restored composed field/time differs");
				++restored_fields;
			}
		}
		++fields;
		return result;
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		m_timer->adjust(attotime::from_msec(1));
		auto &space = m_maincpu->space(AS_PROGRAM);
		if (!m_started)
		{
			m_started = true;
			display_setup(space);
			begin_scene(space);
			while (m_next_delivery < deliveries.size() && !deliveries[m_next_delivery].ms)
			{
				auto const &d = deliveries[m_next_delivery++];
				feed(d.audio, d.bytes);
			}
			return;
		}
		++m_ms;
		while (m_next_delivery < deliveries.size() && deliveries[m_next_delivery].ms <= m_ms)
		{
			auto const &d = deliveries[m_next_delivery++];
			feed(d.audio, d.bytes);
		}
		if (deferred_video && m_ms == 100)
			video_chunk(initial_profile, -1, 0, 32);
		if (long_seconds && (max_rgb_error > 12 || max_pcm_error > 600 || max_native_error || !failures.empty()))
		{
			std::printf("MOTION_LONG_FAILURE ms=%u max_rgb=%u max_pcm=%u native=%u\n", m_ms, max_rgb_error,
						max_pcm_error, max_native_error);
			machine().schedule_exit();
			return;
		}
		if (long_seconds)
		{
			if (m_ms == 300 + m_video_chunks * 2000 - 400)
			{
				video_chunk(0, -1, 0);
				++m_video_chunks;
			}
			if (m_ms == 300 + m_audio_chunks * 2560 - 400)
			{
				audio_chunk(-1, 0);
				++m_audio_chunks;
			}
			if (m_ms % 60000 == 0)
				std::printf("MOTION_LONG seconds=%u fields=%u max_rgb=%u max_pcm=%u samples=%llu\n", m_ms / 1000,
							fields, max_rgb_error, max_pcm_error, (unsigned long long)reference_samples);
		}
		else
		{
			if (!ingress_mode && m_ms == 875)
			{
				space.write_word(0xe040c0, 0x0010);
				m_paused = true;
			}
			if (!ingress_mode && m_ms == 1035)
			{
				space.write_word(0xe040c0, 0x0020);
				m_paused = false;
			}
			auto const &screen = *subdevice<screen_device>("screen");
			bool const blank = screen.vpos() < screen.visible_area().min_y;
			if (!ingress_mode && m_branch < (synchronized_branches ? 2U : 1U) && m_ms >= 1600 * (m_branch + 1) && blank)
			{
				if (!m_pass)
					branch_times.push_back(m_ms);
				++m_branch;
				m_branch_ms = m_ms;
				begin_scene(space);
			}
			if (!m_pass && saves < snapshots.size() && m_ms >= snapshots[saves] && blank)
				machine().schedule_save(path(saves));
		}
		if (capacity_mode && !m_pass && saves < snapshots.size())
		{
			auto const &screen = *subdevice<screen_device>("screen");
			if (m_ms >= snapshots[saves] && screen.vpos() < screen.visible_area().min_y)
				machine().schedule_save(path(saves));
		}
		bool const before = m_dvc_irq_state;
		std::array<uint32_t, 3> const event{space.read_word(0xe0301a), space.read_word(0xe04062), uint32_t(before)};
		expect(before == bool(event[0] | event[1]) && !m_dvc_irq_state,
			   "IRQ assertion/acknowledgement differs from enabled status");
		if (!long_seconds || capacity_mode)
		{
			if (!m_pass)
				m_baseline_events.push_back(event);
			else
				expect(event == m_baseline_events[m_ms - 1], "restored IRQ/status differs at " + std::to_string(m_ms));
		}
		if (capacity_mode)
		{
			if (!m_pass && !m_saved_ms.empty() && m_ms == m_saved_ms.back() + 500)
				m_capacity_chunks.push_back(m_sound_blocks.size() - m_saved_chunks.back());
			if (m_pass && m_ms == m_saved_ms[m_pass - 1] + 500)
			{
				expect(m_restored_chunks == m_capacity_chunks[m_pass - 1], "capacity restored callback count differs");
				if (m_pass == snapshots.size())
				{
					completed = true;
					machine().schedule_exit();
				}
				else
				{
					++m_pass;
					machine().schedule_load(path(m_pass - 1));
				}
				return;
			}
		}
		if (m_ms == (long_seconds ? 300 + long_seconds * 1000 : 4400))
		{
			if (!long_seconds && m_pass)
				expect(m_restored_chunks == m_sound_blocks.size() - m_saved_chunks[m_pass - 1],
					   "restored sound callback count differs");
			if ((long_seconds && !capacity_mode) || m_pass == snapshots.size())
			{
				completed = true;
				machine().schedule_exit();
			}
			else
			{
				++m_pass;
				machine().schedule_load(path(m_pass - 1));
			}
		}
	}
	emu_timer *m_timer = nullptr;
	unsigned m_next_delivery = 0;
	unsigned m_ms = 0, m_branch_ms = 0, m_branch = 0, m_pass = 0, m_field_profile = 0, m_video_chunks = 1,
			 m_audio_chunks = 1;
	int m_field_frame = -1, m_last_frame = -1;
	int64_t m_field_time = 0, m_previous_field_time = 0;
	bool m_started = false, m_paused = false;
	std::vector<unsigned> m_saved_ms, m_saved_chunks, m_capacity_chunks;
	unsigned m_restored_chunks = 0;
	std::vector<std::pair<int64_t, unsigned>> m_sound_blocks;
	std::vector<std::array<int16_t, 2>> m_baseline_pcm;
	std::map<int64_t, uint64_t> m_baseline_fields;
	std::vector<std::array<uint32_t, 3>> m_baseline_events;
};

ROM_START(cdimotion)
ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END
GAME(2026, cdimotion, 0, motion, cdi_dma_integration, cdi_motion_state, empty_init, ROT0, "MAME",
	 "CD-i composed moving A/V fixture", MACHINE_SUPPORTS_SAVE)

void run_cdi_motion(unsigned profile, unsigned seconds, bool capacity = false, bool full_size = false,
					unsigned mode = 0, bool synchronized = false, bool deferred = false, unsigned ingress = 0)
{
	cdi_q_disc temp;
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdimotion).name));
	options.set_value(OPTION_THROTTLE, 0, OPTION_PRIORITY_MAXIMUM);
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdimotion), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);
	auto &state = downcast<cdi_motion_state &>(machine.root_device());
	state.initial_profile = profile;
	state.long_seconds = seconds;
	state.capacity_mode = capacity;
	state.display_mode = mode;
	state.synchronized_branches = synchronized;
	state.deferred_video = deferred;
	state.ingress_mode = ingress;
	std::copy(std::begin(cdi_motion_reference::PROFILES), std::end(cdi_motion_reference::PROFILES),
			  state.formats.begin());
	state.directory = std::filesystem::path(temp.path()).parent_path().string();
	state.snapshots =
		profile == 0 ? std::vector<unsigned>{340, 380, 460, 900, 1600, 3620} : std::vector<unsigned>{900, 1600};
	if (capacity)
		state.snapshots = {347900, 348650, 1333750, 1334500};
	state.video = {std::vector<uint8_t>(cdi_motion_reference::VIDEO_0.begin(), cdi_motion_reference::VIDEO_0.end()),
				   std::vector<uint8_t>(cdi_motion_reference::VIDEO_1.begin(), cdi_motion_reference::VIDEO_1.end()),
				   std::vector<uint8_t>(cdi_motion_reference::VIDEO_2.begin(), cdi_motion_reference::VIDEO_2.end())};
	state.types = {std::vector<uint8_t>(cdi_motion_reference::TYPES_0.begin(), cdi_motion_reference::TYPES_0.end()),
				   std::vector<uint8_t>(cdi_motion_reference::TYPES_1.begin(), cdi_motion_reference::TYPES_1.end()),
				   std::vector<uint8_t>(cdi_motion_reference::TYPES_2.begin(), cdi_motion_reference::TYPES_2.end())};
	state.rgb = {cdi_av_inflate(cdi_motion_reference::RGB_0_Z, 64 * 48 * 50 * 3),
				 cdi_av_inflate(cdi_motion_reference::RGB_1_Z, 80 * 64 * 60 * 3),
				 cdi_av_inflate(cdi_motion_reference::RGB_2_Z, 96 * 48 * 48 * 3)};
	if (full_size)
	{
		for (unsigned i = 0; i < 3; ++i)
		{
			auto const &f = cdi_full_reference::PROFILES[i];
			state.formats[i] = {f.width, f.height, f.rate_num, f.rate_den, f.frames};
		}
		state.video = {cdi_full_bytes(cdi_full_reference::VIDEO_0), cdi_full_bytes(cdi_full_reference::VIDEO_1),
					   cdi_full_bytes(cdi_full_reference::VIDEO_2)};
		state.types = {cdi_full_bytes(cdi_full_reference::TYPES_0), cdi_full_bytes(cdi_full_reference::TYPES_1),
					   cdi_full_bytes(cdi_full_reference::TYPES_2)};
		state.rgb = {cdi_full_rgb(cdi_full_reference::RGB_0_Z, 352 * 288 * 36 * 3),
					 cdi_full_rgb(cdi_full_reference::RGB_1_Z, 352 * 240 * 45 * 3),
					 cdi_full_rgb(cdi_full_reference::RGB_2_Z, 384 * 288 * 36 * 3)};
		state.snapshots =
			synchronized ? std::vector<unsigned>{340, 900, 1600, 1700, 3200, 3320} : std::vector<unsigned>{900, 1600};
	}
	if (deferred)
		state.snapshots = {50, 900, 1600};
	if (ingress)
	{
		state.snapshots = {50, 150, 1800, 1950, 2300};
		state.prepare_ingress();
	}
	state.pcm = cdi_av_inflate(cdi_av_reference::PCM_Z, 112896 * 4);
	state.steady_pcm = cdi_av_inflate(cdi_motion_reference::PCM_STEADY_Z, 112896 * 4);
	cdi_motion_capture = &state;
	int const error = machine.run(true);
	cdi_motion_capture = nullptr;
	manager.set_machine(nullptr);
	std::printf("MOTION_METRICS ingress=%u profile=%u full=%u mode=%u sync=%u deferred=%u seconds=%u saves=%u loads=%u "
				"fields=%u "
				"restored_fields=%u samples=%llu restored_samples=%llu decoded_values=%llu rgb_squared=%llu "
				"pcm_squared=%llu max_rgb=%u max_pcm=%u native=%u\n",
				ingress, profile, full_size, mode, synchronized, deferred, seconds, state.saves, state.loads,
				state.fields, state.restored_fields, (unsigned long long)state.reference_samples,
				(unsigned long long)state.restored_samples, (unsigned long long)state.decoded_values,
				(unsigned long long)state.rgb_error_squared, (unsigned long long)state.pcm_error_squared,
				state.max_rgb_error, state.max_pcm_error, state.max_native_error);
	for (unsigned boundary : state.branch_times)
		std::printf("MOTION_BRANCH mode=%u sync=%u ms=%u delay_ms=%u\n", mode, synchronized, boundary,
					synchronized ? 100U : 0U);
	CAPTURE(profile);
	CAPTURE(seconds);
	CAPTURE(mode);
	CAPTURE(synchronized);
	CAPTURE(state.max_rgb_error);
	CAPTURE(state.max_pcm_error);
	CAPTURE(state.max_native_error);
	CAPTURE(state.decoded_values);
	CAPTURE(state.rgb_error_squared);
	CAPTURE(state.fields);
	CAPTURE(state.restored_fields);
	CAPTURE(state.restored_samples);
	CAPTURE(state.reference_pixels);
	std::string const saved_types(state.saved_picture_types.begin(), state.saved_picture_types.end());
	CAPTURE(saved_types);
	for (auto const &failure : state.failures)
	{
		INFO(failure);
		CHECK(false);
	}
	REQUIRE(error == EMU_ERR_NONE);
	REQUIRE(state.completed);
	if (ingress)
		CHECK(state.queue_telemetry_seen);
	CHECK(state.max_rgb_error <= 12);
	CHECK(state.max_pcm_error <= 600);
	CHECK(state.max_native_error == 0);
	REQUIRE(state.decoded_values > 0);
	CHECK(state.rgb_error_squared / state.decoded_values <= 4);
	REQUIRE(state.reference_samples > 100000);
	CHECK(state.pcm_error_squared / state.reference_samples <= 150 * 150);
	if (!seconds || capacity)
	{
		CHECK(state.saves == state.snapshots.size());
		CHECK(state.loads == state.snapshots.size());
		CHECK(state.restored_fields >= (capacity ? 80 : 101));
		CHECK(state.restored_samples >= (capacity ? 150000 : 300001));
	}
	else
	{
		CHECK(state.fields >= seconds * 49);
		CHECK(state.reference_samples >= uint64_t(seconds) * 44100 * 2);
	}
}

TEST_CASE("DVC moving I P B video composes with native matte and cursor across pause branch and snapshots",
		  "[emu][philips][dvc][motion][integration]")
{
	for (unsigned profile = 0; profile < 3; ++profile)
		run_cdi_motion(profile, 0);
}

TEST_CASE("DVC full-size moving pictures compose with CLUT8 CLUT4 and RL7 native modes",
		  "[emu][philips][dvc][motion-full][integration]")
{
	for (unsigned profile = 0; profile < 3; ++profile)
		run_cdi_motion(profile, 0, false, true, profile);
}

TEST_CASE("DVC synchronized reset branches share future audio video timestamps with mosaic native output",
		  "[emu][philips][dvc][motion-branch][integration]")
{
	run_cdi_motion(0, 0, false, true, 3, true);
}

TEST_CASE("DVC saves retain the first picture timestamp across incomplete PES input",
		  "[emu][philips][dvc][motion-pending-pts][integration]")
{
	run_cdi_motion(0, 0, false, true, 0, false, true);
}

TEST_CASE("DVC sparse PES refill and discontinuous wrapped timestamps preserve composed AV and saves",
		  "[emu][philips][dvc][motion-ingress][integration]")
{
	for (unsigned mode = 1; mode <= 3; ++mode)
		run_cdi_motion(0, 0, false, true, 0, false, false, mode);
}

TEST_CASE("DVC composed moving video and changing audio remain continuous for thirty minutes",
		  "[.][emu][philips][dvc][motion-long][integration]")
{
	char const *duration = std::getenv("CDI_MOTION_SECONDS");
	unsigned const seconds = duration ? std::stoul(duration) : 1800;
	REQUIRE(seconds > 0);
	REQUIRE(seconds <= 1800);
	run_cdi_motion(0, seconds);
}

TEST_CASE("DVC long-playback saves survive both replay-journal capacity boundaries",
		  "[.][emu][philips][dvc][motion-capacity][integration]")
{
	run_cdi_motion(0, 1336, true);
}

void cdi_motion_sound_hook(std::map<std::string, std::vector<std::pair<const float *, int>>> const &sound)
{
	if (cdi_motion_capture)
		cdi_motion_capture->capture(sound);
}
} // anonymous namespace
