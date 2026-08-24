// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/******************************************************************************

    CD-i Mono-I SLAVE MCU simulation
    -------------------

*******************************************************************************

STATUS:
- Functional Mono-I command, pointer, response, and IRQ transport HLE.

TODO:
- Keep the HLE until SCC68070 /DTACK and the MCU bus wiring can support a
  trustworthy LLE implementation.
- Implement currently classified unknown/unimplemented commands only from
  firmware, hardware documentation, or bus traces.

*******************************************************************************/

#include "emu.h"
#include "cdislavehle.h"
#include "cdislavehle_pointer.h"
#include "cdislavehle_state.h"
#include "cdislavehle_transport.h"

#define LOG_IRQS        (1U << 1)
#define LOG_COMMANDS    (1U << 2)
#define LOG_READS       (1U << 3)
#define LOG_WRITES      (1U << 4)
#define LOG_UNKNOWNS    (1U << 5)
#define LOG_INPUTS      (1U << 6)
#define LOG_ALL         (LOG_IRQS | LOG_COMMANDS | LOG_READS | LOG_WRITES | LOG_UNKNOWNS | LOG_INPUTS)

#define VERBOSE         (LOG_INPUTS)
#include "logmacro.h"

// device type definition
DEFINE_DEVICE_TYPE(CDI_SLAVE_HLE, cdislave_hle_device, "cdislavehle", "CD-i Mono-I Slave HLE")


//**************************************************************************
//  MEMBER FUNCTIONS
//**************************************************************************

TIMER_CALLBACK_MEMBER( cdislave_hle_device::trigger_readback_int )
{
	const attotime now = machine().time();

	bool irq_pending = false;
	attotime next_delay = attotime::never;

	for (auto &channel : m_channel)
	{
		if (!cdi_slave_transport::response_window_fits(
				channel.m_out_index, channel.m_out_count,
				sizeof(channel.m_out_buf)))
		{
			LOGMASKED(LOG_UNKNOWNS,
				"SLAVE transport: discarding invalid output window index=%u count=%u capacity=%u\n",
				channel.m_out_index, channel.m_out_count,
				unsigned(sizeof(channel.m_out_buf)));
			memset(channel.m_out_buf, 0, sizeof(channel.m_out_buf));
			channel.m_out_index = 0;
			channel.m_out_count = 0;
			channel.m_out_cmd = 0;
			channel.m_out_ready = false;
			channel.m_out_irq = false;
			channel.m_out_deadline = attotime::never;
			continue;
		}

		const bool pending = channel.m_out_count != 0;

		if (!pending)
			continue;

		if (channel.m_out_irq
				&& !channel.m_out_ready
				&& !channel.m_out_deadline.is_never()
				&& channel.m_out_deadline <= now)
		{
			channel.m_out_ready = true;
		}

		if (cdi_slave_transport::response_holds_irq(
				pending && channel.m_out_irq,
				channel.m_out_ready))
		{
			irq_pending = true;
		}

		if (channel.m_out_irq
				&& !channel.m_out_ready
				&& !channel.m_out_deadline.is_never())
		{
			const attotime remaining =
					channel.m_out_deadline > now
						? channel.m_out_deadline - now
						: attotime::zero;

			next_delay =
					cdi_slave_transport::select_interrupt_delay(
						next_delay,
						remaining);
		}
	}

	LOGMASKED(
			LOG_IRQS,
			"%s IRQ2\n",
			irq_pending ? "Asserting" : "De-asserting");

	m_int_callback(irq_pending ? ASSERT_LINE : CLEAR_LINE);
	m_interrupt_timer->adjust(next_delay);
}

