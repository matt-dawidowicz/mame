// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/******************************************************************************

    SCC68070 SoC peripheral emulation
    ---------------------------------

    written by Ryan Holtz

*******************************************************************************

STATUS:

- SCC68070 peripheral implementation under specification-completion audit.
  Core interrupt arbitration, DMA register state, UART, timers, I2C and MMU
  are modeled; remaining uncertainty is tracked at individual behavior sites.

*******************************************************************************/

#include "emu.h"
#include "scc68070.h"
#include "scc68070_helpers.h"

#define LOG_I2C         (1U << 1)
#define LOG_UART        (1U << 2)
#define LOG_TIMERS      (1U << 3)
#define LOG_TIMERS_HF   (1U << 4)
#define LOG_DMA         (1U << 5)
#define LOG_MMU         (1U << 6)
#define LOG_IRQS        (1U << 7)
#define LOG_UNKNOWN     (1U << 8)
#define LOG_MORE_UART   (1U << 9)
#define LOG_ALL         (LOG_I2C | LOG_UART | LOG_TIMERS | LOG_DMA | LOG_MMU | LOG_IRQS | LOG_UNKNOWN)

#define VERBOSE         (0)

#include "logmacro.h"

#define ENABLE_UART_PRINTING (0)

enum isr_bits
{
	ISR_MST      =  0x80,
	ISR_TRX      =  0x40,
	ISR_BB       =  0x20,
	ISR_PIN      =  0x10,
	ISR_AL       =  0x08,
	ISR_AAS      =  0x04,
	ISR_AD0      =  0x02,
	ISR_LRB      =  0x01,
	ISR_SSR_MASK =  (ISR_MST | ISR_TRX | ISR_BB),
	ISR_START    =  (ISR_MST | ISR_TRX | ISR_BB),
	ISR_STOP     =  (ISR_MST | ISR_TRX)
};

enum umr_bits
{
	UMR_OM          = 0xc0,
	UMR_OM_NORMAL   = 0x00,
	UMR_OM_ECHO     = 0x40,
	UMR_OM_LOOPBACK = 0x80,
	UMR_OM_RLOOP    = 0xc0,
	UMR_TXC         = 0x10,
	UMR_PC          = 0x08,
	UMR_P           = 0x04,
	UMR_SB          = 0x02,
	UMR_CL          = 0x01
};

enum usr_bits
{
	USR_RB          = 0x80,
	USR_FE          = 0x40,
	USR_PE          = 0x20,
	USR_OE          = 0x10,
	USR_TXEMT       = 0x08,
	USR_TXRDY       = 0x04,
	USR_RXRDY       = 0x01
};

enum tsr_bits
{
	TSR_OV0         = 0x80,
	TSR_MA1         = 0x40,
	TSR_CAP1        = 0x20,
	TSR_OV1         = 0x10,
	TSR_MA2         = 0x08,
	TSR_CAP2        = 0x04,
	TSR_OV2         = 0x02
};

enum tcr_bits
{
	TCR_E1          = 0xc0,
	TCR_E1_NONE     = 0x00,
	TCR_E1_RISING   = 0x40,
	TCR_E1_FALLING  = 0x80,
	TCR_E1_BOTH     = 0xc0,
	TCR_M1          = 0x30,
	TCR_M1_NONE     = 0x00,
	TCR_M1_MATCH    = 0x10,
	TCR_M1_CAPTURE  = 0x20,
	TCR_M1_COUNT    = 0x30,
	TCR_E2          = 0x0c,
	TCR_E2_NONE     = 0x00,
	TCR_E2_RISING   = 0x04,
	TCR_E2_FALLING  = 0x08,
	TCR_E2_BOTH     = 0x0c,
	TCR_M2          = 0x03,
	TCR_M2_NONE     = 0x00,
	TCR_M2_MATCH    = 0x01,
	TCR_M2_CAPTURE  = 0x02,
	TCR_M2_COUNT    = 0x03
};

enum csr_bits
{
	CSR_COC         = 0x80,
	CSR_NDT         = 0x20,
	CSR_ERR         = 0x10,
	CSR_CA          = 0x08
};

enum cer_bits
{
	CER_EC          = 0x1f,
	CER_NONE        = 0x00,
	CER_TIMING      = 0x02,
	CER_BUSERR_MEM  = 0x09,
	CER_BUSERR_DEV  = 0x0a,
	CER_SOFT_ABORT  = 0x11
};

