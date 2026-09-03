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
