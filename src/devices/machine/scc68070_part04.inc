	return m_lir & 0x77;
}

void scc68070_device::lir_w(uint8_t data)
{
	LOGMASKED(LOG_IRQS, "%s: LIR Write: %02x\n", machine().describe_context(), data);

	switch (data & 0x88)
	{
	case 0x08:
		if (m_lir & 0x08)
		{
			m_lir &= 0xf7;
			update_ipl();
		}
		break;
	case 0x80:
		if (data & 0x80)
		{
			m_lir &= 0x7f;
			update_ipl();
		}
		break;
	case 0x88:
		if (data & 0x88)
		{
			m_lir &= 0x77;
			update_ipl();
		}
		break;
	}
	m_lir = (m_lir & 0x88) | (data & 0x77);
}

uint8_t scc68070_device::picr1_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_IRQS, "%s: Peripheral Interrupt Control Register 1 Read: %02x\n", machine().describe_context(), m_picr1);
	return m_picr1 & 0x77;
}

void scc68070_device::picr1_w(uint8_t data)
{
	LOGMASKED(LOG_IRQS, "%s: Peripheral Interrupt Control Register 1 Write: %02x\n", machine().describe_context(), data);
	m_picr1 = data & 0x77;
	switch (data & 0x88)
	{
	case 0x08:
		if (m_timer_int)
		{
			m_timer_int = false;
			update_ipl();
		}
		break;
	case 0x80:
		if (m_i2c_int)
		{
			m_i2c_int = false;
			update_ipl();
		}
		break;
	case 0x88:
		if (m_timer_int || m_i2c_int)
		{
			m_timer_int = false;
			m_i2c_int = false;
			update_ipl();
		}
		break;
	}
}

uint8_t scc68070_device::picr2_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_IRQS, "%s: Peripheral Interrupt Control Register 2 Read: %02x\n", machine().describe_context(), m_picr2);
	return m_picr2 & 0x77;
}

void scc68070_device::picr2_w(uint8_t data)
{
	LOGMASKED(LOG_IRQS, "%s: Peripheral Interrupt Control Register 2 Write: %02x\n", machine().describe_context(), data);
	m_picr2 = data & 0x77;
	switch (data & 0x88)
	{
	case 0x08:
		if (m_uart_tx_int)
		{
			m_uart_tx_int = false;
			update_ipl();
		}
		break;
	case 0x80:
		if (m_uart_rx_int)
		{
			m_uart_rx_int = false;
			update_ipl();
		}
		break;
	case 0x88:
		if (m_uart_tx_int || m_uart_rx_int)
		{
			m_uart_tx_int = false;
			m_uart_rx_int = false;
			update_ipl();
		}
		break;
	}
}

uint8_t scc68070_device::idr_r()
{
	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_I2C, "%s: I2C Data Register Read: %02x\n", machine().describe_context(), m_i2c.data_register);
		m_i2c.counter = 0;
		m_i2c.status_register = scc68070::i2c_status_after_data_access(m_i2c.status_register);
		m_i2c_int = false;
		update_ipl();
		if (m_i2c.state == I2C_RX_COMPLETE)
		{
			m_i2c.sda_out_state = (m_i2c.control_register & ICR_ACK) ? false : true;
			m_i2c_sdaw_callback(m_i2c.sda_out_state);
			if (m_i2c.control_register & ICR_ACK)
			{
				m_i2c.state = I2C_SEND_ACK_AND_RX;
				m_i2c.clocks = 9;
			}
			else
			{
				m_i2c.state = I2C_SEND_ACK;
				m_i2c.clocks = 1;
			}
			m_i2c.ack_or_nak_sent = true;
			m_i2c.clock_change_state = I2C_SCL_SET_1;
			set_i2c_timer();
		}
	}
	return m_i2c.data_register;
}

void scc68070_device::idr_w(uint8_t data)
{
	LOGMASKED(LOG_I2C, "%s: I2C Data Register Write: %02x\n", machine().describe_context(), data);
	m_i2c.data_register = data;
	m_i2c.status_register = scc68070::i2c_status_after_data_access(m_i2c.status_register);
	m_i2c_int = false;
	update_ipl();
	if (m_i2c.status_register & ISR_MST && m_i2c.status_register & ISR_TRX && m_i2c.status_register & ISR_BB)
	{
		m_i2c.counter = 0;
		m_i2c.state = I2C_TX_IN_PROGRESS;
		m_i2c.clocks = 9;
		i2c_process_falling_scl();
		m_i2c.clock_change_state = I2C_SCL_SET_1;
		set_i2c_timer();
	}
}

uint8_t scc68070_device::iar_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_I2C, "%s: I2C Address Register Read: %02x\n", machine().describe_context(), m_i2c.address_register);
	return m_i2c.address_register;
}

void scc68070_device::iar_w(uint8_t data)
{
	LOGMASKED(LOG_I2C, "%s: I2C Address Register Write: %02x\n", machine().describe_context(), data);
	m_i2c.address_register = data;
}

uint8_t scc68070_device::isr_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_I2C, "%s: I2C Status Register Read: %02x\n", machine().describe_context(), m_i2c.status_register);
	return m_i2c.status_register;
}

void scc68070_device::isr_w(uint8_t data)
{
	LOGMASKED(LOG_I2C, "%s: I2C Status Register Write: %02x\n", machine().describe_context(), data);
	if (data & ISR_MST)
	{
		if ((data & ISR_SSR_MASK) == ISR_START)
		{
			if ((m_i2c.status_register & ISR_SSR_MASK) == ISR_STOP || (m_i2c.status_register & ISR_SSR_MASK) == 0)
			{
				if (m_i2c_sdar_callback() && m_i2c.state == I2C_IDLE)
				{
					m_i2c.status_register = data;
					if (data & ISR_PIN)
					{
						m_i2c_int = false;
						update_ipl();
					}
					m_i2c.sda_out_state = false;
					m_i2c_sdaw_callback(false);
					m_i2c.clock_change_state = I2C_SCL_SET_0;
					m_i2c.clocks = 10;
					m_i2c.state = I2C_TX_IN_PROGRESS;
					m_i2c.first_byte = true;
					m_i2c.ack_or_nak_sent = false;
					set_i2c_timer();
					m_i2c.counter = 0;
				}
				else
				{
					m_i2c.status_register |= ISR_AL;
					m_i2c.status_register &= ~ISR_PIN;
					m_i2c_int = true;
					update_ipl();
				}
			}
			else if ((m_i2c.status_register & ISR_SSR_MASK) == ISR_MST)
			{
				m_i2c.status_register = data;
				if (data & ISR_PIN)
				{
					m_i2c_int = false;
					update_ipl();
				}
				m_i2c.sda_out_state = true;
				m_i2c_sdaw_callback(true);
				m_i2c.clock_change_state = I2C_SCL_SET_1;
				m_i2c.clocks = 10;
				m_i2c.state = I2C_SEND_RESTART;
				m_i2c.first_byte = true;
				m_i2c.ack_or_nak_sent = false;
				set_i2c_timer();
				m_i2c.counter = 0;
			}
		}
		else if ((data & ISR_SSR_MASK) == ISR_STOP && m_i2c.status_register & ISR_BB)
		{