enum dcr1_bits
{
	DCR1_ERM        = 0x80,
	DCR1_DT         = 0x30,
	DCR1_DS         = 0x08
};

enum dcr2_bits
{
	DCR2_ERM        = 0x80,
	DCR2_DT         = 0x30,
	DCR2_DS         = 0x08
};

enum scr1_bits
{
	SCR1_MAC_INC    = 0x04
};

static constexpr uint32_t DMA_ADDRESS_MASK = scc68070::DMA_ADDRESS_MASK;
static constexpr uint8_t DMA_OCR_READ_FIXED = 0x02;

enum scr2_bits
{
	SCR2_MAC        = 0x0c,
	SCR2_MAC_NONE   = 0x00,
	SCR2_MAC_INC    = 0x04,
	SCR2_DAC        = 0x03,
	SCR2_DAC_NONE   = 0x00,
	SCR2_DAC_INC    = 0x01
};

enum ccr_bits
{
	CCR_SO          = 0x80,
	CCR_SA          = 0x10,
	CCR_INE         = 0x08,
	CCR_IPL         = 0x07
};

enum icr_bits
{
	ICR_SEL = 0x40,
	ICR_ESO = 0x08,
	ICR_ACK = 0x04
};

enum i2c_states
{
	I2C_IDLE = 0,
	I2C_TX_IN_PROGRESS,
	I2C_RX_IN_PROGRESS,
	I2C_RX_COMPLETE,
	I2C_GET_ACK,
	I2C_SEND_ACK,
	I2C_SEND_ACK_AND_RX,
	I2C_SEND_ACK_AND_STOP,
	I2C_SEND_STOP,
	I2C_CHANGED_TO_RX,
	I2C_SEND_RESTART
};

enum i2c_clock_states
{
	I2C_SCL_IDLE = 0,
	I2C_SCL_SET_0,
	I2C_SCL_SET_1,
	I2C_SCL_WAIT_1,
};

DEFINE_DEVICE_TYPE(SCC68070, scc68070_device, "scc68070", "Philips SCC68070")

void scc68070_device::internal_map(address_map &map)
{
	map(0x80001001, 0x80001001).rw(FUNC(scc68070_device::lir_r), FUNC(scc68070_device::lir_w));
	map(0x80002001, 0x80002001).rw(FUNC(scc68070_device::idr_r), FUNC(scc68070_device::idr_w));
	map(0x80002003, 0x80002003).rw(FUNC(scc68070_device::iar_r), FUNC(scc68070_device::iar_w));
	map(0x80002005, 0x80002005).rw(FUNC(scc68070_device::isr_r), FUNC(scc68070_device::isr_w));
	map(0x80002007, 0x80002007).rw(FUNC(scc68070_device::icr_r), FUNC(scc68070_device::icr_w));
	map(0x80002009, 0x80002009).rw(FUNC(scc68070_device::iccr_r), FUNC(scc68070_device::iccr_w));

	// Philips UART register map.  2017/201b are reserved.
	map(scc68070::UART_RHR_ADDRESS, scc68070::UART_RHR_ADDRESS).r(FUNC(scc68070_device::urh_r));
	map(scc68070::UART_THR_ADDRESS, scc68070::UART_THR_ADDRESS).w(FUNC(scc68070_device::uth_w));
	map(scc68070::UART_USR_ADDRESS, scc68070::UART_USR_ADDRESS).r(FUNC(scc68070_device::usr_r));
	map(scc68070::UART_UMR_ADDRESS, scc68070::UART_UMR_ADDRESS).rw(FUNC(scc68070_device::umr_r), FUNC(scc68070_device::umr_w));
	map(scc68070::UART_UCR_ADDRESS, scc68070::UART_UCR_ADDRESS).w(FUNC(scc68070_device::ucr_w));
	map(scc68070::UART_UCS_ADDRESS, scc68070::UART_UCS_ADDRESS).rw(FUNC(scc68070_device::ucsr_r), FUNC(scc68070_device::ucsr_w));

	map(0x80002020, 0x80002029).rw(FUNC(scc68070_device::timer_r), FUNC(scc68070_device::timer_w));
	map(0x80002045, 0x80002045).rw(FUNC(scc68070_device::picr1_r), FUNC(scc68070_device::picr1_w));
	map(0x80002047, 0x80002047).rw(FUNC(scc68070_device::picr2_r), FUNC(scc68070_device::picr2_w));
	map(0x80004000, 0x8000406d).rw(FUNC(scc68070_device::dma_r), FUNC(scc68070_device::dma_w));
	map(0x80008000, 0x8000807f).rw(FUNC(scc68070_device::mmu_r), FUNC(scc68070_device::mmu_w));
}

