
scc68070_device::scc68070_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: scc68070_base_device(mconfig, tag, owner, clock, SCC68070, address_map_constructor(FUNC(scc68070_device::internal_map), this))
	, m_iack2_callback(*this, autovector(2))
	, m_iack4_callback(*this, autovector(4))
	, m_iack5_callback(*this, autovector(5))
	, m_iack7_callback(*this, autovector(7))
	, m_uart_tx_callback(*this)
	, m_uart_rtsn_callback(*this)
	, m_uart_break_callback(*this)
	, m_timer1_out_callback(*this)
	, m_timer2_out_callback(*this)
	, m_i2c_scl_callback(*this)
	, m_i2c_sdaw_callback(*this)
	, m_i2c_sdar_callback(*this, 0)
	, m_ipl(0)
	, m_in2_line(CLEAR_LINE)
	, m_in4_line(CLEAR_LINE)
	, m_in5_line(CLEAR_LINE)
	, m_nmi_line(CLEAR_LINE)
	, m_int1_line(CLEAR_LINE)
	, m_int2_line(CLEAR_LINE)
{
	m_cpu_space_config.m_internal_map = address_map_constructor(FUNC(scc68070_device::cpu_space_map), this);
}

void scc68070_device::device_start()
{
	scc68070_base_device::device_start();

	save_item(NAME(m_ipl));
	save_item(NAME(m_in2_line));
	save_item(NAME(m_in4_line));
	save_item(NAME(m_in5_line));
	save_item(NAME(m_nmi_line));
	save_item(NAME(m_int1_line));
	save_item(NAME(m_int2_line));
	save_item(NAME(m_lir));
	save_item(NAME(m_picr1));
	save_item(NAME(m_picr2));
	save_item(NAME(m_timer_int));
	save_item(NAME(m_i2c_int));
	save_item(NAME(m_uart_rx_int));
	save_item(NAME(m_uart_tx_int));

	save_item(NAME(m_i2c.data_register));
	save_item(NAME(m_i2c.address_register));
	save_item(NAME(m_i2c.status_register));
	save_item(NAME(m_i2c.control_register));
	save_item(NAME(m_i2c.clock_control_register));
	save_item(NAME(m_i2c.scl_out_state));
	save_item(NAME(m_i2c.scl_in_state));
	save_item(NAME(m_i2c.sda_out_state));
	save_item(NAME(m_i2c.sda_in_state));
	save_item(NAME(m_i2c.state));
	save_item(NAME(m_i2c.counter));
	save_item(NAME(m_i2c.clock_change_state));
	save_item(NAME(m_i2c.clocks));
	save_item(NAME(m_i2c.first_byte));
	save_item(NAME(m_i2c.ack_or_nak_sent));

	save_item(NAME(m_uart.mode_register));
	save_item(NAME(m_uart.status_register));
	save_item(NAME(m_uart.clock_select));
	save_item(NAME(m_uart.command_register));
	save_item(NAME(m_uart.receive_holding_register));
	save_item(NAME(m_uart.receive_pointer));
	save_item(NAME(m_uart.receive_buffer));
	save_item(NAME(m_uart.transmit_holding_register));
	save_item(NAME(m_uart.transmit_pointer));
	save_item(NAME(m_uart.transmit_buffer));
	save_item(NAME(m_uart.transmit_ctsn));
	save_item(NAME(m_uart.break_active));
	save_item(NAME(m_uart.break_release_pending));

	save_item(NAME(m_timers.timer_status_register));
	save_item(NAME(m_timers.timer_control_register));
	save_item(NAME(m_timers.reload_register));
	save_item(NAME(m_timers.timer0));
	save_item(NAME(m_timers.timer1));
	save_item(NAME(m_timers.timer2));
	save_item(NAME(m_timers.timer1_input));
	save_item(NAME(m_timers.timer2_input));
	save_item(NAME(m_timers.timer1_output));
	save_item(NAME(m_timers.timer2_output));

	save_item(STRUCT_MEMBER(m_dma.channel, channel_status));
	save_item(STRUCT_MEMBER(m_dma.channel, channel_error));
	save_item(STRUCT_MEMBER(m_dma.channel, device_control));
	save_item(STRUCT_MEMBER(m_dma.channel, operation_control));
	save_item(STRUCT_MEMBER(m_dma.channel, sequence_control));
	save_item(STRUCT_MEMBER(m_dma.channel, channel_control));
	save_item(STRUCT_MEMBER(m_dma.channel, transfer_counter));
	save_item(STRUCT_MEMBER(m_dma.channel, memory_address_counter));
	save_item(STRUCT_MEMBER(m_dma.channel, device_address_counter));

	save_item(NAME(m_mmu.status));
	save_item(NAME(m_mmu.control));
	save_item(STRUCT_MEMBER(m_mmu.desc, attr));
	save_item(STRUCT_MEMBER(m_mmu.desc, length));
	save_item(STRUCT_MEMBER(m_mmu.desc, segment));
	save_item(STRUCT_MEMBER(m_mmu.desc, base));

	m_timers.timer0_timer = timer_alloc(FUNC(scc68070_device::timer0_callback), this);
	m_timers.timer1_match_timer = timer_alloc(FUNC(scc68070_device::timer1_match_callback), this);
	m_timers.timer2_match_timer = timer_alloc(FUNC(scc68070_device::timer2_match_callback), this);
	m_timers.timer0_timer->adjust(attotime::never);
	m_timers.timer1_match_timer->adjust(attotime::never);
	m_timers.timer2_match_timer->adjust(attotime::never);

	m_uart.rx_timer = timer_alloc(FUNC(scc68070_device::rx_callback), this);
	m_uart.tx_timer = timer_alloc(FUNC(scc68070_device::tx_callback), this);
	m_uart.break_timer = timer_alloc(FUNC(scc68070_device::uart_break_timer_callback), this);
	m_uart.rx_timer->adjust(attotime::never);
	m_uart.tx_timer->adjust(attotime::never);
	m_uart.break_timer->adjust(attotime::never);

	m_i2c.timer = timer_alloc(FUNC(scc68070_device::i2c_callback), this);
	m_i2c.timer->adjust(attotime::never);
}

