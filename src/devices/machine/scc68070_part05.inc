			// we should send STOP here, however, unkte06 in magicard appears to expect
			// NAK followed by STOP when in read mode.

			if (data & ISR_PIN)
			{
				m_i2c_int = false;
				update_ipl();
			}

			if (m_i2c.ack_or_nak_sent || (m_i2c.status_register & ISR_TRX))
			{
				m_i2c.state = I2C_SEND_STOP;
				m_i2c.sda_out_state = false;
				m_i2c_sdaw_callback(false);
			}
			else
			{
				m_i2c.ack_or_nak_sent = true;
				m_i2c.sda_out_state = (m_i2c.control_register&ICR_ACK) ? false : true;
				m_i2c_sdaw_callback(m_i2c.sda_out_state);
				m_i2c.state = I2C_SEND_ACK_AND_STOP;
				m_i2c.clocks = 2;
			}
			m_i2c.status_register = data | ISR_BB;
			m_i2c.clock_change_state = I2C_SCL_SET_1;
			set_i2c_timer();
		}
		else if ((data & ISR_SSR_MASK) == ISR_MST)
		{
			m_i2c.status_register = data;
			if (data & ISR_PIN)
			{
				m_i2c_int = false;
				update_ipl();
			}
		}
		else
		{
			if (data & ISR_PIN && !(m_i2c.status_register & ISR_PIN))
			{
				if (m_i2c.state == I2C_CHANGED_TO_RX)
				{
					m_i2c.state = I2C_RX_IN_PROGRESS;
					m_i2c.clock_change_state = I2C_SCL_SET_1;
					m_i2c.status_register = data;
					m_i2c_int = false;
					update_ipl();
					m_i2c.counter = 0;
					m_i2c.clocks = 8;
					set_i2c_timer();
				}
				else
				{
					m_i2c.ack_or_nak_sent = true;
					m_i2c.sda_out_state = (m_i2c.control_register&ICR_ACK) ? false : true;
					m_i2c_sdaw_callback(m_i2c.sda_out_state);
					m_i2c.status_register = data;
					m_i2c_int = false;
					update_ipl();
					m_i2c.state = I2C_SEND_ACK;
					m_i2c.clock_change_state = I2C_SCL_SET_1;
					m_i2c.clocks = 1;
					set_i2c_timer();
				}
			}
			else
			{
				m_i2c.status_register = data;
				if (data & ISR_PIN)
				{
					m_i2c_int = false;
					update_ipl();
				}
			}
		}
	}
	else
	{
		m_i2c.status_register = data;
		m_i2c_int = false;
		update_ipl();
		m_i2c.timer->adjust(attotime::never);
		m_i2c_scl_callback(1);
		m_i2c_sdaw_callback(1);
		m_i2c.scl_out_state = true;
		m_i2c.scl_in_state = true;
		m_i2c.sda_out_state = true;
		m_i2c.state = I2C_IDLE;
	}
}

uint8_t scc68070_device::icr_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_I2C, "%s: I2C Control Register Read: %02x\n", machine().describe_context(), m_i2c.control_register);
	return m_i2c.control_register;
}

void scc68070_device::icr_w(uint8_t data)
{
	LOGMASKED(LOG_I2C, "%s: I2C Control Register Write: %02x\n", machine().describe_context(), data);
	m_i2c.control_register = data;
	if (!(data & ICR_ESO))
	{
		m_i2c.timer->adjust(attotime::never);
		m_i2c_scl_callback(1);
		m_i2c_sdaw_callback(1);
		m_i2c.scl_out_state = true;
		m_i2c.scl_in_state = true;
		m_i2c.sda_out_state = true;
		m_i2c.state = I2C_IDLE;
	}
}

uint8_t scc68070_device::iccr_r()
{
	if (!machine().side_effects_disabled())
		LOGMASKED(LOG_I2C, "%s: I2C Clock Control Register Read: %02x\n", machine().describe_context(), m_i2c.clock_control_register);
	return m_i2c.clock_control_register | 0xe0;
}

void scc68070_device::iccr_w(uint8_t data)
{
	LOGMASKED(LOG_I2C, "%s: I2C Clock Control Register Write: %02x\n", machine().describe_context(), data);
	m_i2c.clock_control_register = data & 0x1f;
}

void scc68070_device::i2c_process_falling_scl()
{
	switch (m_i2c.state)
	{
	case I2C_TX_IN_PROGRESS:
		if (m_i2c.counter < 8)
		{
			m_i2c.sda_out_state = BIT(m_i2c.data_register, 7 - m_i2c.counter);
			m_i2c_sdaw_callback(m_i2c.sda_out_state);
			m_i2c.counter++;
		}
		else
		{
			m_i2c.sda_out_state = true;
			m_i2c_sdaw_callback(true);
			m_i2c.state = I2C_GET_ACK;
		}
		break;
	case I2C_GET_ACK:
		m_i2c.status_register &= ~ISR_PIN;
		m_i2c_int = true;
		update_ipl();
		m_i2c.state = I2C_IDLE;
		if (m_i2c.first_byte)
		{
			m_i2c.first_byte = false;
			if (BIT(m_i2c.data_register, 0))
			{
				m_i2c.status_register &= ~ISR_TRX;
				if (!(m_i2c.status_register & ISR_LRB))
					m_i2c.state = I2C_CHANGED_TO_RX;
			}
		}
		break;
	case I2C_RX_IN_PROGRESS:
		if (m_i2c.counter >= 8)
		{
			m_i2c.status_register &= ~ISR_PIN;
			m_i2c_int = true;
			update_ipl();
			m_i2c.state = I2C_RX_COMPLETE;
		}
		break;
	case I2C_SEND_ACK_AND_RX:
		m_i2c.sda_out_state = true;
		m_i2c_sdaw_callback(true);
		m_i2c.state = I2C_RX_IN_PROGRESS;
		m_i2c.counter = 0;
		break;
	case I2C_SEND_ACK_AND_STOP:
		m_i2c.sda_out_state = false;
		m_i2c_sdaw_callback(false);
		m_i2c.state = I2C_SEND_STOP;
		break;
	case I2C_SEND_ACK:
		m_i2c.state = I2C_IDLE;
		m_i2c.status_register &= ~ISR_PIN;
		m_i2c_int = true;
		update_ipl();
		break;
	}
}

void scc68070_device::i2c_process_rising_scl()
{
	switch (m_i2c.state)
	{
	case I2C_GET_ACK:
		if (m_i2c_sdar_callback())
			m_i2c.status_register |= ISR_LRB;
		else
			m_i2c.status_register &= ~ISR_LRB;
		break;
	case I2C_SEND_STOP:
	case I2C_SEND_RESTART:
		m_i2c.timer->adjust(attotime::from_nsec(5000));
		break;
	case I2C_RX_IN_PROGRESS:
		if (m_i2c.counter < 8)
		{
			m_i2c.data_register <<= 1;
			m_i2c.data_register |= m_i2c_sdar_callback();
			m_i2c.counter++;
		}
		break;
	}
}

void scc68070_device::write_scl(int state)
{
	if (m_i2c.status_register & ISR_MST)
	{
		if (m_i2c.scl_in_state != state && state)
		{
			i2c_process_rising_scl();
			i2c_next_state();
		}
	}
	m_i2c.scl_in_state = state;
}

TIMER_CALLBACK_MEMBER(scc68070_device::i2c_callback)
{
	i2c_next_state();
}
