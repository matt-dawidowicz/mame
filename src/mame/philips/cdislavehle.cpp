// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/******************************************************************************

    CD-i Mono-I SLAVE MCU simulation
    -------------------

*******************************************************************************

STATUS:
- Just enough for the Mono-I CD-i board to work somewhat properly.

TODO:
- Proper LLE.

*******************************************************************************/

#include "emu.h"
#include "cdislavehle.h"
#include "cdislavehle_pointer.h"
#include "cdislavehle_transport.h"

#define LOG_IRQS        (1U << 1)
#define LOG_COMMANDS    (1U << 2)
#define LOG_READS       (1U << 3)
#define LOG_WRITES      (1U << 4)
#define LOG_UNKNOWNS    (1U << 5)
#define LOG_INPUTS      (1U << 6)
#define LOG_ALL         (LOG_IRQS | LOG_COMMANDS | LOG_READS | LOG_WRITES | LOG_UNKNOWNS | LOG_INPUTS)

#define VERBOSE         (0)
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

void cdislave_hle_device::slave_w_mouse(offs_t offset, uint16_t data)
{
	if (m_in_index == 1)
	{
		switch (m_in_buf[0])
		{
			case 0x83: // Enable pointer input
				LOGMASKED(LOG_COMMANDS, "slave_w: Channel %d: Enable Pointer Input (0x83)\n", offset);
				m_pointer_input_enabled = true;
				LOGMASKED(LOG_INPUTS, "CDI_INPUT_TRACE pointer-enable channel=%u\n", unsigned(offset));
				prepare_pointer_readback();
				memset(m_in_buf, 0, sizeof(m_in_buf));
				m_in_index = 0;
				m_in_count = 0;
				return;

			case 0x84: // Disable pointer input
				LOGMASKED(LOG_COMMANDS, "slave_w: Channel %d: Disable Pointer Input (0x84)\n", offset);
				m_pointer_input_enabled = false;
				LOGMASKED(LOG_INPUTS, "CDI_INPUT_TRACE pointer-disable channel=%u\n", unsigned(offset));
				memset(m_in_buf, 0, sizeof(m_in_buf));
				m_in_index = 0;
				m_in_count = 0;
				return;
		}
	}

	const bool set_mouse = m_in_buf[0] >= 0xc0;

	if (set_mouse)
	{
		if (m_in_index == 1)
		{
			LOGMASKED(LOG_COMMANDS, "slave_w: Channel %d: Update Mouse Position (0x%02x)\n", offset, data & 0x00ff);
			m_in_count = 3;
		}
		else if (m_in_index == m_in_count)
		{
			set_mouse_position();
			memset(m_in_buf, 0, sizeof(m_in_buf));
			m_in_index = 0;
			m_in_count = 0;
		}
	}
	else
	{
		LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Channel %d: Unknown register: %02x\n", offset, data & 0x00ff);

		if (m_in_index == 1)
		{
			memset(m_in_buf, 0, sizeof(m_in_buf));
			m_in_index = 0;
			m_in_count = 0;
		}
	}
}

