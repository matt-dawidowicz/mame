// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include "emu.h"
#include "cdi.h"

#include "catch.hpp"
#include "emuopts.h"
#include "main.h"
#include "osdepend.h"
#include "render.h"
#include "ui/menuitem.h"
#include "ui/uimain.h"

#include <memory>
#include <string>
#include <vector>

namespace
{

class cdi_dma_test_osd : public osd_interface
{
public:
	void init(running_machine &machine) override { machine.render().target_alloc(nullptr, RENDER_CREATE_NO_ART); }
	void update(bool skip_redraw) override { }
	void input_update(bool relative_reset) override { }
	void check_osd_inputs() override { }
	void set_verbose(bool print_verbose) override { }

	void init_debugger() override { }
	void wait_for_debugger(device_t &device, bool firststop) override { }

	bool no_sound() override { return true; }
	bool sound_external_per_channel_volume() override { return false; }
	bool sound_split_streams_per_source() override { return false; }
	uint32_t sound_get_generation() override { return 0; }
	osd::audio_info sound_get_information() override { return { }; }
	uint32_t sound_stream_sink_open(uint32_t node, std::string name, uint32_t rate) override { return 0; }
	uint32_t sound_stream_source_open(uint32_t node, std::string name, uint32_t rate) override { return 0; }
	void sound_stream_close(uint32_t id) override { }
	void sound_stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) override { }
	void sound_stream_source_update(uint32_t id, int16_t *buffer, int samples_this_frame) override { }
	void sound_stream_set_volumes(uint32_t id, const std::vector<float> &db) override { }
	void sound_begin_update() override { }
	void sound_end_update() override { }

	void customize_input_type_list(std::vector<input_type_entry> &typelist) override { }
	void add_audio_to_recording(const int16_t *buffer, int samples_this_frame) override { }
	std::vector<ui::menu_item> get_slider_list() override { return { }; }

	osd_font::ptr font_alloc() override { return { }; }
	bool get_font_families(std::string const &font_path, std::vector<std::pair<std::string, std::string>> &result) override { return false; }

	bool execute_command(const char *command) override { return false; }

	std::unique_ptr<osd::midi_input_port> create_midi_input(std::string_view name) override { return { }; }
	std::unique_ptr<osd::midi_output_port> create_midi_output(std::string_view name) override { return { }; }
	std::vector<osd::midi_port_info> list_midi_ports() override { return { }; }

	std::unique_ptr<osd::network_device> open_network_device(int id, osd::network_handler &handler) override { return { }; }
	std::vector<osd::network_device_info> list_network_devices() override { return { }; }
};

class cdi_dma_test_manager : public machine_manager
{
public:
	cdi_dma_test_manager(emu_options &options, osd_interface &osd)
		: machine_manager(options, osd)
	{
		start_http_server();
	}

	ui_manager *create_ui(running_machine &machine) override
	{
		m_ui = std::make_unique<ui_manager>(machine);
		return m_ui.get();
	}

private:
	std::unique_ptr<ui_manager> m_ui;
};

class cdi_dma_integration_state : public cdi_state
{
public:
	cdi_dma_integration_state(const machine_config &mconfig, device_type type, const char *tag)
		: cdi_state(mconfig, type, tag)
	{
	}

	void cdi_dma_integration(machine_config &config)
	{
		cdimono1dvc(config);
	}

