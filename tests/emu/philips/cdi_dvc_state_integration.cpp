// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// This file is included by cdi_dvc_dma_test_support.cpp after the shared
// full-machine test OSD/manager definitions from cdi_dvc_dma_integration.cpp.

namespace
{

class cdi_dvc_state_integration_state : public cdi_state
{
public:
	cdi_dvc_state_integration_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_dvc_state_integration(machine_config &config)
	{
		cdimono1dvc(config);
	}

	bool completed() const { return m_completed; }
	std::vector<std::string> const &failures() const { return m_failures; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_dvc_state_integration_state::test_step), this);
	}

	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_failures.clear();
		m_completed = false;
		m_test_timer->adjust(attotime::zero);
	}

private:
	static constexpr uint32_t FMA_COMMAND = 0x00e03000U;
	static constexpr uint32_t FMA_STATUS = 0x00e03002U;
	static constexpr uint32_t FMA_STREAM = 0x00e03008U;
	static constexpr uint32_t FMA_CURRENT_STREAM = 0x00e0300aU;
	static constexpr uint32_t FMA_IRQ_STATUS = 0x00e0301aU;
	static constexpr uint32_t FMA_IRQ_ENABLE = 0x00e0301cU;
	static constexpr uint32_t FMV_IRQ_ENABLE = 0x00e04060U;
	static constexpr uint32_t FMV_IRQ_STATUS = 0x00e04062U;
	static constexpr uint32_t FMV_FRAME_PERIOD = 0x00e040a8U;
	static constexpr uint32_t FMV_SYSTEM_COMMAND = 0x00e040c0U;
	static constexpr uint32_t FMV_STREAM = 0x00e040c4U;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	static uint32_t hash_word(uint32_t hash, uint16_t value)
	{
		hash ^= uint8_t(value >> 8);
		hash *= 16777619U;
		hash ^= uint8_t(value);
		hash *= 16777619U;
		return hash;
	}

	static std::vector<uint8_t> make_silent_layer2_frame()
	{
		// MPEG-1 Layer II, 192 kbit/s, 44.1 kHz, stereo, no CRC/padding.
		constexpr uint8_t bitrate_index = 10;
		constexpr uint32_t header =
			(0x7ffU << 21)
			| (3U << 19)
			| (2U << 17)
			| (1U << 16)
			| (uint32_t(bitrate_index) << 12);
		auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
		if (!decoded.valid || decoded.frame_size_bytes < 4)
			return { };

		std::vector<uint8_t> frame(decoded.frame_size_bytes, 0);
		frame[0] = uint8_t(header >> 24);
		frame[1] = uint8_t(header >> 16);
		frame[2] = uint8_t(header >> 8);
		frame[3] = uint8_t(header);
		return frame;
	}

	static std::vector<uint8_t> make_video_pes(uint8_t stream, std::vector<uint8_t> payload)
	{
		// MPEG-1 PES with the minimal no-timestamp header byte.  Keep the payload
		// length even so the direct word feeder cannot fabricate a trailing byte.
		if ((payload.size() & 1U) == 0)
			payload.push_back(0);

		uint16_t const packet_length = uint16_t(payload.size() + 1U);
		std::vector<uint8_t> packet {
			0x00, 0x00, 0x01, uint8_t(0xe0U | (stream & 0x0fU)),
			uint8_t(packet_length >> 8), uint8_t(packet_length),
			0x0f
		};
		packet.insert(packet.end(), payload.begin(), payload.end());
		return packet;
	}

	void feed_words(std::vector<uint8_t> const &bytes)
	{
		for (std::size_t index = 0; index < bytes.size(); index += 2)
		{
			uint16_t const word = uint16_t(bytes[index]) << 8
				| uint16_t(index + 1 < bytes.size() ? bytes[index + 1] : 0);
			m_dvc->dma_w(word);
		}
		m_dvc->dma_done();
	}

	void feed_audio_frame(address_space &space, std::vector<uint8_t> const &frame)
	{
		space.write_word(FMA_COMMAND, 0x8000);
		feed_words(frame);
	}

	void feed_fma_program_end(address_space &space)
	{
		space.write_word(FMA_COMMAND, 0x8000);
		feed_words({ 0x00, 0x00, 0x01, 0xb9 });
	}

	void feed_video_payload(address_space &space, uint8_t stream, std::vector<uint8_t> payload)
	{
		space.write_word(FMV_SYSTEM_COMMAND, 0x9008); // decoder on + play + DMA
		feed_words(make_video_pes(stream, std::move(payload)));
	}

	uint32_t run_postload_continuation(
			address_space &space, std::vector<uint8_t> const &audio_frame,
			uint8_t audio_stream, uint8_t video_stream)
	{
		uint32_t hash = 2166136261U;
		for (unsigned index = 0; index < 64; ++index)
		{
			// Exercise real device-level pause/continue/stop/play transitions while
			// the reconstructed audio and video backends remain live.
			switch (index & 3U)
			{
			case 0: space.write_word(FMV_SYSTEM_COMMAND, 0x0010); break; // pause
			case 1: space.write_word(FMV_SYSTEM_COMMAND, 0x0020); break; // continue
			case 2: space.write_word(FMV_SYSTEM_COMMAND, 0x0080); break; // stop
			case 3: space.write_word(FMV_SYSTEM_COMMAND, 0x0008); break; // play
			}

			space.write_word(FMA_STREAM, audio_stream);
			feed_audio_frame(space, audio_frame);
			hash = hash_word(hash, space.read_word(FMA_CURRENT_STREAM));
			hash = hash_word(hash, space.read_word(FMA_STATUS));
			hash = hash_word(hash, space.read_word(FMA_IRQ_STATUS));
			hash = hash_word(hash, space.read_word(FMV_SYSTEM_COMMAND));
			hash = hash_word(hash, space.read_word(FMV_STREAM));
			hash = hash_word(hash, space.read_word(FMV_FRAME_PERIOD));
		}

		// Continue the reconstructed video elementary stream through a real
		// selected PES packet and sequence-end marker.
		feed_video_payload(space, video_stream, { 0x00, 0x00, 0x01, 0xb7 });
		hash = hash_word(hash, space.read_word(FMV_IRQ_STATUS));
		return hash;
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
		address_space &space = m_maincpu->space(AS_PROGRAM);
		std::vector<uint8_t> const audio_frame = make_silent_layer2_frame();
		expect(!audio_frame.empty(), "fixture: failed to construct legal Layer II frame");
		if (audio_frame.empty())
		{
			m_completed = true;
			machine().schedule_exit();
			return;
		}

		space.write_word(FMA_IRQ_ENABLE, 0xffff);
		space.write_word(FMV_IRQ_ENABLE, 0xffff);

		// 1. Pending stream-switch save/load on the live DVC device.
		space.write_word(FMA_COMMAND, 0x0001);
		space.write_word(FMA_STREAM, 1);
		feed_audio_frame(space, audio_frame);
		expect(space.read_word(FMA_CURRENT_STREAM) == 1,
			"stream-save: initial stream did not commit");
		space.read_word(FMA_IRQ_STATUS); // establish a clean interrupt baseline
		space.write_word(FMA_STREAM, 2);
		expect(space.read_word(FMA_STREAM) == 2,
			"stream-save: requested stream did not update");
		expect(space.read_word(FMA_CURRENT_STREAM) == 1,
			"stream-save: current stream changed before a new header");

		{
			ram_state stream_snapshot(machine().save());
			expect(stream_snapshot.save() == STATERR_NONE,
				"stream-save: ram_state capture failed");

			space.write_word(FMA_STREAM, 9);
			feed_audio_frame(space, audio_frame);
			expect(space.read_word(FMA_CURRENT_STREAM) == 9,
				"stream-save: mutation did not commit stream 9");

			expect(stream_snapshot.load() == STATERR_NONE,
				"stream-save: ram_state restore failed");
			expect(space.read_word(FMA_STREAM) == 2,
				"stream-save: requested stream was not restored");
			expect(space.read_word(FMA_CURRENT_STREAM) == 1,
				"stream-save: current stream was not restored");
			space.read_word(FMA_IRQ_STATUS);
			feed_audio_frame(space, audio_frame);
			expect(space.read_word(FMA_CURRENT_STREAM) == 2,
				"stream-save: restored pending stream did not commit");
			uint16_t const irq = space.read_word(FMA_IRQ_STATUS);
			expect(bool(irq & cdi_dvc::FMA_IRQ_STREAM_CHANGE),
				"stream-save: restored stream change did not raise CSU event");
			expect(bool(irq & cdi_dvc::FMA_IRQ_FRAME_DECODED),
				"stream-save: restored decoder did not continue to a frame");
		}

		// 2. Program-end save/load must preserve the closed input state.  Only an
		// explicit stop/reset may reopen input, after which rapid stop/start cycles
		// must continue to commit the requested stream deterministically.
		feed_fma_program_end(space);
		expect(bool(space.read_word(FMA_STATUS) & cdi_dvc::FMA_IRQ_END_ISO),
			"end-save: program end did not latch status");
		{
			ram_state end_snapshot(machine().save());
			expect(end_snapshot.save() == STATERR_NONE,
				"end-save: ram_state capture failed");

			space.write_word(FMA_COMMAND, 0x0001);
			space.write_word(FMA_STREAM, 7);
			feed_audio_frame(space, audio_frame);
			expect(space.read_word(FMA_CURRENT_STREAM) == 7,
				"end-save: mutation did not restart on stream 7");

			expect(end_snapshot.load() == STATERR_NONE,
				"end-save: ram_state restore failed");
			expect(bool(space.read_word(FMA_STATUS) & cdi_dvc::FMA_IRQ_END_ISO),
				"end-save: program-end status was not restored");
			space.read_word(FMA_IRQ_STATUS);
			space.write_word(FMA_STREAM, 7);
			feed_audio_frame(space, audio_frame);
			expect(space.read_word(FMA_CURRENT_STREAM) == 2,
				"end-save: ended input incorrectly accepted a new frame");
			expect(!(space.read_word(FMA_IRQ_STATUS) & cdi_dvc::FMA_IRQ_FRAME_DECODED),
				"end-save: ended input fabricated a decoded-frame event");
		}

		for (unsigned cycle = 0; cycle < 16; ++cycle)
		{
			uint16_t const stream = uint16_t((cycle * 5U + 3U) & 0x1fU);
			space.write_word(FMA_COMMAND, 0x0001);
			space.write_word(FMA_STREAM, stream);
			space.read_word(FMA_IRQ_STATUS);
			feed_audio_frame(space, audio_frame);
			expect(space.read_word(FMA_CURRENT_STREAM) == stream,
				util::string_format("rapid-stop-start: stream %u did not commit", stream));
			expect(bool(space.read_word(FMA_IRQ_STATUS) & cdi_dvc::FMA_IRQ_FRAME_DECODED),
				util::string_format("rapid-stop-start: stream %u did not decode", stream));
		}

		// 3. Save while both opaque PL_MPEG backends contain meaningful state.
		// Audio has a decoded frame queued; video has a valid sequence header and
		// an active playback command.  Replay the same long continuation before
		// and after load and require identical guest-visible hashes.
		constexpr uint8_t audio_stream = 3;
		constexpr uint8_t video_stream = 2;
		space.write_word(FMA_COMMAND, 0x0001);
		space.write_word(FMA_STREAM, audio_stream);
		space.read_word(FMA_IRQ_STATUS);
		feed_audio_frame(space, audio_frame);
		expect(space.read_word(FMA_CURRENT_STREAM) == audio_stream,
			"av-save: audio stream did not commit");
		space.read_word(FMA_IRQ_STATUS);

		space.write_word(FMV_STREAM, video_stream);
		feed_video_payload(space, video_stream,
			{ 0x00, 0x00, 0x01, 0xb3,
			  0x01, 0x00, 0x10, 0x13, 0x00, 0xfa, 0x20, 0xa0 });
		expect(space.read_word(FMV_FRAME_PERIOD) == 3'600,
			"av-save: synthetic 25 Hz video sequence header was not accepted");
		space.read_word(FMV_IRQ_STATUS);

		{
			ram_state av_snapshot(machine().save());
			expect(av_snapshot.save() == STATERR_NONE,
				"av-save: simultaneous A/V ram_state capture failed");

			uint32_t const live_hash = run_postload_continuation(
				space, audio_frame, audio_stream, video_stream);
			expect(av_snapshot.load() == STATERR_NONE,
				"av-save: simultaneous A/V ram_state restore failed");
			expect(space.read_word(FMA_STREAM) == audio_stream,
				"av-save: audio requested stream was not restored");
			expect(space.read_word(FMA_CURRENT_STREAM) == audio_stream,
				"av-save: audio current stream was not restored");
			expect(space.read_word(FMV_STREAM) == video_stream,
				"av-save: video stream was not restored");
			expect(space.read_word(FMV_FRAME_PERIOD) == 3'600,
				"av-save: reconstructed video header lost its 25 Hz period");

			uint32_t const restored_hash = run_postload_continuation(
				space, audio_frame, audio_stream, video_stream);
			expect(restored_hash == live_hash,
				util::string_format(
					"av-save: long continuation hash mismatch live=%08x restored=%08x",
					live_hash, restored_hash));
		}

		m_completed = true;
		machine().schedule_exit();
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

ROM_START(cdiavsave)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdiavsave, 0, cdi_dvc_state_integration, cdi_dma_integration,
	cdi_dvc_state_integration_state, empty_init, ROT0, "MAME",
	"CD-i DVC simultaneous A/V save-state integration fixture", 0)

void run_dvc_state_integration_fixture()
{
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdiavsave).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdiavsave), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_dvc_state_integration_state &>(machine.root_device());
	manager.set_machine(nullptr);

	std::string diagnostics;
	for (std::string const &failure : state.failures())
	{
		if (!diagnostics.empty())
			diagnostics.append("\n");
		diagnostics.append(failure);
	}

	INFO(diagnostics);
	REQUIRE(error == EMU_ERR_NONE);
	REQUIRE(state.completed());
	REQUIRE(state.failures().empty());
}

} // anonymous namespace

TEST_CASE(
	"CD-i DVC live stream end stop-start and simultaneous A/V state survive save/load",
	"[emu][philips][cdi][dvc][save][audio][video][integration]")
{
	run_dvc_state_integration_fixture();
}