void cdislave_hle_device::slave_w(offs_t offset, uint16_t data)
{
	LOGMASKED(LOG_WRITES, "slave_w: Channel %d: %d = %02x\n", offset, m_in_index, data & 0x00ff);

	constexpr std::size_t channel_count = sizeof(m_channel) / sizeof(m_channel[0]);
	if (!cdi_slave_transport::channel_index_valid(std::size_t(offset), channel_count))
	{
		LOGMASKED(LOG_WRITES | LOG_UNKNOWNS,
			"slave_w: invalid channel %u value=%02x\n",
			unsigned(offset), data & 0x00ff);
		return;
	}

	if (!cdi_slave_transport::input_write_fits(m_in_index, sizeof(m_in_buf)))
	{
		LOGMASKED(LOG_UNKNOWNS,
				"slave_w: Channel %d: discarding overlength input command index=%u count=%u\n",
				offset, m_in_index, m_in_count);
		memset(m_in_buf, 0, sizeof(m_in_buf));
		m_in_index = 0;
		m_in_count = 0;
		return;
	}

	if (offset == 1 && m_in_index == 0)
	{
		LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Channel %d: Unknown register: %02x\n", offset, data & 0x00ff);
		memset(m_in_buf, 0, sizeof(m_in_buf));
		m_in_index = 0;
		m_in_count = 0;
		return;
	}

	m_in_buf[m_in_index] = data & 0x00ff;
	m_in_index++;
	switch (offset)
	{
		case 0:
			slave_w_mouse(offset, data);
			break;
		case 1:
			if (m_in_index > 1)
			{
				if (m_in_index == m_in_count)
				{
					switch (m_in_buf[0])
					{
						case 0xf0: // Set Front Panel LCD
							memcpy(m_lcd_state, m_in_buf + 1, sizeof(m_lcd_state));
							break;
						default:
							break;
					}
					memset(m_in_buf, 0, sizeof(m_in_buf));
					m_in_index = 0;
					m_in_count = 0;
				}
			}
			break;
		case 2:
			if (m_in_index > 1)
			{
				if (m_in_index == m_in_count)
				{
					switch (m_in_buf[0])
					{
					case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: case 0xc6: case 0xc7:
						case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf:
							m_atten_w((((u32)m_in_buf[1]) << 24) | (((u32)m_in_buf[2]) << 16) | (((u32)m_in_buf[3]) << 8) | (((u32)m_in_buf[4])));
							m_in_index = 0;
							m_in_count = 0;
							break;
						case 0xf0: // Set Front Panel LCD
							memcpy(m_lcd_state, m_in_buf + 1, sizeof(m_lcd_state));
							memset(m_in_buf, 0, sizeof(m_in_buf));
							m_in_index = 0;
							m_in_count = 0;
							break;
						default:
							memset(m_in_buf, 0, sizeof(m_in_buf));
							m_in_index = 0;
							m_in_count = 0;
							break;
					}
				}
			}
			else
			{
				switch (data & 0x00ff)
				{
					case 0x8a: // Reset Main CPU
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Reset Main CPU (0x8a)\n", offset);
						m_reset_callback(ASSERT_LINE);
						m_reset_callback(CLEAR_LINE);
						m_in_index = 0;
						m_in_count = 0;
						break;
					case 0x80: // Enable Keyboard Events
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Enable Keyboard Events (0x80)\n", offset);
						m_keyboard_events_enabled = true;
						m_in_index = 0;
						m_in_count = 0;
						break;
					case 0x82: // Mute Audio
					{
						LOGMASKED(LOG_COMMANDS, "slave_w: Channel %d: Mute Audio (0x82)\n", offset);
						m_dmadac[0]->set_volume(0);
						m_dmadac[1]->set_volume(0);
						m_in_index = 0;
						m_in_count = 0;
						//cdic->audio_sample_timer->adjust(attotime::never);
						break;
					}
					case 0x83: // Unmute Audio
					{
						LOGMASKED(LOG_COMMANDS, "slave_w: Channel %d: Unmute Audio (0x83)\n", offset);
						m_dmadac[0]->set_volume(0x100);
						m_dmadac[1]->set_volume(0x100);
						m_in_index = 0;
						m_in_count = 0;
						break;
					}
					case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: case 0xc6: case 0xc7:
					case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf:
						LOGMASKED(LOG_COMMANDS, "slave_w: Channel %d: Set Attenuation Audio\n", offset);
						m_in_count = 5;
						break;
					case 0xf0: // Set Front Panel LCD
						LOGMASKED(LOG_COMMANDS, "slave_w: Channel %d: Set Front Panel LCD (0xf0)\n", offset);
						m_in_count = 17;
						break;
					default:
						LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Channel %d: Unknown register: %02x\n", offset, data & 0x00ff);
						memset(m_in_buf, 0, sizeof(m_in_buf));
						m_in_index = 0;
						m_in_count = 0;
						break;
				}
			}
			break;
		case 3:
			if (m_in_index > 1)
			{
				if (m_in_index == m_in_count)
				{
					switch (m_in_buf[0])
					{
						case 0xb0: // Request Disc Status
							prepare_readback(attotime::from_hz(4), 3, 4, 0xb0, 0x00, 0x02, 0x15, 0xb0);
							break;
						//case 0xb1: // Request Disc Base
							//prepare_readback(attotime::from_hz(10000), 3, 4, 0xb1, 0x00, 0x00, 0x00, 0xb1);
							//break;
						default:
							break;
					}
					memset(m_in_buf, 0, sizeof(m_in_buf));
					m_in_index = 0;
					m_in_count = 0;
				}
			}
			else
			{
				switch (data & 0x00ff)
				{
					case 0x80: // TODO: Set some memory.
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Set UNKWN memory (0x80). Unimplemented\n", offset);
						m_in_count = 4;
						break;
					case 0x81: // TODO: Unset some memory.
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Unset UNKWN memory (0x81). Unimplemented\n", offset);
						m_in_count = 4;
						break;
					case 0xb0: // Request Disc Status
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Request Disc Status (0xb0)\n", offset);
						m_in_count = 4;
						break;
					case 0xb1: // Request Disc Base
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Request Disc Base (0xb1)\n", offset);
						m_in_count = 4;
						break;
					case 0xf0: // Request SLAVE Revision
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Request SLAVE Revision (0xf0)\n", offset);
						prepare_readback(attotime::from_hz(10000), 2, 2, 0xf0, 0x32, 0x31, 0, 0xf0);
						m_in_index = 0;
						break;
					case 0xf3: // Request Pointer Type
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Request Pointer Type (0xf3)\n", offset);
						m_in_index = 0;
						prepare_readback(attotime::from_hz(10000), 2, 2, 0xf3, 1, 0, 0, 0xf3);
						break;
					case 0xf4: // Request Test Plug Status
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Request Test Plug Status (0xf4)\n", offset);
						m_in_index = 0;
						prepare_readback(attotime::from_hz(10000), 2, 2, 0xf4, m_testplug_cb() ? 1 : 0, 0, 0, 0xf4);
						break;
					case 0xf6: // Request NTSC/PAL Status
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Request NTSC/PAL Status (0xf6)\n", offset);
						// Real SLAVE responses are F6 02 for PAL and F6 01 for NTSC.
						prepare_readback(attotime::never, 2, 2, 0xf6, m_ntsc ? 1 : 2, 0, 0, 0xf6);
						m_in_index = 0;
						break;
					case 0xf7: // Arm Developer Mode
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Arm Developer Mode (0xf7)\n", offset);
						m_debug_mode = 1;
						m_in_index = 0;
						break;
					case 0xfa: // Enable X-Bus Interrupts
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: X-Bus Interrupt Enable (0xfa)\n", offset);
						m_xbus_interrupt_enable = 1;
						m_in_index = 0;
						break;
					case 0xfe: // Disarm Developer Mode
						LOGMASKED(LOG_COMMANDS | LOG_WRITES, "slave_w: Channel %d: Disarm Developer Mode (0xfe)\n", offset);
						m_debug_mode = 0;
						m_in_index = 0;
						break;
					default:
						LOGMASKED(LOG_COMMANDS | LOG_UNKNOWNS, "slave_w: Channel %d: Unknown register: %02x\n", offset, data & 0x00ff);
						memset(m_in_buf, 0, sizeof(m_in_buf));
						m_in_index = 0;
						m_in_count = 0;
						break;
				}
			}
			break;
	}
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

	memset(m_in_buf, 0, sizeof(m_in_buf));
	m_in_index = 0;
	m_in_count = 0;
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