TIMER_CALLBACK_MEMBER( cdislave_hle_device::poll_inputs )
{
	const uint16_t x = m_read_mousex();
	const uint16_t y = m_read_mousey();
	const uint8_t btn = m_read_mousebtn();

	const bool changed = cdi_slave_pointer::host_sample_changed(
		m_input_mouse_initialized,
		m_input_mouse_x,
		m_input_mouse_y,
		m_input_mouse_btn,
		x,
		y,
		btn);
	if (!changed)
		return;

	LOGMASKED(LOG_INPUTS,
		"CDI_INPUT_TRACE sample x=%04x y=%04x buttons=%02x initialized=%u enabled=%u ctx=%s\n",
		x, y, btn, m_input_mouse_initialized ? 1U : 0U,
		m_pointer_input_enabled ? 1U : 0U, machine().describe_context());

	const auto movement = cdi_slave_pointer::decode_host_movement(
		m_input_mouse_initialized,
		m_input_mouse_x,
		m_input_mouse_y,
		x,
		y);

	m_input_mouse_x = x;
	m_input_mouse_y = y;
	m_input_mouse_btn = btn;
	m_input_mouse_initialized = true;

	m_device_mouse_x =
		cdi_slave_pointer::clamp_x(
			int32_t(m_device_mouse_x) + movement.x);
	m_device_mouse_y =
		cdi_slave_pointer::clamp_y(
			int32_t(m_device_mouse_y) + movement.y);

	LOGMASKED(LOG_INPUTS,
		"CDI_INPUT_TRACE applied dx=%d dy=%d device_x=%d device_y=%d buttons=%02x enabled=%u\n",
		movement.x, movement.y, m_device_mouse_x, m_device_mouse_y, btn,
		m_pointer_input_enabled ? 1U : 0U);

	if (m_pointer_input_enabled)
		prepare_pointer_readback();
}

void cdislave_hle_device::prepare_readback(const attotime &delay, uint8_t channel, uint8_t count, uint8_t data0, uint8_t data1, uint8_t data2, uint8_t data3, uint8_t cmd)
{
	constexpr std::size_t channel_count = sizeof(m_channel) / sizeof(m_channel[0]);
	if (!cdi_slave_transport::channel_index_valid(channel, channel_count)
			|| !cdi_slave_transport::response_window_fits(
				0, count, sizeof(m_channel[0].m_out_buf)))
	{
		LOGMASKED(LOG_UNKNOWNS,
			"SLAVE transport: refusing invalid readback channel=%u count=%u\n",
			unsigned(channel), unsigned(count));
		return;
	}

	channel_state &state = m_channel[channel];

	state.m_out_index = 0;
	state.m_out_count = count;
	state.m_out_buf[0] = data0;
	state.m_out_buf[1] = data1;
	state.m_out_buf[2] = data2;
	state.m_out_buf[3] = data3;
	state.m_out_cmd = cmd;

	state.m_out_irq = !delay.is_never();

	const bool immediate =
			delay == attotime::zero || delay.is_never();

	state.m_out_ready =
			cdi_slave_transport::response_ready_on_prepare(immediate);

	state.m_out_deadline =
			state.m_out_irq && !state.m_out_ready
				? machine().time() + delay
				: attotime::never;

	// Re-evaluate all channels immediately.  This is required not only when a
	// new response will assert IRQ2, but also when a new response replaces a
	// previously ready response and therefore needs to de-assert IRQ2.
	m_interrupt_timer->adjust(attotime::zero);
}

