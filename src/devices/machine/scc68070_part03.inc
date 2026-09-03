void scc68070_device::in4_w(int state)
{
	m_in4_line = state;
	update_ipl();
}

void scc68070_device::in5_w(int state)
{
	m_in5_line = state;
	update_ipl();
}

void scc68070_device::nmi_w(int state)
{
	m_nmi_line = state;
	update_ipl();
}

void scc68070_device::int1_w(int state)
{
	if (m_int1_line != state)
	{
		if (state == ASSERT_LINE && !BIT(m_lir, 7))
		{
			m_lir |= 0x80;
			update_ipl();
		}
		m_int1_line = state;
	}
}

void scc68070_device::int2_w(int state)
{
	if (m_int2_line != state)
	{
		if (state == ASSERT_LINE && !BIT(m_lir, 3))
		{
			m_lir |= 0x08;
			update_ipl();
		}
		m_int2_line = state;
	}
}

uint8_t scc68070_device::iack_r(offs_t offset)
{
	switch (offset)
	{
	case 2:
		if (m_in2_line == ASSERT_LINE)
			return m_iack2_callback();
		break;
	case 4:
		if (m_in4_line == ASSERT_LINE)
			return m_iack4_callback();
		break;
	case 5:
		if (m_in5_line == ASSERT_LINE)
			return m_iack5_callback();
		break;
	case 7:
		if (m_nmi_line == ASSERT_LINE)
			return m_iack7_callback();
		break;
	}

	if (!machine().side_effects_disabled())
	{
		const std::array<uint8_t, 8> levels = {
			BIT(m_lir, 7) ? uint8_t((m_lir >> 4) & 7) : uint8_t(0),
			BIT(m_lir, 3) ? uint8_t(m_lir & 7) : uint8_t(0),
			m_timer_int ? uint8_t(m_picr1 & 7) : uint8_t(0),
			m_uart_rx_int ? uint8_t((m_picr2 >> 4) & 7) : uint8_t(0),
			m_uart_tx_int ? uint8_t(m_picr2 & 7) : uint8_t(0),
			m_i2c_int ? uint8_t((m_picr1 >> 4) & 7) : uint8_t(0),
			(m_dma.channel[0].channel_status & CSR_COC) && (m_dma.channel[0].channel_control & CCR_INE) ? uint8_t(m_dma.channel[0].channel_control & CCR_IPL) : uint8_t(0),
			(m_dma.channel[1].channel_status & CSR_COC) && (m_dma.channel[1].channel_control & CCR_INE) ? uint8_t(m_dma.channel[1].channel_control & CCR_IPL) : uint8_t(0)
		};

		switch (scc68070::first_interrupt_source(levels, offset))
		{
		case scc68070::interrupt_source::int1: m_lir &= 0x7f; break;
		case scc68070::interrupt_source::int2: m_lir &= 0xf7; break;
		case scc68070::interrupt_source::timer: m_timer_int = false; break;
		case scc68070::interrupt_source::uart_rx: m_uart_rx_int = false; break;
		case scc68070::interrupt_source::uart_tx: m_uart_tx_int = false; break;
		case scc68070::interrupt_source::i2c: m_i2c_int = false; break;
		case scc68070::interrupt_source::dma1:
		case scc68070::interrupt_source::dma2:
		case scc68070::interrupt_source::none:
			break;
		}
		update_ipl();
	}

	return 0x38 + offset;
}

void scc68070_device::set_timer_callback(int channel)
{
	if (channel != 0)
		fatalerror("Unsupported timer channel to set_timer_callback!\n");

	const uint32_t compare = 0x10000 - m_timers.timer0;
	m_timers.timer0_timer->adjust(cycles_to_attotime(96 * compare));
}

uint16_t scc68070_device::timer0_current() const
{
	return uint16_t(0x10000 - (attotime_to_cycles(m_timers.timer0_timer->remaining()) / 96));
}

void scc68070_device::timer_set_status(uint8_t status_bit)
{
	m_timers.timer_status_register |= status_bit;
	m_timer_int = true;
	update_ipl();
}

void scc68070_device::timer_set_output(unsigned channel, bool state)
{
	bool *const output = channel == 1 ? &m_timers.timer1_output : &m_timers.timer2_output;
	if (*output == state)
		return;
	*output = state;
	if (channel == 1)
		m_timer1_out_callback(state ? ASSERT_LINE : CLEAR_LINE);
	else
		m_timer2_out_callback(state ? ASSERT_LINE : CLEAR_LINE);
}