void scc68070_device::reset_peripheral_state()
{
	m_lir = 0;
	m_picr1 = 0;
	m_picr2 = 0;
	m_timer_int = false;
	m_i2c_int = false;
	m_uart_rx_int = false;
	m_uart_tx_int = false;

	m_i2c.data_register = 0;
	m_i2c.address_register = 0;
	m_i2c.status_register = ISR_PIN;
	m_i2c.control_register = 0;
	m_i2c.clock_control_register = 0;
	m_i2c.scl_out_state = true;
	m_i2c.scl_in_state = true;
	m_i2c.sda_out_state = true;
	m_i2c.sda_in_state = true;
	m_i2c.state = I2C_IDLE;
	m_i2c.counter = 0;
	m_i2c.clock_change_state = I2C_SCL_IDLE;
	m_i2c.clocks = 0;
	m_i2c.first_byte = false;
	m_i2c.ack_or_nak_sent = false;

	// Philips reset state: all UART mode/command/clock/status bits clear.
	m_uart.mode_register = 0;
	m_uart.status_register = 0;
	m_uart.clock_select = 0;
	m_uart.command_register = 0;
	m_uart.transmit_holding_register = 0;
	m_uart.receive_holding_register = 0;
	m_uart.receive_pointer = -1;
	std::fill(std::begin(m_uart.receive_buffer), std::end(m_uart.receive_buffer), 0);
	m_uart.transmit_pointer = -1;
	std::fill(std::begin(m_uart.transmit_buffer), std::end(m_uart.transmit_buffer), 0);
	m_uart.transmit_ctsn = true;
	m_uart.break_active = false;
	m_uart.break_release_pending = false;
	m_uart_break_callback(CLEAR_LINE);

	m_timers.timer_status_register = 0;
	m_timers.timer_control_register = 0;
	m_timers.reload_register = 0;
	m_timers.timer0 = 0;
	m_timers.timer1 = 0;
	m_timers.timer2 = 0;
	m_timers.timer1_input = false;
	m_timers.timer2_input = false;
	m_timers.timer1_output = false;
	m_timers.timer2_output = false;
	m_timer1_out_callback(CLEAR_LINE);
	m_timer2_out_callback(CLEAR_LINE);

	for (int index = 0; index < 2; index++)
	{
		scc68070::reset_dma_control_state(
				m_dma.channel[index],
				index == 0 ? DCR1_DT : 0,
				DMA_OCR_READ_FIXED,
				index == 0 ? SCR1_MAC_INC : 0);
	}

	m_mmu.status = 0;
	m_mmu.control = 0;
	for (int index = 0; index < 8; index++)
	{
		m_mmu.desc[index].attr = 0;
		m_mmu.desc[index].length = 0;
		m_mmu.desc[index].segment = 0;
		m_mmu.desc[index].base = 0;
	}

	m_uart.rx_timer->adjust(attotime::never);
	m_uart.tx_timer->adjust(attotime::never);
	m_uart.break_timer->adjust(attotime::never);
	m_timers.timer1_match_timer->adjust(attotime::never);
	m_timers.timer2_match_timer->adjust(attotime::never);
	m_i2c.timer->adjust(attotime::never);
	set_timer_callback(0);
	update_ipl();
}