uint16_t cdislave_hle_device::slave_r(offs_t offset)
{
	constexpr std::size_t channel_count = sizeof(m_channel) / sizeof(m_channel[0]);
	if (!cdi_slave_transport::channel_index_valid(std::size_t(offset), channel_count))
	{
		LOGMASKED(LOG_READS | LOG_UNKNOWNS,
			"slave_r: invalid channel %u\n", unsigned(offset));
		return 0xff;
	}

	channel_state &state = m_channel[offset];
	if (!cdi_slave_transport::response_window_fits(
			state.m_out_index, state.m_out_count, sizeof(state.m_out_buf)))
	{
		LOGMASKED(LOG_READS | LOG_UNKNOWNS,
			"slave_r: Channel %u invalid output window index=%u count=%u\n",
			unsigned(offset), state.m_out_index, state.m_out_count);
		memset(state.m_out_buf, 0, sizeof(state.m_out_buf));
		state.m_out_index = 0;
		state.m_out_count = 0;
		state.m_out_cmd = 0;
		state.m_out_ready = false;
		state.m_out_irq = false;
		state.m_out_deadline = attotime::never;
		m_interrupt_timer->adjust(attotime::zero);
		return 0xff;
	}

	if (cdi_slave_transport::response_readable(
		state.m_out_count != 0,
		state.m_out_ready))
	{
		uint8_t ret = state.m_out_buf[state.m_out_index];
		LOGMASKED(LOG_READS, "%s: slave_r: Channel %d: %d, %02x\n", machine().describe_context(), offset, state.m_out_index, ret);
		if (state.m_out_cmd == 0xf7)
			LOGMASKED(LOG_INPUTS,
				"CDI_INPUT_TRACE guest-read channel=%u index=%u value=%02x remaining=%u ctx=%s\n",
				unsigned(offset), state.m_out_index, ret,
				state.m_out_count, machine().describe_context());

		state.m_out_index++;
		state.m_out_count--;

		if (!state.m_out_count)
		{
			state.m_out_index = 0;
			state.m_out_cmd = 0;
			state.m_out_ready = false;
			state.m_out_irq = false;
			state.m_out_deadline = attotime::never;
			memset(state.m_out_buf, 0, sizeof(state.m_out_buf));
		}

		bool output_pending = false;
		for (const auto &channel : m_channel)
		{
			if (cdi_slave_transport::response_holds_irq(
				channel.m_out_count != 0 && channel.m_out_irq,
				channel.m_out_ready))
			{
				output_pending = true;
				break;
			}
		}

		if (!output_pending)
		{
			LOGMASKED(LOG_IRQS, "slave_r: De-asserting IRQ2\n");
			m_int_callback(CLEAR_LINE);
		}

		return ret;
	}

	LOGMASKED(LOG_READS, "slave_r: Channel %d: %d (nothing to output)\n", offset, state.m_out_index);
	return 0xff;
}

void cdislave_hle_device::set_mouse_position()
{
	const auto position = cdi_slave_pointer::decode_set_position(
		m_in_buf[0], m_in_buf[1], m_in_buf[2]);

	m_device_mouse_x = position.x;
	m_device_mouse_y = position.y;
}

void cdislave_hle_device::prepare_pointer_readback()
{
	const auto packet = cdi_slave_pointer::encode_readback(
		m_device_mouse_x, m_device_mouse_y, m_input_mouse_btn);

	LOGMASKED(LOG_INPUTS,
		"CDI_INPUT_TRACE packet bytes=%02x,%02x,%02x,%02x device_x=%d device_y=%d buttons=%02x\n",
		packet[0], packet[1], packet[2], packet[3],
		m_device_mouse_x, m_device_mouse_y, m_input_mouse_btn);

	prepare_readback(
		attotime::zero,
		0,
		4,
		packet[0],
		packet[1],
		packet[2],
		packet[3],
		0xf7);
}

void cdislave_hle_device::clear_input()
{
	memset(m_in_buf, 0, sizeof(m_in_buf));
	m_in_channel = cdi_slave_hle::NO_CHANNEL;
	m_in_index = 0;
	m_in_count = 0;
}

