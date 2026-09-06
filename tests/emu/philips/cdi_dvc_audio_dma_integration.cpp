// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// This file is included by cdi_dvc_dma_test_support.cpp after the shared
// full-machine test OSD/manager definitions from cdi_dvc_dma_integration.cpp.

namespace
{

class cdi_dvc_audio_dma_state : public cdi_state
{
public:
	cdi_dvc_audio_dma_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_dvc_audio_dma(machine_config &config)
	{
		cdimono1dvc(config);
	}

	bool completed() const { return m_completed; }
	std::vector<std::string> const &failures() const { return m_failures; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_dvc_audio_dma_state::test_step), this);
	}

	void machine_reset() override
	{
		cdi_state::machine_reset();
		m_failures.clear();
		m_completed = false;
		m_test_timer->adjust(attotime::zero, 0);
	}

private:
	static constexpr uint32_t DMA2_STATUS = 0x80004040U;
	static constexpr uint32_t DMA2_CONTROL = 0x80004044U;
	static constexpr uint32_t DMA2_SEQUENCE = 0x80004046U;
	static constexpr uint32_t DMA2_COUNTER = 0x8000404aU;
	static constexpr uint32_t DMA2_MAC_HI = 0x8000404cU;
	static constexpr uint32_t DMA2_MAC_LO = 0x8000404eU;
	static constexpr uint32_t FMA_COMMAND = 0x00e03000U;
	static constexpr uint32_t FMA_CURRENT_STREAM = 0x00e0300aU;
	static constexpr uint32_t FMA_IRQ_STATUS = 0x00e0301aU;
	static constexpr uint32_t FMA_IRQ_ENABLE = 0x00e0301cU;
	static constexpr uint32_t SOURCE = 0x00d01000U;
	static constexpr uint8_t DMA_IRQ_LEVEL = 3;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	static std::vector<uint8_t> make_silent_layer2_frame()
	{
		constexpr uint32_t header =
			(0x7ffU << 21)
			| (3U << 19)
			| (2U << 17)
			| (1U << 16)
			| (10U << 12); // 192 kbit/s, 44.1 kHz, stereo
		auto const decoded = cdi_dvc::decode_mpeg1_layer2_audio_header(header);
		if (!decoded.valid || decoded.frame_size_bytes < 4)
			return { };

		std::vector<uint8_t> frame(decoded.frame_size_bytes, 0);
		frame[0] = uint8_t(header >> 24);
		frame[1] = uint8_t(header >> 16);
		frame[2] = uint8_t(header >> 8);
		frame[3] = uint8_t(header);
		if (frame.size() & 1U)
			frame.push_back(0);
		return frame;
	}

	void program_dma(address_space &space, uint16_t count)
	{
		space.write_word(DMA2_CONTROL, 0x3010); // M2D, word operand
		space.write_word(DMA2_SEQUENCE, 0x040b); // increment MAC, INE, IPL3
		space.write_word(DMA2_COUNTER, count);
		space.write_word(DMA2_MAC_HI, uint16_t(SOURCE >> 16));
		space.write_word(DMA2_MAC_LO, uint16_t(SOURCE));
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		address_space &space = m_maincpu->space(AS_PROGRAM);
		switch (param)
		{
		case 0:
		{
			m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);
			std::vector<uint8_t> const frame = make_silent_layer2_frame();
			expect(!frame.empty(), "audio-dma: failed to construct legal Layer II frame");
			if (frame.empty())
			{
				m_completed = true;
				machine().schedule_exit();
				break;
			}

			for (std::size_t index = 0; index < frame.size(); index += 2)
			{
				uint16_t const word = uint16_t(frame[index]) << 8 | frame[index + 1];
				space.write_word(SOURCE + uint32_t(index), word);
			}

			uint16_t const words = uint16_t(frame.size() / 2U);
			program_dma(space, words);
			space.write_word(FMA_IRQ_ENABLE, 0xffff);
			space.write_word(FMA_COMMAND, 0x8000);

			expect(m_dvc_dma_service_active,
				"audio-dma: live DVC request did not arm SCC channel 2 service");
			expect(m_maincpu->dma_channel_active(1),
				"audio-dma: SCC channel 2 CA did not assert");
			expect(m_maincpu->dma_channel_remaining(1) == words,
				"audio-dma: SCC transfer count did not latch full frame");

			// First word is serviced at zero delay; each remaining word advances
			// by the validated two-SCC-clock service cadence.
			m_test_timer->adjust(attotime::from_ticks(
				uint64_t(words) * 2U + 8U, m_maincpu->clock()), 1);
			break;
		}

		case 1:
		{
			expect(!m_dvc_dma_service_active,
				"audio-dma: service remained active after exact frame completion");
			expect(!m_maincpu->dma_channel_active(1),
				"audio-dma: SCC CA remained active after exact frame completion");
			expect(m_maincpu->dma_channel_remaining(1) == 0,
				"audio-dma: SCC count did not reach zero");
			expect(m_dvc_dma_service_events != 0,
				"audio-dma: no live SCC-to-DVC service events were observed");
			expect((space.read_word(FMA_COMMAND) & 0x8000U) == 0,
				"audio-dma: DVC completion did not clear the FMA request bit");
			expect(space.read_word(FMA_CURRENT_STREAM) == 0,
				"audio-dma: Layer II header did not commit stream 0");

			uint16_t const fma_irq = space.read_word(FMA_IRQ_STATUS);
			expect(bool(fma_irq & cdi_dvc::FMA_IRQ_DECODING_STARTED),
				"audio-dma: decoder did not report header/start across live DMA");
			expect(bool(fma_irq & cdi_dvc::FMA_IRQ_FRAME_DECODED),
				"audio-dma: decoder did not produce the exact DMA-delivered frame");

			uint16_t const dma_status = space.read_word(DMA2_STATUS);
			expect(bool(dma_status & 0x8000U),
				"audio-dma: SCC COC missing after frame delivery");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == ASSERT_LINE,
				"audio-dma: SCC completion interrupt missing");
			space.write_word(DMA2_STATUS, 0x8000);
			m_test_timer->adjust(attotime::from_ticks(1, m_maincpu->clock()), 2);
			break;
		}

		case 2:
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == CLEAR_LINE,
				"audio-dma: SCC completion interrupt did not clear after acknowledgement");
			m_completed = true;
			machine().schedule_exit();
			break;
		}
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

ROM_START(cdiaudma)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdiaudma, 0, cdi_dvc_audio_dma, cdi_dma_integration,
	cdi_dvc_audio_dma_state, empty_init, ROT0, "MAME",
	"CD-i DVC Layer II live DMA integration fixture", 0)

void run_dvc_audio_dma_fixture()
{
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdiaudma).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdiaudma), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_dvc_audio_dma_state &>(machine.root_device());
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
	"CD-i DVC Layer II frame crosses live SCC68070 DMA and reaches the decoder exactly",
	"[emu][philips][cdi][dvc][dma][audio][integration]")
{
	run_dvc_audio_dma_fixture();
}