void scc68070_device::cpu_space_map(address_map &map)
{
	map(0xfffffff0, 0xffffffff).r(FUNC(scc68070_device::iack_r)).umask16(0x00ff);
}

//-------------------------------------------------
//  scc68070_device - constructor
//-------------------------------------------------


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
	, m_dma_reconfigure_callback(*this)
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
	// uart_rx() and local-loopback injection both occur when a complete serial
	// character has arrived.  Transfer it directly into RHR instead of adding a
	// second character-time delay inside the SCC.
	if ((m_uart.command_register & 3) != 1 && mode != UMR_OM_LOOPBACK)
		return;

	// The SCC has a receive holding register, not an unbounded guest-visible
	// FIFO.  If software has not consumed the previous character when the next
	// complete character arrives, report overrun and replace the old RHR value.
	if (m_uart.status_register & USR_RXRDY)
		m_uart.status_register |= USR_OE;
	m_uart.receive_holding_register = data & scc68070::uart_character_mask(m_uart.mode_register);
	m_uart.status_register |= USR_RXRDY;
	if (framing_error)
		m_uart.status_register |= USR_FE;
	if (parity_error)
		m_uart.status_register |= USR_PE;
	if (break_received)
		m_uart.status_register |= USR_RB;

	m_uart_rx_int = true;
	update_ipl();
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
	// Character completion is delivered synchronously by uart_rx() or by the
	// TX character timer in local loopback.  Keep this legacy timer inert until
	// its state is removed in the consolidation cleanup.
	m_uart.rx_timer->adjust(attotime::never);
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

	const uint8_t mode = m_uart.mode_register & UMR_OM;
	if (mode == UMR_OM_ECHO || mode == UMR_OM_RLOOP)
	{
		m_uart.status_register &= ~(USR_TXRDY | USR_TXEMT);
		m_uart_tx_int = false;
	}
	else if (((m_uart.command_register >> 2) & 3) == 1 && m_uart.transmit_pointer < 0)
		m_uart.status_register |= USR_TXRDY | USR_TXEMT;

	update_ipl();
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
	// Receive characters arrive through uart_rx() only after their serial frame
	// has completed (or from the TX frame timer in local loopback), so there is
	// no second SCC receive-delay timer to schedule.
	m_uart.rx_timer->adjust(attotime::never);

	const uint32_t uart_clock = scc68070::uart_baud_clock(
		clock(), m_uart_external_clock, BIT(m_uart.clock_select, 7));
	if (!uart_clock)
	{
		m_uart.tx_timer->adjust(attotime::never);
		return;
	}

	const uint64_t frame_bits = scc68070::uart_frame_bits(m_uart.mode_register);
	const attotime tx_rate = attotime::from_ticks(
		uint64_t(scc68070::uart_baud_divisor(m_uart.clock_select & 7)) * frame_bits,
		uart_clock);
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

	const uint8_t mode = m_uart.mode_register & UMR_OM;
	if (transmitter_command == 1 && m_uart.transmit_pointer < 0
		&& mode != UMR_OM_ECHO && mode != UMR_OM_RLOOP)
		m_uart.status_register |= USR_TXRDY | USR_TXEMT;
	if (mode == UMR_OM_ECHO || mode == UMR_OM_RLOOP)
	{
		m_uart.status_register &= ~(USR_TXRDY | USR_TXEMT);
		m_uart_tx_int = false;
	}

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

	update_ipl();
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
		m_uart.status_register &= ~USR_RXRDY;
		if (m_uart_rx_int)
		{
			m_uart_rx_int = false;
			update_ipl();
		}
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
		// the currently running Timer-0 interval.  They do change any match
		// delay that crosses the next overflow, so recompute those callbacks.
		COMBINE_DATA(&m_timers.reload_register);
		timer_schedule_match(1);
		timer_schedule_match(2);
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
	if (offset <= (0x2c / 2))
		m_dma_reconfigure_callback(0);
	else if (offset >= (0x40 / 2) && offset <= (0x6c / 2))
		m_dma_reconfigure_callback(1);

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

uint32_t scc68070_device::dma_channel_remaining(unsigned channel) const
{
	if (channel >= 2)
		return 0;
	uint16_t const counter = m_dma.channel[channel].transfer_counter;
	return !counter && dma_channel_active(channel) ? 65536U : counter;
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