void cdislave_hle_device::execute_command(cdi_slave_hle::command_descriptor descriptor)
{
	switch (descriptor.kind)
	{
	case cdi_slave_hle::command::pointer_enable:
		LOGMASKED(LOG_COMMANDS, "slave_w: Enable Pointer Input (0x83)\n");
		m_pointer_input_enabled = true;
		LOGMASKED(LOG_INPUTS, "CDI_INPUT_TRACE pointer-enable channel=0\n");
		prepare_pointer_readback();
		break;

	case cdi_slave_hle::command::pointer_disable:
		LOGMASKED(LOG_COMMANDS, "slave_w: Disable Pointer Input (0x84)\n");
		m_pointer_input_enabled = false;
		LOGMASKED(LOG_INPUTS, "CDI_INPUT_TRACE pointer-disable channel=0\n");
		break;

	case cdi_slave_hle::command::pointer_position:
		LOGMASKED(LOG_COMMANDS, "slave_w: Update Mouse Position (0x%02x)\n", m_in_buf[0]);
		set_mouse_position();
		break;

	case cdi_slave_hle::command::keyboard_enable:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Enable Keyboard Events (0x80); event delivery unimplemented\n");
		m_keyboard_events_enabled = true;
		break;

	case cdi_slave_hle::command::audio_mute:
		LOGMASKED(LOG_COMMANDS, "slave_w: Mute Audio (0x82)\n");
		m_dmadac[0]->set_volume(0);
		m_dmadac[1]->set_volume(0);
		break;

	case cdi_slave_hle::command::audio_unmute:
		LOGMASKED(LOG_COMMANDS, "slave_w: Unmute Audio (0x83)\n");
		m_dmadac[0]->set_volume(0x100);
		m_dmadac[1]->set_volume(0x100);
		break;

	case cdi_slave_hle::command::cpu_reset:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Reset Main CPU (0x8a)\n");
		// The SLAVE owns system reset in the board documentation.  Pulse width
		// is not documented here, so retain the existing compatibility pulse.
		m_reset_callback(ASSERT_LINE);
		m_reset_callback(CLEAR_LINE);
		break;

	case cdi_slave_hle::command::audio_attenuation:
		LOGMASKED(LOG_COMMANDS, "slave_w: Set Attenuation Audio (0x%02x)\n", m_in_buf[0]);
		m_atten_w(
			(uint32_t(m_in_buf[1]) << 24) |
			(uint32_t(m_in_buf[2]) << 16) |
			(uint32_t(m_in_buf[3]) << 8) |
			uint32_t(m_in_buf[4]));
		break;

	case cdi_slave_hle::command::lcd_write:
		LOGMASKED(LOG_COMMANDS, "slave_w: Set Front Panel LCD (0xf0)\n");
		memcpy(m_lcd_state, m_in_buf + 1, sizeof(m_lcd_state));
		break;

	case cdi_slave_hle::command::memory_set:
		LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Set unknown memory (0x80); unimplemented\n");
		break;

	case cdi_slave_hle::command::memory_clear:
		LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Clear unknown memory (0x81); unimplemented\n");
		break;

	case cdi_slave_hle::command::disc_status:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Request Disc Status (0xb0)\n");
		// Payload and 250 ms delay are retained compatibility values.  There is
		// no live SERVO/CD-processor interface from which to derive them yet.
		prepare_readback(
			attotime::from_hz(4),
			descriptor.response_channel,
			descriptor.response_length,
			0xb0, 0x00, 0x02, 0x15, 0xb0);
		break;

	case cdi_slave_hle::command::disc_base:
		LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Request Disc Base (0xb1); unimplemented\n");
		// Do not fabricate a response without a known disc-base definition.
		break;

	case cdi_slave_hle::command::revision:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Request SLAVE Revision (0xf0)\n");
		// The service low-level test reads the echo, SLAVE release, and CD
		// processor release.  The fixed release values are compatibility data.
		prepare_readback(
			attotime::from_hz(10000),
			descriptor.response_channel,
			descriptor.response_length,
			0xf0, 0x32, 0x31, 0, 0xf0);
		break;

	case cdi_slave_hle::command::pointer_type:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Request Pointer Type (0xf3)\n");
		prepare_readback(
			attotime::from_hz(10000),
			descriptor.response_channel,
			descriptor.response_length,
			0xf3, 1, 0, 0, 0xf3);
		break;

	case cdi_slave_hle::command::test_plug:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Request Test Plug Status (0xf4)\n");
		prepare_readback(
			attotime::from_hz(10000),
			descriptor.response_channel,
			descriptor.response_length,
			0xf4, m_testplug_cb() ? 1 : 0, 0, 0, 0xf4);
		break;

	case cdi_slave_hle::command::video_standard:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Request NTSC/PAL Status (0xf6)\n");
		// Real observations establish F6 01 for NTSC and F6 02 for PAL.  The
		// immediately readable, non-IRQ timing remains an HLE compatibility rule.
		prepare_readback(
			attotime::never,
			descriptor.response_channel,
			descriptor.response_length,
			0xf6, m_ntsc ? 1 : 2, 0, 0, 0xf6);
		break;

	case cdi_slave_hle::command::developer_enable:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Arm Developer Mode (0xf7); side effects unimplemented\n");
		m_debug_mode = 1;
		break;

	case cdi_slave_hle::command::xbus_interrupt_enable:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: X-Bus Interrupt Enable (0xfa); routing unimplemented\n");
		m_xbus_interrupt_enable = 1;
		break;

	case cdi_slave_hle::command::developer_disable:
		LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Disarm Developer Mode (0xfe); side effects unimplemented\n");
		m_debug_mode = 0;
		break;

	case cdi_slave_hle::command::unknown:
		LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Refusing unknown command\n");
		break;
	}
}

