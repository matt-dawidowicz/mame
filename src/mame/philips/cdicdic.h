// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/******************************************************************************


    CD-i Mono-I CDIC MCU simulation
    -------------------

    written by Ryan Holtz


*******************************************************************************

STATUS:

- Just enough for the Mono-I CD-i board to work somewhat properly.

TODO:

- Decapping and proper emulation.

*******************************************************************************/

#ifndef MAME_PHILIPS_CDICDIC_H
#define MAME_PHILIPS_CDICDIC_H

#pragma once

#include "cdicdic_state.h"

#include "imagedev/cdromimg.h"
#include "machine/scc68070.h"
#include "sound/cdda.h"
#include "sound/dmadac.h"

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> cdicdic_device

class cdicdic_device : public device_t
{
public:
	// construction/destruction
	cdicdic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void set_clock2(uint32_t clock) { m_clock2 = clock; }
	void set_clock2(const XTAL &xtal) { set_clock2(xtal.value()); }
	uint32_t clock2() const { return m_clock2; }

	auto intreq_callback() { return m_intreq_callback.bind(); }

	// non-static internal members
	void sample_trigger();

	uint16_t regs_r(offs_t offset, uint16_t mem_mask = ~0);
	void regs_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t ram_r(offs_t offset, uint16_t mem_mask = ~0);
	void ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	uint8_t intack_r();
	void atten_w(uint32_t state);

protected:
	// device_t implementation
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// internal callbacks
	TIMER_CALLBACK_MEMBER(sector_tick);
	TIMER_CALLBACK_MEMBER(audio_tick);

private:
	enum : uint8_t
	{
		DISC_NONE  = uint8_t(cdic_hle::disc_operation::none),
		DISC_MODE1 = uint8_t(cdic_hle::disc_operation::mode1),
		DISC_MODE2 = uint8_t(cdic_hle::disc_operation::mode2),
		DISC_CDDA  = uint8_t(cdic_hle::disc_operation::cdda),
		DISC_TOC   = uint8_t(cdic_hle::disc_operation::toc)
	};

	enum
	{
		SECTOR_SYNC        = 0,

		SECTOR_HEADER      = 12,

		SECTOR_MINUTES     = 12,
		SECTOR_SECONDS     = 13,
		SECTOR_FRACS       = 14,
		SECTOR_MODE        = 15,

		SECTOR_FILE1       = 16,
		SECTOR_CHAN1       = 17,
		SECTOR_SUBMODE1    = 18,
		SECTOR_CODING1     = 19,

		SECTOR_FILE2       = 20,
		SECTOR_CHAN2       = 21,
		SECTOR_SUBMODE2    = 22,
		SECTOR_CODING2     = 23,

		SECTOR_DATA        = 24,

		SECTOR_SIZE        = 2352,
		SECTOR_AUDIO_SIZE  = 2304,

		SECTOR_DATASIZE    = 2048,
		SECTOR_AUDIOSIZE   = 2304,
		SECTOR_VIDEOSIZE   = 2324,

		SUBMODE_EOF        = 0x80,
		SUBMODE_RT         = 0x40,
		SUBMODE_FORM       = 0x20,
		SUBMODE_TRIG       = 0x10,
		SUBMODE_DATA       = 0x08,
		SUBMODE_AUDIO      = 0x04,
		SUBMODE_VIDEO      = 0x02,
		SUBMODE_EOR        = 0x01,

		SUBCODE_Q_CONTROL       = 12,
		SUBCODE_Q_TRACK         = 13,
		SUBCODE_Q_INDEX         = 14,
		SUBCODE_Q_MODE1_MINS    = 15,
		SUBCODE_Q_MODE1_SECS    = 16,
		SUBCODE_Q_MODE1_FRAC    = 17,
		SUBCODE_Q_MODE1_ZERO    = 18,
		SUBCODE_Q_MODE1_AMINS   = 19,
		SUBCODE_Q_MODE1_ASECS   = 20,
		SUBCODE_Q_MODE1_AFRAC   = 21,
		SUBCODE_Q_CRC0          = 22,
		SUBCODE_Q_CRC1          = 23
	};

