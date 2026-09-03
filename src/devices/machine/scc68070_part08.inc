
			{
				// Software abort terminates the current operation.
				dma.channel_status &= ~CSR_CA;
				dma.channel_status |= CSR_COC | CSR_ERR;
				dma.channel_error = CER_SOFT_ABORT;
			}
			else if (data & CCR_SO)
			{
				// START is an SCC controller command.  Peripheral DREQ must never
				// manufacture an active channel on its own.
				if (dma.channel_status & (CSR_COC | CSR_NDT | CSR_ERR | CSR_CA))
				{
					dma.channel_status &= ~CSR_CA;
					dma.channel_status |= CSR_COC | CSR_ERR;
					dma.channel_error = CER_TIMING;
				}
				else
				{
					dma.channel_error = CER_NONE;
					dma.channel_status |= CSR_CA;
				}
			}
			update_ipl();
		}
		if (ACCESSING_BITS_8_15)
		{
			if ((offset / 32) == 0)
				m_dma.channel[0].sequence_control = SCR1_MAC_INC;
			else
				m_dma.channel[1].sequence_control = (data >> 8) & (SCR2_MAC | SCR2_DAC);
		}
		break;
	case 0x0a/2:
	case 0x4a/2:
		COMBINE_DATA(&m_dma.channel[offset / 32].transfer_counter);
		break;
	case 0x0c/2:
	case 0x4c/2:
		m_dma.channel[offset / 32].memory_address_counter =
			scc68070::dma_address_high_write(m_dma.channel[offset / 32].memory_address_counter, data, mem_mask);
		break;
	case 0x0e/2:
	case 0x4e/2:
		m_dma.channel[offset / 32].memory_address_counter =
			scc68070::dma_address_low_write(m_dma.channel[offset / 32].memory_address_counter, data, mem_mask);
		break;
	case 0x14/2:
		break;
	case 0x54/2:
		m_dma.channel[offset / 32].device_address_counter =
			scc68070::dma_address_high_write(m_dma.channel[offset / 32].device_address_counter, data, mem_mask);
		break;
	case 0x16/2:
		break;
	case 0x56/2:
		m_dma.channel[offset / 32].device_address_counter =
			scc68070::dma_address_low_write(m_dma.channel[offset / 32].device_address_counter, data, mem_mask);
		break;
	case 0x2c/2:
	case 0x6c/2:
		// Fixed-priority register.  Writes have no effect.
		break;
	default:
		LOGMASKED(LOG_DMA | LOG_UNKNOWN, "%s: DMA Unknown Register Write: %04x = %04x & %04x\n", machine().describe_context(), offset * 2, data, mem_mask);
		break;
	}
}

bool scc68070_device::dma_channel_active(unsigned channel) const
{
	return channel < 2 && (m_dma.channel[channel].channel_status & CSR_CA);
}

