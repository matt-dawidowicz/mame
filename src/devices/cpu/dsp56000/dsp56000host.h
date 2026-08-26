// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay,Matt Dawidowicz

#ifndef MAME_CPU_DSP56000_DSP56000HOST_H
#define MAME_CPU_DSP56000_DSP56000HOST_H

#pragma once

#include <cstdint>

class dsp56000_host_interface
{
public:
	enum host_register : unsigned
	{
		ICR  = 0,
		CVR  = 1,
		ISR  = 2,
		IVR  = 3,
		TRX0 = 4,
		TRXH = 5,
		TRXM = 6,
		TRXL = 7
	};

	static constexpr std::uint8_t ICR_RREQ = 0x01;
	static constexpr std::uint8_t ICR_TREQ = 0x02;
	static constexpr std::uint8_t ICR_HF0  = 0x08;
	static constexpr std::uint8_t ICR_HF1  = 0x10;
	static constexpr std::uint8_t ICR_INIT = 0x80;

	static constexpr std::uint8_t CVR_HV_MASK = 0x3f;
	static constexpr std::uint8_t CVR_HC      = 0x80;

	static constexpr std::uint8_t ISR_RXDF = 0x01;
	static constexpr std::uint8_t ISR_TXDE = 0x02;
	static constexpr std::uint8_t ISR_TRDY = 0x04;

	static constexpr std::uint8_t HSR_HRDF = 0x01;
	static constexpr std::uint8_t HSR_HTDE = 0x02;
	static constexpr std::uint8_t HSR_HCP  = 0x04;
	static constexpr std::uint8_t HSR_HF0  = 0x08;
	static constexpr std::uint8_t HSR_HF1  = 0x10;

	static constexpr unsigned BOOTSTRAP_WORDS = 0x200;

	void reset() noexcept
	{
		for (std::uint8_t &value : m_hostport)
			value = 0;

		for (std::uint8_t &value : m_tx)
			value = 0;

		for (std::uint8_t &value : m_rx)
			value = 0;

		for (std::uint32_t &value : m_bootstrap)
			value = 0;

		m_hostport[CVR] = 0x12;
		m_hostport[ISR] = ISR_TRDY | ISR_TXDE;
		m_hostport[IVR] = 0x0f;

		m_hsr = HSR_HTDE;
		m_dsp_host_rtx = 0;
		m_dsp_host_htx = 0;
		m_bootstrap_pos = 0;
		m_running = false;
	}

	std::uint8_t read(unsigned offset) noexcept
	{
		offset &= 7U;

		switch (offset)
		{
		case TRXH:
			return m_rx[0];

		case TRXM:
			return m_rx[1];

		case TRXL:
		{
			const std::uint8_t result = m_rx[2];

			// Reading the low receive byte completes a host receive.
			m_hostport[ISR] &= ~ISR_RXDF;

			// A DSP-side HTX value may have been waiting while RXDF was full.
			transfer_dsp_to_host();

			return result;
		}

		default:
			return m_hostport[offset];
		}
	}

	void write(unsigned offset, std::uint8_t data) noexcept
	{
		offset &= 7U;

		switch (offset)
		{
		case ICR:
			write_icr(data);
			break;

		case CVR:
			// HV occupies bits 0-5; bit 6 is reserved and reads as zero.
			m_hostport[CVR] = data & (CVR_HC | CVR_HV_MASK);

			if (m_hostport[CVR] & CVR_HC)
				m_hsr |= HSR_HCP;
			else
				m_hsr &= ~HSR_HCP;
			break;

		case ISR:
			// Read-only from the host side.
			break;

		case IVR:
			m_hostport[IVR] = data;
			break;

		case TRX0:
			// Reserved/unused in normal 24-bit transfers.
			break;

		case TRXH:
			m_tx[0] = data;
			break;

		case TRXM:
			m_tx[1] = data;
			break;

		case TRXL:
			m_tx[2] = data;
			host_write_low();
			break;
		}
	}

	bool running() const noexcept
	{
		return m_running;
	}

	bool host_command_pending() const noexcept
	{
		return (m_hsr & HSR_HCP) != 0;
	}

	std::uint8_t host_command_vector() const noexcept
	{
		return m_hostport[CVR] & CVR_HV_MASK;
	}

	void acknowledge_host_command() noexcept
	{
		m_hostport[CVR] &= ~CVR_HC;
		m_hsr &= ~HSR_HCP;
	}

	bool dsp_receive_pending() const noexcept
	{
		return (m_hsr & HSR_HRDF) != 0;
	}

	bool dsp_read_rx(std::uint32_t &value) noexcept
	{
		if (!(m_hsr & HSR_HRDF))
			return false;

		value = m_dsp_host_rtx & 0x00ffffffU;

		m_hsr &= ~HSR_HRDF;

		// If the host had another word queued, move it into HRX now.
		transfer_host_to_dsp();
		update_trdy();

		return true;
	}

	bool dsp_transmit_empty() const noexcept
	{
		return (m_hsr & HSR_HTDE) != 0;
	}

