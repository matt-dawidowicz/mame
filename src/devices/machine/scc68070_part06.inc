void scc68070_device::i2c_next_state()
{
	switch (m_i2c.clock_change_state)
	{
	case I2C_SCL_SET_0:
		if (m_i2c.state == I2C_SEND_STOP)
		{
			if (!m_i2c.sda_out_state)
			{
				m_i2c.sda_out_state = true;
				m_i2c_sdaw_callback(true);
				set_i2c_timer();
			}
			else
			{
				m_i2c.state = I2C_IDLE;
				m_i2c.status_register &= ~(ISR_PIN | ISR_BB);
				m_i2c_int = true;
				update_ipl();
				m_i2c.clock_change_state = I2C_SCL_IDLE;
			}
		}
		else if (m_i2c.state == I2C_SEND_RESTART)
		{
			m_i2c.sda_out_state = false;
			m_i2c_sdaw_callback(false);
			set_i2c_timer();
			m_i2c.clock_change_state = I2C_SCL_SET_0;
			m_i2c.state = I2C_TX_IN_PROGRESS;
		}
		else
		{
			m_i2c.scl_out_state = false;
			m_i2c_scl_callback(false);
			if (m_i2c.clocks)
				m_i2c.clocks--;
			if (m_i2c.clocks == 0)
				m_i2c.clock_change_state = I2C_SCL_IDLE;
			else
			{
				set_i2c_timer();
				m_i2c.clock_change_state = I2C_SCL_SET_1;
			}
			i2c_process_falling_scl();
		}
		break;
	case I2C_SCL_SET_1:
		m_i2c.clock_change_state = I2C_SCL_WAIT_1;
		m_i2c.scl_out_state = true;
		m_i2c_scl_callback(true);
		break;
	case I2C_SCL_WAIT_1:
		set_i2c_timer();
		m_i2c.clock_change_state = I2C_SCL_SET_0;
		break;
	}
}

void scc68070_device::set_i2c_timer()
{
	static constexpr int divider[] = { 1, 78, 90, 102, 126, 150, 174, 198,
		246, 294, 342, 390, 486, 582, 678, 774,
		996, 1158, 1350, 1542, 1926, 2310, 2694, 3078,
		3846, 4614, 5382, 6150, 7686, 9222, 10758, 12294 };
	m_i2c.timer->adjust(cycles_to_attotime(divider[m_i2c.clock_control_register]));
}

uint8_t scc68070_device::umr_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_MORE_UART, "%s: UART Mode Register Read: %02x\n", machine().describe_context(), m_uart.mode_register);
	return m_uart.mode_register & ~0x20U;
}

void scc68070_device::umr_w(uint8_t data)
{
	LOGMASKED(LOG_MORE_UART, "%s: UART Mode Register Write: %02x\n", machine().describe_context(), data);
	m_uart.mode_register = data & ~0x20U;
	update_uart_timing();
}

uint8_t scc68070_device::usr_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_MORE_UART, "%s: UART Status Register Read: %02x\n", machine().describe_context(), m_uart.status_register);
	return scc68070::uart_status_read_value(m_uart.status_register);
}

uint8_t scc68070_device::ucsr_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_UART, "%s: UART Clock Select Read: %02x\n", machine().describe_context(), m_uart.clock_select);
	return m_uart.clock_select & ~0x08U;
}

void scc68070_device::ucsr_w(uint8_t data)
{
	LOGMASKED(LOG_UART, "%s: UART Clock Select Write: %02x\n", machine().describe_context(), data);
	m_uart.clock_select = data & ~0x08U;
	update_uart_timing();
}

void scc68070_device::update_uart_timing()
{
	const uint32_t uart_clock = scc68070::uart_baud_clock(
		clock(), m_uart_external_clock, BIT(m_uart.clock_select, 7));
	if (!uart_clock)
	{
		m_uart.rx_timer->adjust(attotime::never);
		m_uart.tx_timer->adjust(attotime::never);
		return;
	}

	const uint64_t frame_bits = scc68070::uart_frame_bits(m_uart.mode_register);
	const attotime rx_rate = attotime::from_ticks(
		uint64_t(scc68070::uart_baud_divisor((m_uart.clock_select >> 4) & 7)) * frame_bits,
		uart_clock);
	const attotime tx_rate = attotime::from_ticks(
		uint64_t(scc68070::uart_baud_divisor(m_uart.clock_select & 7)) * frame_bits,
		uart_clock);
	m_uart.rx_timer->adjust(rx_rate, 0, rx_rate);
	m_uart.tx_timer->adjust(tx_rate, 0, tx_rate);
}

attotime scc68070_device::uart_tx_bit_period() const
{
	const uint32_t uart_clock = scc68070::uart_baud_clock(
		clock(), m_uart_external_clock, BIT(m_uart.clock_select, 7));
	return uart_clock
		? attotime::from_ticks(scc68070::uart_baud_divisor(m_uart.clock_select & 7), uart_clock)
		: attotime::never;
}

uint8_t scc68070_device::ucr_r()
{
	return m_uart.command_register & 0x0f;
}