bool scc68070_device::dma_channel_transfer(unsigned channel, uint16_t &data)
{
	if (!dma_channel_active(channel))
		return false;

	dma_channel_t &dma = m_dma.channel[channel];
	auto terminate_timing_error = [this, &dma]()
	{
		dma.channel_status = scc68070::dma_status_after_error(dma.channel_status);
		dma.channel_error = CER_TIMING;
		update_ipl();
	};
	address_space &memory = space(AS_PROGRAM);
	uint32_t operand_size;

	switch (dma.operation_control & SCC68070_OCR_OS)
	{
	case SCC68070_OCR_OS_BYTE:
		operand_size = 1;
		break;
	case SCC68070_OCR_OS_WORD:
		operand_size = 2;
		break;
	default:
		terminate_timing_error();
		return false;
	}

	const uint8_t memory_mode = channel == 0 ? 0x01 : (dma.sequence_control & SCR2_MAC) >> 2;
	const uint8_t device_mode = channel == 0 ? 0x00 : dma.sequence_control & SCR2_DAC;
	const scc68070::dma_address_result next_memory =
		scc68070::dma_address_after_transfer(dma.memory_address_counter, memory_mode, operand_size);
	const scc68070::dma_address_result next_device =
		scc68070::dma_address_after_transfer(dma.device_address_counter, device_mode, operand_size);

	// Reserved MAC/DAC encodings terminate the operation before any partial
	// transfer can reach memory or the peripheral.
	if (!next_memory.valid || !next_device.valid)
	{
		terminate_timing_error();
		return false;
	}

	const uint32_t memory_address = dma.memory_address_counter & DMA_ADDRESS_MASK;
	if (operand_size == 1)
	{
		if (dma.operation_control & SCC68070_OCR_D)
			memory.write_byte(memory_address, data & 0x00ff);
		else
			data = (data & 0xff00) | memory.read_byte(memory_address);
	}
	else if (dma.operation_control & SCC68070_OCR_D)
		memory.write_word(memory_address, data);
	else
		data = memory.read_word(memory_address);

	dma.memory_address_counter = next_memory.address;
	if (channel == 1)
		dma.device_address_counter = next_device.address;

	const scc68070::dma_count_result count = scc68070::dma_count_after_transfer(dma.transfer_counter);
	dma.transfer_counter = count.remaining;
	dma.channel_status = scc68070::dma_status_after_count(dma.channel_status, count.complete);
	if (count.complete)
		update_ipl();

	return true;
}

bool scc68070_device::dma_channel_device_terminate(unsigned channel)
{
	if (!dma_channel_active(channel))
		return false;

	dma_channel_t &dma = m_dma.channel[channel];
	dma.channel_status = scc68070::dma_status_after_device_termination(dma.channel_status);
	dma.channel_error = CER_NONE;
	update_ipl();
	return true;
}

bool scc68070_device::dma_channel_memory_bus_error(unsigned channel)
{
	if (!dma_channel_active(channel))
		return false;

	dma_channel_t &dma = m_dma.channel[channel];
	dma.channel_status = scc68070::dma_status_after_error(dma.channel_status);
	dma.channel_error = CER_BUSERR_MEM;
	update_ipl();
	return true;
}

bool scc68070_device::dma_channel_device_bus_error(unsigned channel)
{
	if (!dma_channel_active(channel))
		return false;

	dma_channel_t &dma = m_dma.channel[channel];
	dma.channel_status = scc68070::dma_status_after_error(dma.channel_status);
	dma.channel_error = CER_BUSERR_DEV;
	update_ipl();
	return true;
}

bool scc68070_device::dma_channel_external_start(unsigned channel)
{
	// DREQ is only a request/gate.  START/CA ownership stays in the SCC.
	return dma_channel_active(channel);
}

bool scc68070_device::dma_channel_memory_to_device(unsigned channel) const
{
	return channel < 2
		&& (m_dma.channel[channel].operation_control & SCC68070_OCR_D) == SCC68070_OCR_D_M2D;
}

bool scc68070_device::dma_channel_word_transfer(unsigned channel) const
{
	return channel < 2
		&& (m_dma.channel[channel].operation_control & SCC68070_OCR_OS) == SCC68070_OCR_OS_WORD;
}

bool scc68070_device::dma_channel_memory_increment(unsigned channel, bool &increment) const
{
	if (channel >= 2)
		return false;
	if (channel == 0)
	{
		increment = true;
		return true;
	}

	switch (m_dma.channel[1].sequence_control & SCR2_MAC)
	{
	case SCR2_MAC_NONE:
		increment = false;
		return true;
	case SCR2_MAC_INC:
		increment = true;
		return true;
	default:
		return false;
	}
}

uint16_t scc68070_device::dma_channel_remaining(unsigned channel) const
{
	return channel < 2 ? m_dma.channel[channel].transfer_counter : 0;
}