void cdislave_hle_device::slave_w(offs_t offset, uint16_t data)
{
	const uint8_t value = data & 0x00ff;
	LOGMASKED(LOG_WRITES, "slave_w: Channel %d: %d = %02x\n", offset, m_in_index, value);

	constexpr std::size_t channel_count = sizeof(m_channel) / sizeof(m_channel[0]);
	if (!cdi_slave_transport::channel_index_valid(std::size_t(offset), channel_count))
	{
		LOGMASKED(LOG_WRITES | LOG_UNKNOWNS,
			"slave_w: invalid channel %u value=%02x\n",
			unsigned(offset), value);
		return;
	}

	if (!cdi_slave_transport::input_write_fits(m_in_index, sizeof(m_in_buf)))
	{
		LOGMASKED(LOG_UNKNOWNS,
			"slave_w: Channel %d: discarding overlength input command index=%u count=%u\n",
			offset, m_in_index, m_in_count);
		clear_input();
		return;
	}

	const cdi_slave_hle::parser_state parser =
	{
		m_in_channel,
		m_in_index ? m_in_buf[0] : uint8_t(0),
		m_in_index,
		m_in_count
	};
	const cdi_slave_hle::parser_transition transition =
		cdi_slave_hle::parse_byte(parser, uint8_t(offset), value, sizeof(m_in_buf));

	if (transition.result == cdi_slave_hle::parse_result::rejected)
	{
		if (m_in_index == 0)
		{
			LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS,
				"slave_w: Channel %d: Unknown command %02x\n", offset, value);
		}
		else
		{
			LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS,
				"slave_w: Channel %d: Rejecting malformed continuation %02x for channel %u command %02x\n",
				offset, value, m_in_channel, m_in_buf[0]);
		}
		clear_input();
		return;
	}

	m_in_buf[transition.write_index] = value;
	if (transition.result == cdi_slave_hle::parse_result::collecting)
	{
		m_in_channel = transition.next.origin_channel;
		m_in_index = transition.next.index;
		m_in_count = transition.next.count;
		return;
	}

	execute_command(transition.descriptor);
	clear_input();
}

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  cdislave_hle_device - constructor
//-------------------------------------------------