void scc68070_device::ucr_w(uint8_t data)
{
	LOGMASKED(LOG_MORE_UART, "%s: UART Command Register Write: %02x\n", machine().describe_context(), data);

	uint8_t controls = m_uart.command_register & 0x0f;
	const uint8_t receiver_command = data & 0x03;
	const uint8_t transmitter_command = (data >> 2) & 0x03;
	if (receiver_command == 1 || receiver_command == 2)
		controls = (controls & ~0x03U) | receiver_command;
	if (transmitter_command == 1 || transmitter_command == 2)
		controls = (controls & ~0x0cU) | (transmitter_command << 2);
	m_uart.command_register = controls;

	if (receiver_command == 2)
		m_uart.status_register &= ~USR_RXRDY;
	if (transmitter_command == 1 && m_uart.transmit_pointer < 0)
		m_uart.status_register |= USR_TXRDY | USR_TXEMT;

	const uint8_t misc_command = (data >> 4) & 0x07;
	switch (misc_command)
	{
	case 0x2: // Reset receiver and disable it.
		m_uart.receive_pointer = -1;
		m_uart.receive_holding_register = 0;
		m_uart.status_register &= ~USR_RXRDY;
		m_uart.command_register = (m_uart.command_register & ~0x03U) | 0x02;
		m_uart_rx_int = false;
		update_ipl();
		break;
	case 0x3: // Reset transmitter and disable it.
		m_uart.transmit_pointer = -1;
		m_uart.transmit_holding_register = 0;
		m_uart.status_register &= ~(USR_TXRDY | USR_TXEMT);
		m_uart.command_register = (m_uart.command_register & ~0x0cU) | 0x08;
		m_uart_tx_int = false;
		update_ipl();
		break;
	case 0x4: // Reset error status.
		m_uart.status_register &= ~(USR_RB | USR_FE | USR_PE | USR_OE);
		break;
	case 0x6: // Start break: force TxD low.
		m_uart.break_timer->adjust(attotime::never);
		m_uart.break_release_pending = false;
		m_uart.break_active = true;
		m_uart_break_callback(ASSERT_LINE);
		break;
	case 0x7: // Stop break: release within two bit times, then hold mark for one bit.
		if (m_uart.break_active)
		{
			const attotime bit = uart_tx_bit_period();
			if (bit.is_never())
			{
				m_uart.break_active = false;
				m_uart.break_release_pending = false;
				m_uart_break_callback(CLEAR_LINE);
			}
			else
				m_uart.break_timer->adjust(bit * 2, 0);
		}
		break;
	default:
		break;
	}
}

TIMER_CALLBACK_MEMBER(scc68070_device::uart_break_timer_callback)
{
	if (param == 0)
	{
		m_uart.break_active = false;
		m_uart.break_release_pending = true;
		m_uart_break_callback(CLEAR_LINE);
		const attotime bit = uart_tx_bit_period();
		if (bit.is_never())
			m_uart.break_release_pending = false;
		else
			m_uart.break_timer->adjust(bit, 1);
	}
	else
		m_uart.break_release_pending = false;
}

uint8_t scc68070_device::uth_r()
{
	return m_uart.transmit_holding_register;
}

void scc68070_device::uth_w(uint8_t data)
{
	LOGMASKED(LOG_MORE_UART, "%s: UART Transmit Holding Register Write: %02x ('%c')\n",
		machine().describe_context(), data, (data >= 0x20 && data < 0x7f) ? data : ' ');
	m_uart.transmit_holding_register = data & scc68070::uart_character_mask(m_uart.mode_register);

	// CPU-to-transmitter path is disconnected in auto-echo and remote-loopback.
	const uint8_t mode = m_uart.mode_register & UMR_OM;
	if (mode == UMR_OM_NORMAL || mode == UMR_OM_LOOPBACK)
		uart_tx(m_uart.transmit_holding_register);
}

uint8_t scc68070_device::urh_r()
{
	const uint8_t data = m_uart.receive_holding_register;
	if (!machine().side_effects_disabled())
	{
		if (m_uart_rx_int)
		{
			m_uart_rx_int = false;
			update_ipl();
		}
		if (m_uart.receive_pointer >= 0)
		{
			for (int index = 0; index < m_uart.receive_pointer; index++)
				m_uart.receive_buffer[index] = m_uart.receive_buffer[index + 1];
			m_uart.receive_pointer--;
		}
		if (m_uart.receive_pointer >= 0)
		{
			m_uart.receive_holding_register = m_uart.receive_buffer[0];
			m_uart.status_register |= USR_RXRDY;
		}
		else
			m_uart.status_register &= ~USR_RXRDY;
	}
	return data;
}

uint16_t scc68070_device::timer_r(offs_t offset, uint16_t mem_mask)
{
	switch (offset)
	{
	case 0x0/2:
		return (m_timers.timer_status_register << 8) | m_timers.timer_control_register;
	case 0x2/2:
		return m_timers.reload_register;
	case 0x4/2:
		return timer0_current();