	bool dsp_write_tx(std::uint32_t value) noexcept
	{
		if (!(m_hsr & HSR_HTDE))
			return false;

		m_dsp_host_htx = value & 0x00ffffffU;
		m_hsr &= ~HSR_HTDE;

		transfer_dsp_to_host();

		return true;
	}

	std::uint16_t bootstrap_pos() const noexcept
	{
		return m_bootstrap_pos;
	}

	std::uint32_t bootstrap_word(unsigned address) const noexcept
	{
		return (address < BOOTSTRAP_WORDS)
			? m_bootstrap[address]
			: 0;
	}

	std::uint8_t hsr() const noexcept
	{
		return m_hsr;
	}

	// Public state is intentional: the owning MAME device must register all
	// of this state with the save-state system.
	std::uint8_t m_hostport[8]{};
	std::uint8_t m_tx[3]{};
	std::uint8_t m_rx[3]{};

	std::uint32_t m_bootstrap[BOOTSTRAP_WORDS]{};

	std::uint8_t m_hsr = 0;
	std::uint32_t m_dsp_host_rtx = 0;
	std::uint32_t m_dsp_host_htx = 0;

	std::uint16_t m_bootstrap_pos = 0;
	bool m_running = false;

private:
	std::uint32_t tx_word() const noexcept
	{
		return
			(std::uint32_t(m_tx[0]) << 16) |
			(std::uint32_t(m_tx[1]) << 8) |
			std::uint32_t(m_tx[2]);
	}

	void write_icr(std::uint8_t data) noexcept
	{
		// Bit 2 is reserved and reads as zero.
		m_hostport[ICR] = data & 0xfb;

		// Host flags are reflected on the DSP side.
		m_hsr &= ~(HSR_HF0 | HSR_HF1);
		m_hsr |= m_hostport[ICR] & (HSR_HF0 | HSR_HF1);

		if (m_hostport[ICR] & ICR_INIT)
		{
			if (m_hostport[ICR] & ICR_RREQ)
			{
				m_hostport[ISR] &= ~ISR_RXDF;
				m_hsr |= HSR_HTDE;
			}

			if (m_hostport[ICR] & ICR_TREQ)
			{
				m_hostport[ISR] |= ISR_TXDE;
				m_hsr &= ~HSR_HRDF;
			}

			// INIT is self-clearing.
			m_hostport[ICR] &= ~ICR_INIT;
		}

		// During bootstrap, asserting HF0 releases the DSP for execution.
		if (!m_running && (m_hostport[ICR] & ICR_HF0))
			m_running = true;

		update_trdy();
	}

	void host_write_low() noexcept
	{
		if (!m_running)
		{
			if (m_bootstrap_pos < BOOTSTRAP_WORDS)
			{
				m_bootstrap[m_bootstrap_pos++] =
					tx_word() & 0x00ffffffU;

				// The standard bootstrap also starts automatically if the
				// complete 512-word bootstrap RAM has been filled.
				if (m_bootstrap_pos == BOOTSTRAP_WORDS)
					m_running = true;
			}

			return;
		}

		/*
		 * Runtime host transfer.
		 *
		 * If TRDY is asserted, HRX is empty and the transfer can move
		 * directly to the DSP side. Otherwise the host-side transmit
		 * register remains occupied until HRX becomes available.
		 */
		if (m_hostport[ISR] & ISR_TRDY)
		{
			m_dsp_host_rtx = tx_word() & 0x00ffffffU;
			m_hsr |= HSR_HRDF;
		}
		else
		{
			m_hostport[ISR] &= ~ISR_TXDE;
		}

		update_trdy();
	}

	void transfer_host_to_dsp() noexcept
	{
		// Nothing queued on the host side.
		if (m_hostport[ISR] & ISR_TXDE)
			return;

		// DSP has not consumed its current HRX word yet.
		if (m_hsr & HSR_HRDF)
			return;

		m_dsp_host_rtx = tx_word() & 0x00ffffffU;
		m_hsr |= HSR_HRDF;

		m_hostport[ISR] |= ISR_TXDE;
	}

	void transfer_dsp_to_host() noexcept
	{
		// Host receive register is still occupied.
		if (m_hostport[ISR] & ISR_RXDF)
			return;

		// DSP has no pending HTX word.
		if (m_hsr & HSR_HTDE)
			return;

		m_rx[0] = std::uint8_t(m_dsp_host_htx >> 16);
		m_rx[1] = std::uint8_t(m_dsp_host_htx >> 8);
		m_rx[2] = std::uint8_t(m_dsp_host_htx);

		m_hsr |= HSR_HTDE;
		m_hostport[ISR] |= ISR_RXDF;
	}

	void update_trdy() noexcept
	{
		m_hostport[ISR] &= ~ISR_TRDY;

		if ((m_hostport[ISR] & ISR_TXDE) &&
			!(m_hsr & HSR_HRDF))
		{
			m_hostport[ISR] |= ISR_TRDY;
		}
	}
};

#endif // MAME_CPU_DSP56000_DSP56000HOST_H