uint32_t scc68070_device::dma_channel_memory_address(unsigned channel) const
{
	return channel < 2 ? (m_dma.channel[channel].memory_address_counter & DMA_ADDRESS_MASK) : 0;
}

uint16_t scc68070_device::mmu_r(offs_t offset, uint16_t mem_mask)
{
	switch (offset)
	{
	case 0x00/2:
		return scc68070::mmu_status_control_word(m_mmu.status, m_mmu.control);
	case 0x40/2:
	case 0x48/2:
	case 0x50/2:
	case 0x58/2:
	case 0x60/2:
	case 0x68/2:
	case 0x70/2:
	case 0x78/2:
		return m_mmu.desc[(offset - 0x20) / 4].attr;
	case 0x42/2:
	case 0x4a/2:
	case 0x52/2:
	case 0x5a/2:
	case 0x62/2:
	case 0x6a/2:
	case 0x72/2:
	case 0x7a/2:
		return m_mmu.desc[(offset - 0x20) / 4].length;
	case 0x44/2:
	case 0x4c/2:
	case 0x54/2:
	case 0x5c/2:
	case 0x64/2:
	case 0x6c/2:
	case 0x74/2:
	case 0x7c/2:
		if (ACCESSING_BITS_0_7)
			return m_mmu.desc[(offset - 0x20) / 4].segment;
		break;
	case 0x46/2:
	case 0x4e/2:
	case 0x56/2:
	case 0x5e/2:
	case 0x66/2:
	case 0x6e/2:
	case 0x76/2:
	case 0x7e/2:
		return m_mmu.desc[(offset - 0x20) / 4].base;
	default:
		LOGMASKED(LOG_MMU | LOG_UNKNOWN, "%s: MMU Unknown Register Read: %04x & %04x\n", machine().describe_context(), offset * 2, mem_mask);
		break;
	}
	return 0;
}

void scc68070_device::mmu_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	switch (offset)
	{
	case 0x00/2:
		if (ACCESSING_BITS_0_7)
			m_mmu.control = scc68070::mmu_control_value(data);
		break;
	case 0x40/2:
	case 0x48/2:
	case 0x50/2:
	case 0x58/2:
	case 0x60/2:
	case 0x68/2:
	case 0x70/2:
	case 0x78/2:
		COMBINE_DATA(&m_mmu.desc[(offset - 0x20) / 4].attr);
		break;
	case 0x42/2:
	case 0x4a/2:
	case 0x52/2:
	case 0x5a/2:
	case 0x62/2:
	case 0x6a/2:
	case 0x72/2:
	case 0x7a/2:
		COMBINE_DATA(&m_mmu.desc[(offset - 0x20) / 4].length);
		m_mmu.desc[(offset - 0x20) / 4].length =
			scc68070::mmu_segment_length(m_mmu.desc[(offset - 0x20) / 4].length);
		break;
	case 0x44/2:
	case 0x4c/2:
	case 0x54/2:
	case 0x5c/2:
	case 0x64/2:
	case 0x6c/2:
	case 0x74/2:
	case 0x7c/2:
		if (ACCESSING_BITS_0_7)
			m_mmu.desc[(offset - 0x20) / 4].segment = data & 0x00ff;
		break;
	case 0x46/2:
	case 0x4e/2:
	case 0x56/2:
	case 0x5e/2:
	case 0x66/2:
	case 0x6e/2:
	case 0x76/2:
	case 0x7e/2:
		COMBINE_DATA(&m_mmu.desc[(offset - 0x20) / 4].base);
		m_mmu.desc[(offset - 0x20) / 4].base =
			scc68070::mmu_base_address(m_mmu.desc[(offset - 0x20) / 4].base);
		break;
	default:
		LOGMASKED(LOG_MMU | LOG_UNKNOWN, "%s: Unknown Register Write: %04x = %04x & %04x\n", machine().describe_context(), offset * 2, data, mem_mask);
		break;
	}
}
