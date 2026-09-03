
	case 0x6/2:
		return m_timers.timer1;
	case 0x8/2:
		return m_timers.timer2;
	default:
		LOGMASKED(LOG_TIMERS | LOG_UNKNOWN, "%s: Timer Unknown Register Read: %04x & %04x\n", machine().describe_context(), offset * 2, mem_mask);
		break;
	}
	return 0;
}

void scc68070_device::timer_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	switch (offset)
	{
	case 0x0/2:
		if (ACCESSING_BITS_0_7)
		{
			m_timers.timer_control_register = data & 0x00ff;
			for (unsigned channel = 1; channel <= 2; ++channel)
			{
				if (scc68070::timer_channel_mode(m_timers.timer_control_register, channel) == scc68070::timer_mode::match)
				{
					timer_set_output(channel, false);
					timer_schedule_match(channel);
				}
				else
				{
					(channel == 1 ? m_timers.timer1_match_timer : m_timers.timer2_match_timer)->adjust(attotime::never);
					timer_set_output(channel, false);
				}
			}
		}
		if (ACCESSING_BITS_8_15)
		{
			m_timers.timer_status_register &= ~(data >> 8);
			m_timer_int = m_timers.timer_status_register != 0;
			update_ipl();
		}
		break;
	case 0x2/2:
		// TRR writes update the next Timer-0 reload value; they do not re-phase
		// the currently running Timer-0 interval.
		COMBINE_DATA(&m_timers.reload_register);
		break;
	case 0x4/2:
		COMBINE_DATA(&m_timers.timer0);
		set_timer_callback(0);
		timer_schedule_match(1);
		timer_schedule_match(2);
		break;
	case 0x6/2:
		COMBINE_DATA(&m_timers.timer1);
		timer_schedule_match(1);
		break;
	case 0x8/2:
		COMBINE_DATA(&m_timers.timer2);
		timer_schedule_match(2);
		break;
	default:
		LOGMASKED(LOG_TIMERS | LOG_UNKNOWN, "%s: Timer Unknown Register Write: %04x = %04x & %04x\n", machine().describe_context(), offset * 2, data, mem_mask);
		break;
	}
}

uint16_t scc68070_device::dma_r(offs_t offset, uint16_t mem_mask)
{
	switch (offset)
	{
	case 0x00/2:
	case 0x40/2:
		return (m_dma.channel[offset / 32].channel_status << 8) | m_dma.channel[offset / 32].channel_error;
	case 0x04/2:
	case 0x44/2:
		return (m_dma.channel[offset / 32].device_control << 8) | m_dma.channel[offset / 32].operation_control;
	case 0x06/2:
	case 0x46/2:
		return (m_dma.channel[offset / 32].sequence_control << 8) | m_dma.channel[offset / 32].channel_control;
	case 0x0a/2:
	case 0x4a/2:
		return m_dma.channel[offset / 32].transfer_counter;
	case 0x0c/2:
	case 0x4c/2:
		return (m_dma.channel[offset / 32].memory_address_counter >> 16) & 0x00ff;
	case 0x0e/2:
	case 0x4e/2:
		return m_dma.channel[offset / 32].memory_address_counter;
	case 0x14/2:
		return 0;
	case 0x54/2:
		return (m_dma.channel[offset / 32].device_address_counter >> 16) & 0x00ff;
	case 0x16/2:
		return 0;
	case 0x56/2:
		return m_dma.channel[offset / 32].device_address_counter;
	case 0x2c/2:
	case 0x6c/2:
		// Fixed-priority hardware: channel 1 precedes channel 2.
		return offset < (0x40 / 2) ? 0x00 : 0x01;
	default:
		LOGMASKED(LOG_DMA | LOG_UNKNOWN, "%s: DMA Unknown Register Read: %04x & %04x\n", machine().describe_context(), offset * 2, mem_mask);
		break;
	}
	return 0;
}

void scc68070_device::dma_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	switch (offset)
	{
	case 0x00/2:
	case 0x40/2:
		if (ACCESSING_BITS_8_15)
		{
			dma_channel_t &dma = m_dma.channel[offset / 32];
			const uint8_t clear_mask = (data >> 8) & (CSR_COC | CSR_NDT | CSR_ERR);
			dma.channel_status &= ~clear_mask;
			if (clear_mask & CSR_ERR)
				dma.channel_error = CER_NONE;
			update_ipl();
		}
		break;

	case 0x04/2:
	case 0x44/2:
		if (ACCESSING_BITS_0_7)
		{
			const int channel = offset / 32;
			m_dma.channel[channel].operation_control =
				(data & (SCC68070_OCR_D | SCC68070_OCR_OS)) | DMA_OCR_READ_FIXED;
			if (channel == 0)
			{
				m_dma.channel[0].device_control &= ~DCR1_DS;
				if (m_dma.channel[0].operation_control & SCC68070_OCR_OS)
					m_dma.channel[0].device_control |= DCR1_DS;
			}
			else if ((m_dma.channel[1].device_control & DCR2_DT) == DCR2_DT)
			{
				m_dma.channel[1].device_control &= ~DCR2_DS;
				if (m_dma.channel[1].operation_control & SCC68070_OCR_OS)
					m_dma.channel[1].device_control |= DCR2_DS;
			}
		}
		if (ACCESSING_BITS_8_15)
		{
			const int channel = offset / 32;
			if (channel == 0)
			{
				m_dma.channel[0].device_control = ((data >> 8) & DCR1_ERM) | DCR1_DT;
				if (m_dma.channel[0].operation_control & SCC68070_OCR_OS)
					m_dma.channel[0].device_control |= DCR1_DS;
			}
			else
			{
				m_dma.channel[1].device_control = (data >> 8) & (DCR2_ERM | DCR2_DT | DCR2_DS);
				if ((m_dma.channel[1].device_control & DCR2_DT) == DCR2_DT)
				{
					m_dma.channel[1].device_control &= ~DCR2_DS;
					if (m_dma.channel[1].operation_control & SCC68070_OCR_OS)
						m_dma.channel[1].device_control |= DCR2_DS;
				}
			}
		}
		break;
	case 0x06/2:
	case 0x46/2:
		if (ACCESSING_BITS_0_7)
		{
			dma_channel_t &dma = m_dma.channel[offset / 32];
			dma.channel_control = data & (CCR_INE | CCR_IPL);
			if (data & CCR_SA)