	bool completed() const { return m_completed; }
	std::vector<std::string> const &failures() const { return m_failures; }

protected:
	void machine_start() override
	{
		cdi_state::machine_start();
		m_test_timer = timer_alloc(FUNC(cdi_dma_integration_state::test_step), this);
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
	static constexpr uint32_t DVC_FMA_COMMAND = 0x00e03000U;
	static constexpr uint32_t SOURCE = 0x00d00100U;
	static constexpr uint16_t WORDS = 3;
	static constexpr uint8_t DMA_IRQ_LEVEL = 3;

	void expect(bool condition, std::string message)
	{
		if (!condition)
			m_failures.push_back(std::move(message));
	}

	void expect_remaining(uint16_t expected, char const *where)
	{
		uint16_t const actual = m_maincpu->dma_channel_remaining(1);
		if (actual != expected)
		{
			m_failures.push_back(util::string_format(
					"%s: DMA2 remaining expected %u, got %u", where, expected, actual));
		}
	}

	void expect_address(uint32_t expected, char const *where)
	{
		uint32_t const actual = m_maincpu->dma_channel_memory_address(1);
		if (actual != expected)
		{
			m_failures.push_back(util::string_format(
					"%s: DMA2 MAC expected %08x, got %08x", where, expected, actual));
		}
	}

	void expect_events(uint32_t expected, char const *where)
	{
		if (m_dvc_dma_service_events != expected)
		{
			m_failures.push_back(util::string_format(
					"%s: DVC service events expected %u, got %u",
					where, expected, m_dvc_dma_service_events));
		}
	}

	bool fma_dma_requested(address_space &space)
	{
		return bool(space.read_word(DVC_FMA_COMMAND) & 0x8000U);
	}

	TIMER_CALLBACK_MEMBER(test_step)
	{
		address_space &space = m_maincpu->space(AS_PROGRAM);

		switch (param)
		{
		case 0:
		{
			// Keep the SCC core present but stop guest instruction execution. The
			// integration fixture drives only the real mapped RAM/register path.
			m_maincpu->suspend(SUSPEND_REASON_DISABLE, true);

			space.write_word(SOURCE + 0, 0x1234);
			space.write_word(SOURCE + 2, 0xabcd);
			space.write_word(SOURCE + 4, 0x55aa);

			// Channel 2: memory-to-device, word operand, MAC increment, fixed DAC,
			// completion interrupt enabled at IPL3. Program through the SCC register
			// map so byte-lane/control semantics are part of the regression.
			space.write_word(DMA2_CONTROL, 0x3010);
			space.write_word(DMA2_SEQUENCE, 0x040b);
			space.write_word(DMA2_COUNTER, WORDS);
			space.write_word(DMA2_MAC_HI, uint16_t(SOURCE >> 16));
			space.write_word(DMA2_MAC_LO, uint16_t(SOURCE));

			bool increment = false;
			expect(m_maincpu->dma_channel_memory_to_device(1),
					"program: DMA2 is not memory-to-device");
			expect(m_maincpu->dma_channel_word_transfer(1),
					"program: DMA2 is not a word transfer");
			expect(m_maincpu->dma_channel_memory_increment(1, increment) && increment,
					"program: DMA2 MAC increment mode was not accepted");
			expect_remaining(WORDS, "program");
			expect_address(SOURCE, "program");
			expect(!m_maincpu->dma_channel_active(1),
					"program: DMA2 became active before peripheral DREQ");

			// Use the real readable FMA DMA command as the DVC-side completion
			// observable. It asserts DREQ here and dma_done() clears bit 15.
			space.write_word(DVC_FMA_COMMAND, 0x8000);
			expect(fma_dma_requested(space),
					"start: DVC FMA DMA request bit did not assert");
			expect(m_dvc_dma_service_active,
					"start: CD-i DVC DMA service did not arm");
			expect(m_maincpu->dma_channel_active(1),
					"start: SCC DMA2 did not enter CA state on DREQ");
			expect_events(0, "start");
			expect_remaining(WORDS, "start");
			expect_address(SOURCE, "start");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == CLEAR_LINE,
					"start: DMA completion IRQ asserted before any transfer");

			// dvc_dma_req_w schedules the first production service tick at zero.
			// Observe one SCC clock later, before the second tick at +2 clocks.
			m_test_timer->adjust(attotime::from_ticks(1, m_maincpu->clock()), 1);
			break;
		}

		case 1:
			expect_events(1, "word1");
			expect_remaining(2, "word1");
			expect_address(SOURCE + 2, "word1");
			expect(m_maincpu->dma_channel_active(1),
					"word1: DMA2 stopped before the counter expired");
			expect(m_dvc_dma_service_active,
					"word1: DVC service stopped before the counter expired");
			expect(fma_dma_requested(space),
					"word1: DVC completed before the SCC counter expired");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == CLEAR_LINE,
					"word1: completion IRQ asserted too early");
			m_test_timer->adjust(attotime::from_ticks(2, m_maincpu->clock()), 2);
			break;

		case 2:
			expect_events(2, "word2");
			expect_remaining(1, "word2");
			expect_address(SOURCE + 4, "word2");
			expect(m_maincpu->dma_channel_active(1),
					"word2: DMA2 stopped before the final word");
			expect(m_dvc_dma_service_active,
					"word2: DVC service stopped before the final word");
			expect(fma_dma_requested(space),
					"word2: DVC completed before the final word");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == CLEAR_LINE,
					"word2: completion IRQ asserted before the final word");
			m_test_timer->adjust(attotime::from_ticks(2, m_maincpu->clock()), 3);
			break;