	devcb_write_line m_intreq_callback;

	required_device_array<dmadac_sound_device, 2> m_dmadac;
	required_device<scc68070_device> m_scc;
	required_device<cdrom_image_device> m_cdrom;

	uint32_t m_clock2;

	// internal state
	uint16_t m_command;           // CDIC Command Register            (0x303c00)
	uint32_t m_time;              // CDIC Time Register               (0x303c02)
	uint16_t m_file;              // CDIC File Register               (0x303c06)
	uint32_t m_channel;           // CDIC Channel Register            (0x303c08)
	uint16_t m_audio_channel;     // CDIC Audio Channel Register      (0x303c0c)
	cdic_hle::mode2_filter_state m_mode2_filter;
	uint16_t m_data_select = 0;

	uint16_t m_audio_buffer;      // CDIC Audio Buffer Register       (0x303ff4)
	uint16_t m_x_buffer;          // CDIC X-Buffer Register           (0x303ff6)
	uint16_t m_dma_control;       // CDIC DMA Control Register        (0x303ff8)
	uint16_t m_z_buffer;          // CDIC Z-Buffer Register           (0x303ffa)
	uint16_t m_interrupt_vector;  // CDIC Interrupt Vector Register   (0x303ffc)
	uint16_t m_data_buffer;       // CDIC Data Buffer Register        (0x303ffe)

	bool m_cd_byteswap;

	emu_timer *m_sector_timer;
	uint8_t m_disc_command;
	uint8_t m_disc_state;
	uint8_t m_disc_mode;
	uint8_t m_disc_spinup_counter;
	uint32_t m_curr_lba;
	uint8_t m_next_data_buffer;
	uint8_t m_next_audio_buffer;

	emu_timer *m_audio_timer;
	cdic_hle::audio_map_state m_audio_map;
	cdic_hle::realtime_audio_state m_realtime_audio;
	bool m_cdda_pending;
	std::unique_ptr<uint8_t[]> m_cdda_pending_data;

	// Audio Attenuation (L->L, L->R, R->R, R->L)
	uint8_t m_atten[4];
	int16_t m_xa_last[4];
	std::unique_ptr<uint8_t[]> m_ram;
	std::unique_ptr<int16_t[]> m_samples[2];

	void play_xa_group(const cdic_hle::xa_coding &coding, const uint8_t *data, const uint16_t idx);
	void play_audio_sector(const uint8_t coding, const uint8_t *data);
	void play_cdda_sector(const uint8_t *data);
	void receive_cdda_sector(const uint8_t *data);
	void try_play_realtime_audio();
	void start_realtime_audio();
	void stop_realtime_audio();
	void process_audio_map();

	void descramble_sector(uint8_t *buffer);
	bool is_valid_sector(const uint8_t *buffer);
	cdic_hle::sector_decision mode2_sector_decision(const uint8_t *buffer) const;

	void process_disc_sector();
	void process_sector_data(const uint8_t *buffer, const uint8_t *subcode_buffer, bool audio_sector);
	void update_mode2_filters(cdic_hle::mode2_filter_boundary boundary);
	void init_disc_read(uint8_t disc_mode);
	void cancel_disc_read();
	void handle_cdic_command();

	void update_interrupt_state();

	uint32_t lba_from_time();

	static uint8_t get_sector_count_for_coding(uint8_t coding);

	static const int32_t s_samples_per_sector;
	static const uint16_t s_crc_ccitt_table[256];
	static const uint8_t s_sector_scramble[2448];
};

// device type definition
DECLARE_DEVICE_TYPE(CDI_CDIC, cdicdic_device)

#endif // MAME_PHILIPS_CDICDIC_H