void scc68070_device::timer_schedule_match(unsigned channel)
{
	emu_timer *const timer = channel == 1 ? m_timers.timer1_match_timer : m_timers.timer2_match_timer;
	if (scc68070::timer_channel_mode(m_timers.timer_control_register, channel) != scc68070::timer_mode::match)
	{
		timer->adjust(attotime::never);
		return;
	}

	const uint16_t current = timer0_current();
	const uint16_t target = channel == 1 ? m_timers.timer1 : m_timers.timer2;
	uint32_t ticks;

	if (target >= current)
		ticks = uint32_t(target) - current;
	else if (target >= m_timers.reload_register)
		ticks = (0x10000U - current) + (uint32_t(target) - m_timers.reload_register);
	else
	{
		// After overflow Timer 0 reloads from TRR, so a target below TRR is
		// unreachable until software reprograms Timer 0 or TRR.
		timer->adjust(attotime::never);
		return;
	}

	timer->adjust(ticks ? cycles_to_attotime(96 * ticks) : attotime::zero);
}

TIMER_CALLBACK_MEMBER(scc68070_device::timer0_callback)
{
	m_timers.timer0 = m_timers.reload_register;
	set_timer_callback(0);
	timer_set_status(TSR_OV0);

	for (unsigned channel = 1; channel <= 2; ++channel)
	{
		if (scc68070::timer_channel_mode(m_timers.timer_control_register, channel) == scc68070::timer_mode::match)
		{
			timer_set_output(channel, true);
			timer_schedule_match(channel);
		}
	}
}

TIMER_CALLBACK_MEMBER(scc68070_device::timer1_match_callback)
{
	timer_set_output(1, false);
	timer_set_status(TSR_MA1);
}

TIMER_CALLBACK_MEMBER(scc68070_device::timer2_match_callback)
{
	timer_set_output(2, false);
	timer_set_status(TSR_MA2);
}

void scc68070_device::timer_input_w(unsigned channel, int state)
{
	bool &previous = channel == 1 ? m_timers.timer1_input : m_timers.timer2_input;
	const bool current = state != 0;
	const auto mode = scc68070::timer_channel_mode(m_timers.timer_control_register, channel);
	const auto edge = scc68070::timer_channel_edge(m_timers.timer_control_register, channel);

	if (scc68070::timer_edge_matches(edge, previous, current))
	{
		uint16_t &timer = channel == 1 ? m_timers.timer1 : m_timers.timer2;
		if (mode == scc68070::timer_mode::capture)
		{
			timer = timer0_current();
			timer_set_status(channel == 1 ? TSR_CAP1 : TSR_CAP2);
		}
		else if (mode == scc68070::timer_mode::count)
		{
			const auto result = scc68070::timer_count_external_event(timer);
			timer = result.value;
			if (result.overflow)
				timer_set_status(channel == 1 ? TSR_OV1 : TSR_OV2);
		}
	}
	previous = current;
}

void scc68070_device::timer1_w(int state)
{
	timer_input_w(1, state);
}

void scc68070_device::timer2_w(int state)
{
	timer_input_w(2, state);
}

void scc68070_device::uart_ctsn(int state)
{
	m_uart.transmit_ctsn = state != 0;
}

void scc68070_device::uart_queue_receive(uint8_t data, bool framing_error, bool parity_error, bool break_received)
{
	const uint8_t mode = m_uart.mode_register & UMR_OM;
	// Local loopback feeds the receive datapath internally even when the
	// normal receiver-enable command is clear.
	if ((m_uart.command_register & 3) != 1 && mode != UMR_OM_LOOPBACK)
		return;

	if (m_uart.receive_pointer >= int16_t(std::size(m_uart.receive_buffer) - 1))
	{
		m_uart.status_register |= USR_OE;
		return;
	}

	m_uart.receive_pointer++;
	m_uart.receive_buffer[m_uart.receive_pointer] = data & scc68070::uart_character_mask(m_uart.mode_register);
	if (framing_error)
		m_uart.status_register |= USR_FE;
	if (parity_error)
		m_uart.status_register |= USR_PE;
	if (break_received)
		m_uart.status_register |= USR_RB;
}

void scc68070_device::uart_rx(uint8_t data)
{
	uart_rx(data, false, false, false);
}