cdislave_hle_device::cdislave_hle_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, CDI_SLAVE_HLE, tag, owner, clock)
	, m_int_callback(*this)
	, m_reset_callback(*this)
	, m_read_mousex(*this, 0x0000)
	, m_read_mousey(*this, 0x0000)
	, m_read_mousebtn(*this, 0x00)
	, m_dmadac(*this, ":dac%u", 1U)
	, m_atten_w(*this)
	, m_testplug_cb(*this, 0)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void cdislave_hle_device::device_start()
{
	save_item(NAME(m_channel[0].m_out_buf[0]));
	save_item(NAME(m_channel[0].m_out_buf[1]));
	save_item(NAME(m_channel[0].m_out_buf[2]));
	save_item(NAME(m_channel[0].m_out_buf[3]));
	save_item(NAME(m_channel[0].m_out_index));
	save_item(NAME(m_channel[0].m_out_count));
	save_item(NAME(m_channel[0].m_out_cmd));
	save_item(NAME(m_channel[0].m_out_ready));
	save_item(NAME(m_channel[0].m_out_irq));
	save_item(NAME(m_channel[0].m_out_deadline));
	save_item(NAME(m_channel[1].m_out_buf[0]));
	save_item(NAME(m_channel[1].m_out_buf[1]));
	save_item(NAME(m_channel[1].m_out_buf[2]));
	save_item(NAME(m_channel[1].m_out_buf[3]));
	save_item(NAME(m_channel[1].m_out_index));
	save_item(NAME(m_channel[1].m_out_count));
	save_item(NAME(m_channel[1].m_out_cmd));
	save_item(NAME(m_channel[1].m_out_ready));
	save_item(NAME(m_channel[1].m_out_irq));
	save_item(NAME(m_channel[1].m_out_deadline));
	save_item(NAME(m_channel[2].m_out_buf[0]));
	save_item(NAME(m_channel[2].m_out_buf[1]));
	save_item(NAME(m_channel[2].m_out_buf[2]));
	save_item(NAME(m_channel[2].m_out_buf[3]));
	save_item(NAME(m_channel[2].m_out_index));
	save_item(NAME(m_channel[2].m_out_count));
	save_item(NAME(m_channel[2].m_out_cmd));
	save_item(NAME(m_channel[2].m_out_ready));
	save_item(NAME(m_channel[2].m_out_irq));
	save_item(NAME(m_channel[2].m_out_deadline));
	save_item(NAME(m_channel[3].m_out_buf[0]));
	save_item(NAME(m_channel[3].m_out_buf[1]));
	save_item(NAME(m_channel[3].m_out_buf[2]));
	save_item(NAME(m_channel[3].m_out_buf[3]));
	save_item(NAME(m_channel[3].m_out_index));
	save_item(NAME(m_channel[3].m_out_count));
	save_item(NAME(m_channel[3].m_out_cmd));
	save_item(NAME(m_channel[3].m_out_ready));
	save_item(NAME(m_channel[3].m_out_irq));
	save_item(NAME(m_channel[3].m_out_deadline));

	save_item(NAME(m_in_buf));
	save_item(NAME(m_in_channel));
	save_item(NAME(m_in_index));
	save_item(NAME(m_in_count));
	save_item(NAME(m_keyboard_events_enabled));

	save_item(NAME(m_debug_mode));

	save_item(NAME(m_xbus_interrupt_enable));

	save_item(NAME(m_lcd_state));

	save_item(NAME(m_input_mouse_x));
	save_item(NAME(m_input_mouse_y));
	save_item(NAME(m_input_mouse_btn));
	save_item(NAME(m_input_mouse_initialized));
	save_item(NAME(m_pointer_input_enabled));

	save_item(NAME(m_device_mouse_x));
	save_item(NAME(m_device_mouse_y));

	m_interrupt_timer = timer_alloc(FUNC(cdislave_hle_device::trigger_readback_int), this);
	m_interrupt_timer->adjust(attotime::never);

	m_input_poll_timer = timer_alloc(FUNC(cdislave_hle_device::poll_inputs), this);
	m_input_poll_timer->adjust(attotime::never);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void cdislave_hle_device::device_reset()
{
	for (auto & elem : m_channel)
	{
		memset(elem.m_out_buf, 0, sizeof(elem.m_out_buf));
		elem.m_out_index = 0;
		elem.m_out_count = 0;
		elem.m_out_cmd = 0;
		elem.m_out_ready = false;
		elem.m_out_irq = false;
		elem.m_out_deadline = attotime::never;
	}

	clear_input();
	m_keyboard_events_enabled = false;

	m_debug_mode = 0;

	m_xbus_interrupt_enable = 0;

	memset(m_lcd_state, 0, sizeof(m_lcd_state));

	m_input_mouse_x = 0xffff;
	m_input_mouse_y = 0xffff;
	m_input_mouse_btn = 0;
	m_input_mouse_initialized = false;

	m_pointer_input_enabled =
		cdi_slave_transport::reset_pointer_input_enabled(
			m_pointer_input_enabled);

	m_device_mouse_x = 0;
	m_device_mouse_y = 0;

	m_interrupt_timer->adjust(attotime::never);
	m_int_callback(CLEAR_LINE);

	m_input_poll_timer->adjust(attotime::from_hz(60), 0, attotime::from_hz(60));
}