		case 3:
		{
			expect_events(3, "complete");
			expect_remaining(0, "complete");
			expect_address(SOURCE + 6, "complete");
			expect(!m_maincpu->dma_channel_active(1),
					"complete: DMA2 CA remained active after counter reached zero");
			expect(!m_dvc_dma_service_active,
					"complete: driver service remained active after final word");
			expect(!fma_dma_requested(space),
					"complete: DVC dma_done() did not clear the FMA DMA request bit");

			uint16_t const status = space.read_word(DMA2_STATUS);
			expect(bool(status & 0x8000U),
					"complete: DMA2 COC was not asserted");
			expect(!(status & 0x0800U),
					"complete: DMA2 CA was still visible in the status register");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == ASSERT_LINE,
					"complete: DMA2 COC+INE did not assert IPL3");

			// Wait through another full service period. No fourth transfer is legal.
			m_test_timer->adjust(attotime::from_ticks(2, m_maincpu->clock()), 4);
			break;
		}

		case 4:
			expect_events(3, "quiescent");
			expect_remaining(0, "quiescent");
			expect_address(SOURCE + 6, "quiescent");
			expect(!m_dvc_dma_service_active,
					"quiescent: service re-armed after completion without a new DREQ");
			expect(!fma_dma_requested(space),
					"quiescent: DVC DMA request reasserted after completion");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == ASSERT_LINE,
					"quiescent: completion IRQ dropped before COC acknowledgement");

			// DMA status uses write-one-to-clear for COC. Exercise the real register
			// acknowledgement and observe the interrupt edge independently of DVC.
			space.write_word(DMA2_STATUS, 0x8000);
			m_test_timer->adjust(attotime::from_ticks(1, m_maincpu->clock()), 5);
			break;

		case 5:
		{
			uint16_t const status = space.read_word(DMA2_STATUS);
			expect(!(status & 0x8000U),
					"ack: DMA2 COC did not clear through the status register");
			expect(m_maincpu->input_line_state(DMA_IRQ_LEVEL) == CLEAR_LINE,
					"ack: IPL3 remained asserted after COC acknowledgement");
			expect_events(3, "ack");
			expect_remaining(0, "ack");
			expect_address(SOURCE + 6, "ack");

			m_completed = true;
			machine().schedule_exit();
			break;
		}
		}
	}

	emu_timer *m_test_timer = nullptr;
	bool m_completed = false;
	std::vector<std::string> m_failures;
};

static INPUT_PORTS_START(cdi_dma_integration)
	PORT_START("MOUSEX")
	PORT_BIT(0xffff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("MOUSEY")
	PORT_BIT(0xffff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("MOUSEBTN")
	PORT_BIT(0xffff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("TESTPLUG")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END

ROM_START(cdidmaint)
	ROM_REGION16_BE(0x80000, "maincpu", ROMREGION_ERASE00)
ROM_END

GAME(2026, cdidmaint, 0, cdi_dma_integration, cdi_dma_integration,
	cdi_dma_integration_state, empty_init, ROT0, "MAME",
	"CD-i DVC DMA integration fixture", 0)

} // anonymous namespace

TEST_CASE(
	"CD-i DVC DMA traverses the live SCC68070 channel 2 one word per service tick",
	"[emu][philips][cdi][dvc][dma][integration]")
{
	emu_options options;
	options.set_system_name(std::string(GAME_NAME(cdidmaint).name));
	cdi_dma_test_osd osd;
	cdi_dma_test_manager manager(options, osd);
	machine_config config(GAME_NAME(cdidmaint), options);
	running_machine machine(config, manager);
	manager.set_machine(&machine);

	int const error = machine.run(true);
	auto &state = downcast<cdi_dma_integration_state &>(machine.root_device());
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