void scc68070_device::uart_rx(uint8_t data, bool framing_error, bool parity_error, bool break_received)
{
	const uint8_t mode = m_uart.mode_register & UMR_OM;
	const bool receiver_enabled = (m_uart.command_register & 3) == 1;
	const uint8_t character = data & scc68070::uart_character_mask(m_uart.mode_register);

	switch (mode)
	{
	case UMR_OM_NORMAL:
		uart_queue_receive(character, framing_error, parity_error, break_received);
		break;
	case UMR_OM_ECHO:
		// Auto-echo uses the receive path/clock.  CPU-to-transmitter is
		// disconnected and normal transmitter enable/CTS do not gate echo.
		if (receiver_enabled)
		{
			uart_queue_receive(character, framing_error, parity_error, break_received);
			m_uart_tx_callback(character);
		}
		break;
	case UMR_OM_LOOPBACK:
		// Local loopback ignores the external receiver input.
		break;
	case UMR_OM_RLOOP:
		// Remote loopback also uses the receive path/clock, but received data
		// is not made visible to the local CPU.  Receive error status remains
		// meaningful even though RxRDY stays inactive.
		if (receiver_enabled)
		{
			if (framing_error)
				m_uart.status_register |= USR_FE;
			if (parity_error)
				m_uart.status_register |= USR_PE;
			if (break_received)
				m_uart.status_register |= USR_RB;
			m_uart_tx_callback(character);
		}
		break;
	}
}

void scc68070_device::uart_tx(uint8_t data)
{
	if (m_uart.transmit_pointer >= int16_t(std::size(m_uart.transmit_buffer) - 1))
		return;

	m_uart.transmit_pointer++;
	m_uart.transmit_buffer[m_uart.transmit_pointer] = data & scc68070::uart_character_mask(m_uart.mode_register);
	m_uart.status_register &= ~(USR_TXEMT | USR_TXRDY);
}

TIMER_CALLBACK_MEMBER(scc68070_device::rx_callback)
{
	const uint8_t mode = m_uart.mode_register & UMR_OM;
	if ((((m_uart.command_register & 3) != 1) && mode != UMR_OM_LOOPBACK) || m_uart.receive_pointer < 0)
	{
		m_uart.status_register &= ~USR_RXRDY;
		return;
	}

	m_uart.receive_holding_register = m_uart.receive_buffer[0];
	m_uart.status_register |= USR_RXRDY;
	m_uart_rx_int = true;
	update_ipl();
}

TIMER_CALLBACK_MEMBER(scc68070_device::tx_callback)
{
	const uint8_t mode = m_uart.mode_register & UMR_OM;

	// In automatic echo and remote loopback, the normal transmitter datapath
	// is bypassed and its ready/empty status and interrupt are inactive.
	if (mode == UMR_OM_ECHO || mode == UMR_OM_RLOOP)
	{
		m_uart.status_register &= ~(USR_TXEMT | USR_TXRDY);
		if (m_uart_tx_int)
		{
			m_uart_tx_int = false;
			update_ipl();
		}
		return;
	}

	if (((m_uart.command_register >> 2) & 3) != 1 || m_uart.break_active || m_uart.break_release_pending)
		return;
	if (m_uart.transmit_pointer < 0)
	{
		m_uart.status_register |= USR_TXEMT | USR_TXRDY;
		return;
	}
	// External CTS gates only normal transmission.  Local loopback is an
	// internal diagnostic path and ignores the external handshake input.
	if (mode == UMR_OM_NORMAL && m_uart.transmit_ctsn && BIT(m_uart.mode_register, 4))
		return;

	m_uart.transmit_holding_register = m_uart.transmit_buffer[0];
	if (mode == UMR_OM_LOOPBACK)
		uart_queue_receive(m_uart.transmit_holding_register, false, false, false);
	else
		m_uart_tx_callback(m_uart.transmit_holding_register);

	for (int index = 0; index < m_uart.transmit_pointer; index++)
		m_uart.transmit_buffer[index] = m_uart.transmit_buffer[index + 1];
	m_uart.transmit_pointer--;
	m_uart.status_register |= USR_TXRDY;
	if (m_uart.transmit_pointer < 0)
		m_uart.status_register |= USR_TXEMT;
	m_uart_tx_int = true;
	update_ipl();
}

uint8_t scc68070_device::lir_r()
{
	// LIR priority level: 80001001