void scc68070_device::device_reset()
{
	scc68070_base_device::device_reset();
	reset_peripheral_state();
}

void scc68070_device::device_config_complete()
{
	scc68070_base_device::device_config_complete();
	reset_cb().append(*this, FUNC(scc68070_device::reset_peripherals));
}

void scc68070_device::reset_peripherals(int state)
{
	if (state)
		reset_peripheral_state();
}

void scc68070_device::update_ipl()
{
	const uint8_t external_level = (m_nmi_line == ASSERT_LINE) ? 7
		: (m_in5_line == ASSERT_LINE) ? 5
		: (m_in4_line == ASSERT_LINE) ? 4
		: (m_in2_line == ASSERT_LINE) ? 2 : 0;
	const uint8_t int1_level = BIT(m_lir, 7) ? (m_lir >> 4) & 7 : 0;
	const uint8_t int2_level = BIT(m_lir, 3) ? m_lir & 7 : 0;
	const uint8_t timer_level = m_timer_int ? m_picr1 & 7 : 0;
	const uint8_t uart_rx_level = m_uart_rx_int ? (m_picr2 >> 4) & 7 : 0;
	const uint8_t uart_tx_level = m_uart_tx_int ? m_picr2 & 7 : 0;
	const uint8_t i2c_level = m_i2c_int ? (m_picr1 >> 4) & 7 : 0;
	const uint8_t dma_ch1_level = (m_dma.channel[0].channel_status & CSR_COC) && (m_dma.channel[0].channel_control & CCR_INE) ? m_dma.channel[0].channel_control & CCR_IPL : 0;
	const uint8_t dma_ch2_level = (m_dma.channel[1].channel_status & CSR_COC) && (m_dma.channel[1].channel_control & CCR_INE) ? m_dma.channel[1].channel_control & CCR_IPL : 0;

	const uint8_t new_ipl = scc68070::highest_interrupt_level(std::array<uint8_t, 9>{
		external_level, int1_level, int2_level, timer_level, uart_rx_level,
		uart_tx_level, i2c_level, dma_ch1_level, dma_ch2_level });

	if (m_ipl != new_ipl)
	{
		if (m_ipl != 0)
			set_input_line(m_ipl, CLEAR_LINE);
		if (new_ipl != 0)
			set_input_line(new_ipl, ASSERT_LINE);
		m_ipl = new_ipl;
	}
}

void scc68070_device::in2_w(int state)
{
	m_in2_line = state;
	update_ipl();
}
