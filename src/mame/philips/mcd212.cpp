// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/******************************************************************************


    CD-i MCD212 Video Decoder and System Controller emulation
    -------------------

    written by Ryan Holtz, Vincent.Halver


*******************************************************************************

TODO:

- Validate QHY first-field edge handling and full-frame output against Extended Case hardware.

*******************************************************************************/

#include "emu.h"
#include "mcd212.h"
#include "mcd212_video.h"
#include "screen.h"

#define LOG_UNKNOWNS        (1U << 1)
#define LOG_REGISTERS       (1U << 2)
#define LOG_ICA             (1U << 3)
#define LOG_DCA             (1U << 4)
#define LOG_VSR             (1U << 5)
#define LOG_STATUS          (1U << 6)
#define LOG_MAIN_REG_READS  (1U << 7)
#define LOG_MAIN_REG_WRITES (1U << 8)
#define LOG_CLUT            (1U << 9)
#define LOG_ALL             (LOG_UNKNOWNS | LOG_REGISTERS | LOG_ICA | LOG_DCA | LOG_VSR | LOG_STATUS | LOG_MAIN_REG_READS | LOG_MAIN_REG_WRITES | LOG_CLUT)

#define VERBOSE             (0)
#include "logmacro.h"

// device type definition
DEFINE_DEVICE_TYPE(MCD212, mcd212_device, "mcd212", "MCD212 VDSC")

inline ATTR_FORCE_INLINE uint8_t mcd212_device::get_weight_factor(const uint32_t matte_idx)
{
	return (uint8_t)((m_matte_control[matte_idx] & MC_WF) >> MC_WF_SHIFT);
}

inline ATTR_FORCE_INLINE uint8_t mcd212_device::get_matte_op(const uint32_t matte_idx)
{
	return (m_matte_control[matte_idx] & MC_OP) >> MC_OP_SHIFT;
}

void mcd212_device::update_matte_arrays()
{
	const int width = get_screen_width();
	const int num_mattes = BIT(m_image_coding_method, ICM_NM_BIT) ? 2 : 1;

	bool latched_mf[2]{ false, false };
	uint8_t latched_wf[2] = { m_weight_factor[0][0], m_weight_factor[1][0] };
	int matte_idx[2] = { 0, 4 };

	for (int x = 0; x < width; x++)
	{
		for (int matte = 0; matte < num_mattes; matte++)
		{
			const int max_matte_id = ((num_mattes == 2) ? 4 : 8) + (matte ? 4 : 0);
			if (matte_idx[matte] >= max_matte_id)
			{
				continue;
			}
			const uint32_t matte_ctrl = m_matte_control[matte_idx[matte]];

			if (x == (matte_ctrl & MC_X))
			{
				const uint32_t matte_op = get_matte_op(matte_idx[matte]);
				const int flag = (num_mattes == 2) ? matte : BIT(m_matte_control[matte_idx[matte]], MC_MF_BIT);
				// See 5.10.2 Matte Commands. Changing the MF-bit inside a line is undefined. Greenbook says don't do it.
				// Console validation shows the 220 reads and uses this value anyway.
				switch (matte_op)
				{
				case 0: // Disregard all commands in higher registers. See 5.10.2
					matte_idx[matte] = 8;
					break;
				case 1: case 2: case 3: case 5: case 7: case 10: case 11: // Not used
					break;
				case 4: case 6: // Change weight of plane (A or B)
					latched_wf[BIT(matte_op, 1)] = get_weight_factor(matte_idx[matte]);
					break;
				case 8: case 9: // (Reset or Set) matte flag
					latched_mf[flag] = BIT(matte_op, 0);
					break;
				case 12: case 13: case 14: case 15: // Change weight of plane (A or B) and (Reset or Set) matte flag
					latched_wf[BIT(matte_op, 1)] = get_weight_factor(matte_idx[matte]);
					latched_mf[flag] = BIT(matte_op, 0);
					break;
				}
				matte_idx[matte]++;
			}
		}
		m_weight_factor[0][x] = latched_wf[0];
		m_weight_factor[1][x] = latched_wf[1];
		m_matte_flag[0][x] = latched_mf[0];
		m_matte_flag[1][x] = latched_mf[1];
	}
}

template <int Path>
void mcd212_device::set_register(uint8_t reg, uint32_t value)
{
	switch (reg)
	{
		case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: // CLUT 0 - 63
		case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
		case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			{
				const uint8_t clut_index = m_clut_bank[Path] * 0x40 + (reg - 0x80);
				LOGMASKED(LOG_CLUT, "%s: Path %d: CLUT[%d] = %08x\n", machine().describe_context(), Path, clut_index, value);
				m_clut[clut_index] = value & 0x00fcfcfc;
				if (clut_index >= 0x80 && clut_index < 0x88)
					m_qhy_levels[clut_index - 0x80] = value & 0x00ffffff;
			}
			break;
		case 0xc0: // Image Coding Method
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Path 0: Image Coding Method = %08x\n", machine().describe_context(), value);
				m_image_coding_method = value & 0x004c0f0f;
				update_matte_arrays();
			}
			break;
		case 0xc1: // Transparency Control
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Transparency Control = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_transparency_control = value & 0x00800f0f;
			}
			break;
		case 0xc2: // Plane Order
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Plane Order = %08x\n", machine().describe_context(), screen().vpos(), value & 7);
				m_plane_order = value & 0x00000001;
			}
			break;
		case 0xc3: // CLUT Bank Register
			LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path %d: CLUT Bank Register = %08x\n", machine().describe_context(), screen().vpos(), Path, value & 3);
			m_clut_bank[Path] = Path ? (2 | (value & 0x00000001)) : (value & 0x00000003);
			break;
		case 0xc4: // Transparent Color A
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Transparent Color A = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_transparent_color[0] = value & 0x00fcfcfc;
			}
			break;
		case 0xc6: // Transparent Color B
			if (Path == 1)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 1: Transparent Color B = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_transparent_color[1] = value & 0x00fcfcfc;
			}
			break;
		case 0xc7: // Mask Color A
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Mask Color A = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_mask_color[0] = value & 0x00fcfcfc;
			}
			break;
		case 0xc9: // Mask Color B
			if (Path == 1)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 1: Mask Color B = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_mask_color[1] = value & 0x00fcfcfc;
			}
			break;
		case 0xca: // Delta YUV Absolute Start Value A
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Delta YUV Absolute Start Value A = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_dyuv_abs_start[0] = value;
			}
			break;
		case 0xcb: // Delta YUV Absolute Start Value B
			if (Path == 1)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 1: Delta YUV Absolute Start Value B = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_dyuv_abs_start[1] = value;
			}
			break;
		case 0xcd: // Cursor Position
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Cursor Position = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_cursor_position = value & 0x003ff3ff;
			}
			break;
		case 0xce: // Cursor Control
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Cursor Control = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_cursor_control = value & 0x00ff800f;
			}
			break;
		case 0xcf: // Cursor Pattern
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Cursor Pattern[%d] = %04x\n", machine().describe_context(), screen().vpos(), (value >> 16) & 0x000f, value & 0x0000ffff);
				m_cursor_pattern[(value >> 16) & 0x000f] = value & 0x0000ffff;
			}
			break;
		case 0xd0: // matte Control 0-7
		case 0xd1:
		case 0xd2:
		case 0xd3:
		case 0xd4:
		case 0xd5:
		case 0xd6:
		case 0xd7:
			LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path %d: matte Control %d = %08x\n", machine().describe_context(), screen().vpos(), Path, reg & 7, value);
			m_matte_control[reg & 7] = value & 0x00f1ffff;
			update_matte_arrays();
			break;
		case 0xd8: // Backdrop Color
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Backdrop Color = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_backdrop_color = value & 0x0000000f;
			}
			break;
		case 0xd9: // Mosaic Pixel Hold Factor A
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Mosaic Pixel Hold Factor A = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_mosaic_hold[0] = value & 0x008000ff;
			}
			break;
		case 0xda: // Mosaic Pixel Hold Factor B
			if (Path == 1)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 1: Mosaic Pixel Hold Factor B = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_mosaic_hold[1] = value & 0x008000ff;
			}
			break;
		case 0xdb: // Weight Factor A
			if (Path == 0)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 0: Weight Factor A = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_weight_factor[0][0] = value & 0x3f;
				update_matte_arrays();
			}
			break;
		case 0xdc: // Weight Factor B
			if (Path == 1)
			{
				LOGMASKED(LOG_REGISTERS, "%s: Scanline %d, Path 1: Weight Factor B = %08x\n", machine().describe_context(), screen().vpos(), value);
				m_weight_factor[1][0] = value & 0x3f;
				update_matte_arrays();
			}
			break;
	}
}

template <int Path>
inline ATTR_FORCE_INLINE uint32_t mcd212_device::get_vsr()
{
	return ((m_dcr[Path] & 0x3f) << 16) | m_vsr[Path];
}

template <int Path>
inline ATTR_FORCE_INLINE void mcd212_device::set_vsr(uint32_t value)
{
	m_vsr[Path] = value & 0x0000ffff;
	m_dcr[Path] &= 0xffc0;
	m_dcr[Path] |= (value >> 16) & 0x003f;
}

template <int Path>
inline ATTR_FORCE_INLINE void mcd212_device::set_dcp(uint32_t value)
{
	m_dcp[Path] = value & 0x0000fffc;
	m_ddr[Path] &= 0xffc0;
	m_ddr[Path] |= (value >> 16) & 0x003f;
}

template <int Path>
inline ATTR_FORCE_INLINE uint32_t mcd212_device::get_dcp()
{
	return ((m_ddr[Path] & 0x3f) << 16) | m_dcp[Path];
}

template <int Path>
inline ATTR_FORCE_INLINE void mcd212_device::set_display_parameters(uint8_t value)
{
	m_ddr[Path] &= 0xf0ff;
	m_ddr[Path] |= (value & 0x0f) << 8;
	m_dcr[Path] &= 0xf7ff;
	m_dcr[Path] |= (value & 0x10) << 7;
}

void mcd212_device::update_screen_timing()
{
	// One MAME raster row represents one half-line.  Interlaced fields
	// therefore retain the documented trailing half-line, while single-field
	// output uses the integral totals from MCD212 tables 5-6 and 5-7.
	const auto timing = mcd212_video::make_timing_profile(
		BIT(m_dcr[0], DCR_CF_BIT),
		BIT(m_dcr[0], DCR_FD_BIT),
		BIT(m_dcr[0], DCR_SM_BIT),
		BIT(m_csrw[0], CSR1W_ST_BIT));

	m_active_start = timing.active_start_lines;
	m_active_height = timing.active_lines;
	m_total_height = timing.total_lines;
	m_ica_lines = timing.blank_lines;

	const rectangle visarea(
		0, 767,
		m_active_start * 2,
		(m_active_start + m_active_height) * 2 - 1);

	const attotime frame_period = attotime::from_ticks(
		uint64_t(timing.horizontal_total) * timing.total_half_lines, clock());

	screen().configure(
		timing.horizontal_total,
		timing.total_half_lines,
		visarea,
		frame_period);

	// Timing changes alter the positions of the ICA and DCA windows.
	if (m_dca_timer)
	{
		const int field_offset = timing.field_halfline_offset(BIT(m_csrr[0], CSR1R_PA_BIT));
		m_dca_timer->adjust(screen().time_until_pos(m_active_start * 2 + field_offset, 784));
	}

	if (m_ica_timer)
		m_ica_timer->adjust(screen().time_until_pos(0, 0));
}

int mcd212_device::get_screen_width()
{
	int width = 768;
	if (!BIT(m_dcr[0], DCR_CF_BIT) || BIT(m_csrw[0], CSR1W_ST_BIT))
		width = 720;
	return width;
}

int mcd212_device::get_border_width()
{
	int width = 0;
	if (!BIT(m_dcr[0], DCR_CF_BIT) || BIT(m_csrw[0], CSR1W_ST_BIT))
		width = 24;
	return width;
}

uint32_t mcd212_device::get_backdrop_plane()
{
	if (BIT(m_image_coding_method, ICM_EV_BIT))
		return 0; // External Video Background. Default to Black since there is no DVC.
	else
		return s_4bpp_color[m_backdrop_color];
}

uint32_t mcd212_device::dyuv_to_rgb(uint32_t yuv) const
{
	const uint8_t y = mcd212_video::yuv_y(yuv);
	const uint8_t u = mcd212_video::yuv_u(yuv);
	const uint8_t v = mcd212_video::yuv_v(yuv);
	const uint32_t *const limit_rgb = m_dyuv_limit_lut + y + 0x100;
	return
		(limit_rgb[m_dyuv_v_to_r[v]] << 16) |
		(limit_rgb[m_dyuv_u_to_g[u] + m_dyuv_v_to_g[v]] << 8) |
		limit_rgb[m_dyuv_u_to_b[u]];
}

void mcd212_device::update_interrupt_state()
{
	const bool asserted = mcd212_video::interrupt_line_asserted(
		m_csrr[1], BIT(m_csrw[0], CSR1W_DI1_BIT), BIT(m_csrw[1], CSR1W_DI1_BIT));
	m_int_callback(asserted ? ASSERT_LINE : CLEAR_LINE);
}

template <int Path>
void mcd212_device::process_ica()
{
	uint16_t *ica = Path ? m_planeb.target() : m_planea.target();
	const int max_to_process = m_ica_lines * 120;
	// LCT depends on the current frame parity
	uint32_t addr = mcd212_video::ica_pointer_word_offset(BIT(m_csrr[0], CSR1R_PA_BIT));

	for (int i = 0; i < max_to_process; i++)
	{
		uint32_t cmd = ica[addr++] << 16;
		cmd |= ica[addr++];
		switch ((cmd & 0xff000000) >> 24)
		{
			case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: // STOP
			case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: STOP\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				return;
			case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: // NOP
			case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: NOP\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				break;
			case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: // RELOAD DCP
			case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: RELOAD DCP: %06x\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path, cmd & 0x003fffff);
				set_dcp<Path>(cmd & 0x003ffffc);
				break;
			case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: // RELOAD DCP and STOP
			case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: RELOAD DCP and STOP: %06x\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path, cmd & 0x003fffff);
				set_dcp<Path>(cmd & 0x003ffffc);
				return;
			case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: // RELOAD VSR (ICA)
			case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: RELOAD VSR: %06x\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path, cmd & 0x003fffff);
				addr = (cmd & 0x0007ffff) / 2;
				break;
			case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: // RELOAD VSR and STOP
			case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: RELOAD VSR and STOP: VSR = %05x\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path, cmd & 0x003fffff);
				set_vsr<Path>(cmd & 0x003fffff);
				return;
			case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67: // INTERRUPT
			case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: INTERRUPT\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				m_csrr[1] |= 1 << (2 - Path);
				update_interrupt_state();
				break;
			case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f: // RELOAD DISPLAY PARAMETERS
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: RELOAD DISPLAY PARAMETERS\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				set_display_parameters<Path>(cmd & 0x1f);
				break;
			default:
				LOGMASKED(LOG_ICA, "%08x: %08x: ICA %d: SET REGISTER %02x = %06x\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path, cmd >> 24, cmd & 0x00ffffff);
				set_register<Path>(cmd >> 24, cmd & 0x00ffffff);
				break;
		}
	}
}

template <int Path>
void mcd212_device::process_dca()
{
	uint16_t *dca = Path ? m_planeb.target() : m_planea.target();
	uint32_t addr = (m_dca[Path] & 0x0007ffff) / 2;
	uint32_t cmd = 0;
	uint32_t count = 0;
	const uint32_t max = mcd212_video::dca_bytes_per_line(BIT(m_dcr[0], DCR_CF_BIT));
	bool addr_changed = false;
	bool processing = true;

	LOGMASKED(LOG_DCA, "Scanline %d: Processing DCA %d\n", screen().vpos(), Path);

	while (processing && count < max)
	{
		cmd = dca[addr++] << 16;
		cmd |= dca[addr++];
		count += 4;
		switch ((cmd & 0xff000000) >> 24)
		{
			case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: // STOP
			case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: STOP\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				processing = false;
				break;
			case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: // NOP
			case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: NOP\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				break;
			case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: // RELOAD DCP
			case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: RELOAD DCP (NOP)\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				break;
			case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: // RELOAD DCP and STOP
			case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: RELOAD DCP and STOP\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				set_dcp<Path>(cmd & 0x003ffffc);
				m_dca[Path] = cmd & 0x0007fffc;
				return;
			case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: // RELOAD VSR
			case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: RELOAD VSR: %06x\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path, cmd & 0x001fffff);
				set_vsr<Path>(cmd & 0x003fffff);
				break;
			case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: // RELOAD VSR and STOP
			case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: RELOAD VSR and STOP: %06x\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path, cmd & 0x001fffff);
				set_vsr<Path>(cmd & 0x003fffff);
				processing = false;
				break;
			case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67: // INTERRUPT
			case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: INTERRUPT\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				m_csrr[1] |= 1 << (2 - Path);
				update_interrupt_state();
				break;
			case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f: // RELOAD DISPLAY PARAMETERS
				LOGMASKED(LOG_DCA, "%08x: %08x: DCA %d: RELOAD DISPLAY PARAMETERS\n", (addr - 2) * 2 + Path * 0x200000, cmd, Path);
				set_display_parameters<Path>(cmd & 0x1f);
				break;
			default:
				set_register<Path>(cmd >> 24, cmd & 0x00ffffff);
				break;
		}
	}

	if (!addr_changed)
	{
		addr += (max - count) >> 1;
	}

	m_dca[Path] = addr * 2;
}

template <int Path>
static inline uint8_t BYTE_TO_CLUT(int icm, uint8_t byte, bool clut_select)
{
	switch (icm)
	{
	case 1:
		return byte;
	case 3:
		return (Path ? 0x80 : 0) | (byte & 0x7f);
	case 4:
		if (Path == 0)
		{
			return (clut_select ? 0x80 : 0) | (byte & 0x7f);
		}
		break;
	case 11:
		return (Path ? 0x80 : 0) | (byte & 0x0f);
	default:
		break;
	}
	return 0;
}

template <int Path>
inline ATTR_FORCE_INLINE uint8_t mcd212_device::get_transparency_control()
{
	return (m_transparency_control >> (Path ? 8 : 0)) & 0x0f;
}

template <int Path>
inline ATTR_FORCE_INLINE uint8_t mcd212_device::get_icm()
{
	const uint32_t mask = Path ? ICM_MODE2 : ICM_MODE1;
	const uint32_t shift = Path ? ICM_MODE2_SHIFT : ICM_MODE1_SHIFT;
	return (m_image_coding_method & mask) >> shift;
}

template <int Path>
inline ATTR_FORCE_INLINE bool mcd212_device::get_mosaic_enable()
{
	return (m_ddr[Path] & DDR_FT) == DDR_FT_MOSAIC;
}

template <int Path>
inline ATTR_FORCE_INLINE uint8_t mcd212_device::get_mosaic_factor()
{
	return 1 << (((m_ddr[Path] & DDR_MT) >> DDR_MT_SHIFT) + 1);
}

template <int Path>
void mcd212_device::process_vsr(int active_line, uint32_t *pixels, bool *transparent)
{
	const uint8_t *data = reinterpret_cast<uint8_t *>(Path ? m_planeb.target() : m_planea.target());
	const uint8_t *data2 = reinterpret_cast<uint8_t*>(!Path ? m_planeb.target() : m_planea.target());
	const uint8_t icm = get_icm<Path>();
	const uint8_t tp_ctrl = get_transparency_control<Path>();
	const int width = get_screen_width();

	if constexpr (Path == 0)
	{
		if (active_line >= 0 && active_line < int(std::size(m_qhy_dyuv_valid)))
			m_qhy_dyuv_valid[active_line] = false;
	}

	uint32_t vsr = get_vsr<Path>();
	uint32_t vsr2 = get_vsr<!Path>();
	const bool qhy_base =
		Path == 0 && icm == ICM_DYUV && get_icm<1>() == ICM_QHY;

	if ((tp_ctrl == TCR_ALWAYS && !qhy_base) || !icm || !vsr)
	{
		std::fill_n(pixels, get_screen_width(), s_4bpp_color[0]);
		std::fill_n(transparent, get_screen_width(), (tp_ctrl == TCR_ALWAYS));
		return;
	}

	const uint32_t decodingMode = m_ddr[Path] & DDR_FT;

	const uint8_t mosaic_enable = get_mosaic_enable<Path>();
	const uint8_t mosaic_factor = get_mosaic_factor<Path>();

	const uint32_t dyuv_abs_start = m_dyuv_abs_start[Path];
	uint8_t y = (dyuv_abs_start >> 16) & 0x000000ff;
	uint8_t u = (dyuv_abs_start >>  8) & 0x000000ff;
	uint8_t v = (dyuv_abs_start >>  0) & 0x000000ff;

	const uint32_t mask_bits = (~m_mask_color[Path]) & 0x00fcfcfc;
	const uint32_t tp_color_match = m_transparent_color[Path] & mask_bits;
	const uint8_t tp_ctrl_type = tp_ctrl & 0x07;

	const bool use_rgb_tp_bit = (tp_ctrl_type == TCR_RGB);
	const bool tp_check_parity = !BIT(tp_ctrl, 3);
	const bool tp_always = ((tp_ctrl_type == TCR_ALWAYS) && tp_check_parity);
	const int matte_flag_index = BIT(~tp_ctrl_type, 0);
	const bool *const matte_flags = m_matte_flag[matte_flag_index];
	const bool use_matte_flag = (tp_ctrl_type >= TCR_MF0 && tp_ctrl_type <= TCR_MF1_KEY1);
	const bool is_dyuv_rgb = (icm == ICM_DYUV) || (icm == ICM_QHY) || ((icm == ICM_RGB555) && (Path == 1)); // DYUV, QHY, and RGB do not have access to color key.
	const bool use_color_key = !is_dyuv_rgb && ((tp_ctrl_type == TCR_KEY) || (tp_ctrl_type == TCR_MF0_KEY1) || (tp_ctrl_type == TCR_MF1_KEY1));

	LOGMASKED(LOG_VSR, "Scanline %d: VSR Path %d, ICM (%02x), VSR (%08x)\n", screen().vpos(), Path, icm, vsr);

	if constexpr (Path == 1)
	{
		if (icm == ICM_QHY)
		{
			uint8_t qhy_codes[768];
			std::fill_n(qhy_codes, width, 0);
			const auto decoded = mcd212_video::decode_qhy_line(
				[data, vsr](std::size_t offset)
				{
					return data[((vsr + offset) & 0x0007ffff) ^ 1];
				},
				width + 2, qhy_codes, width);
			vsr += decoded.bytes;
			set_vsr<Path>(vsr);

			if (!decoded.valid)
				LOGMASKED(LOG_UNKNOWNS, "Scanline %d: Malformed QHY line (%d pixels, %d bytes)\n", screen().vpos(), int(decoded.pixels), int(decoded.bytes));

			const bool row_valid =
				active_line >= 0 &&
				active_line < m_active_height &&
				active_line < int(std::size(m_qhy_dyuv_valid)) &&
				m_qhy_dyuv_valid[active_line];
			const int next_line = std::min(active_line + 1, m_active_height - 1);
			const bool next_valid =
				next_line >= 0 &&
				next_line < int(std::size(m_qhy_dyuv_valid)) &&
				m_qhy_dyuv_valid[next_line];
			const bool vertical_half =
				BIT(m_dcr[0], DCR_SM_BIT) && !BIT(m_csrr[0], CSR1R_PA_BIT);
			const int normal_width = width / 2;

			for (int x = 0; x < width; ++x)
			{
				const int column = x / 2;
				const int next_column = std::min(column + 1, normal_width - 1);
				const uint32_t fallback = mcd212_video::pack_yuv(16, 128, 128);
				const uint32_t p00 = row_valid ? m_qhy_dyuv_field[active_line][column] : fallback;
				const uint32_t p10 = row_valid ? m_qhy_dyuv_field[active_line][next_column] : p00;
				const uint32_t p01 = next_valid ? m_qhy_dyuv_field[next_line][column] : p00;
				const uint32_t p11 = next_valid ? m_qhy_dyuv_field[next_line][next_column] : p10;
				const uint32_t filtered = mcd212_video::interpolate_yuv(
					p00, p10, p01, p11, BIT(x, 0), vertical_half);
				pixels[x] = mcd212_video::add_qhy_level(
					dyuv_to_rgb(filtered), m_qhy_levels[qhy_codes[x]]);
				transparent[x] =
					tp_always ||
					(use_matte_flag && (matte_flags[x] == tp_check_parity));
			}
			return;
		}
	}

	if constexpr (Path == 0)
	{
		if (icm == ICM_DYUV && active_line >= 0 && active_line < int(std::size(m_qhy_dyuv_valid)))
			m_qhy_dyuv_valid[active_line] = true;
	}

	for (uint32_t x = 0; x < width; )
	{
		const uint8_t byte = data[(vsr++ & 0x0007ffff) ^ 1];
		uint32_t color0 = 0;
		uint32_t color1 = 0;
		bool rgb_tp_bit = false;
		if (icm == ICM_DYUV)
		{
			const uint8_t byte1 = data[(vsr++ & 0x0007ffff) ^ 1];
			const uint8_t y2 = y + m_delta_y_lut[byte];
			y = y2 + m_delta_y_lut[byte1];
			u += m_delta_uv_lut[byte];
			v += m_delta_uv_lut[byte1];

			const uint32_t *limit_rgb = m_dyuv_limit_lut + y2 + 0x100;
			const uint32_t *limit_rgb2 = m_dyuv_limit_lut + y + 0x100;

			color0 = (limit_rgb[m_dyuv_v_to_r[v]] << 16) | (limit_rgb[m_dyuv_u_to_g[u] + m_dyuv_v_to_g[v]] << 8) | limit_rgb[m_dyuv_u_to_b[u]];

			const uint8_t byte2 = data[(vsr & 0x0007ffff) ^ 1]; // Peek ahead, for calculating the half-step.
			const uint8_t byte3 = data[((vsr + 1) & 0x0007ffff) ^ 1];
			const uint8_t u8 = u + m_delta_uv_lut[byte2];
			const uint8_t v8 = v + m_delta_uv_lut[byte3];
			const uint8_t u6 = (u >> 1) + (u8 >> 1) + (u & u8 & 1);
			const uint8_t v6 = (v >> 1) + (v8 >> 1) + (v & v8 & 1);

			color1 = (limit_rgb2[m_dyuv_v_to_r[v6]] << 16) | (limit_rgb2[m_dyuv_u_to_g[u6] + m_dyuv_v_to_g[v6]] << 8) | limit_rgb2[m_dyuv_u_to_b[u6]];

			if constexpr (Path == 0)
			{
				if (active_line >= 0 && active_line < int(std::size(m_qhy_dyuv_valid)))
				{
					m_qhy_dyuv_field[active_line][x / 2] = mcd212_video::pack_yuv(y2, u, v);
					m_qhy_dyuv_field[active_line][x / 2 + 1] = mcd212_video::pack_yuv(y, u6, v6);
				}
			}
			pixels[x] = color0;
			pixels[x + 1] = color0;
			pixels[x + 2] = color1;
			pixels[x + 3] = color1;
			transparent[x    ] = tp_always || (use_matte_flag && (matte_flags[x    ] == tp_check_parity));
			transparent[x + 1] = tp_always || (use_matte_flag && (matte_flags[x + 1] == tp_check_parity));
			transparent[x + 2] = tp_always || (use_matte_flag && (matte_flags[x + 2] == tp_check_parity));
			transparent[x + 3] = tp_always || (use_matte_flag && (matte_flags[x + 3] == tp_check_parity));
			x += 4;
		}
		else
		{
			bool clut_select = BIT(m_image_coding_method, ICM_CS_BIT);
			if (icm == ICM_RGB555 && Path == 1)
			{
				const uint8_t byte1 = data2[(vsr2++ & 0x0007ffff) ^ 1];
				const uint8_t blue = (byte & 0b11111) << 3;
				const uint8_t green = ((byte & 0b11100000) >> 2) + ((byte1 & 0b11) << 6);
				const uint8_t red = (byte1 & 0b01111100) << 1;
				rgb_tp_bit = (use_rgb_tp_bit && (BIT(byte1,7) == tp_check_parity));
				color1 = color0 = (uint32_t(red) << 16) | (uint32_t(green) << 8) | blue;
			}
			else if (icm == ICM_CLUT4)
			{
				const uint8_t mask = (decodingMode == DDR_FT_RLE) ? 0x7 : 0xf;
				color0 = m_clut[BYTE_TO_CLUT<Path>(icm, mask & (byte >> 4), clut_select)];
				color1 = m_clut[BYTE_TO_CLUT<Path>(icm, mask & byte, clut_select)];
			}
			else
			{
				color1 = color0 = m_clut[BYTE_TO_CLUT<Path>(icm, byte, clut_select)];
			}

			int length_m = mosaic_enable ? (mosaic_factor * 2) : 2;
			if (decodingMode == DDR_FT_RLE)
			{
				const uint16_t length = (byte & 0x80) ? data[((vsr++) & 0x0007ffff) ^ 1] : 1;
				length_m = length ? (length * 2) : width;
			}

			const bool color_match0 = ((mask_bits & color0) == tp_color_match) == tp_check_parity;
			const bool color_match1 = ((mask_bits & color1) == tp_color_match) == tp_check_parity;
			const int end = std::min<int>(width, x + length_m);
			for (int rl_index = x; rl_index < end; rl_index += 2)
			{
				pixels[rl_index    ] = color0;
				pixels[rl_index + 1] = color1;
				transparent[rl_index    ] = tp_always || rgb_tp_bit || (use_color_key && color_match0) || (use_matte_flag && (matte_flags[rl_index    ] == tp_check_parity));
				transparent[rl_index + 1] = tp_always || rgb_tp_bit || (use_color_key && color_match1) || (use_matte_flag && (matte_flags[rl_index + 1] == tp_check_parity));
			}
			x = end;
		}
	}
	set_vsr<Path>(vsr);
	set_vsr<!Path>(vsr2);
}

const uint32_t mcd212_device::s_4bpp_color[16] =
{
	0xff101010, 0xff10107a, 0xff107a10, 0xff107a7a, 0xff7a1010, 0xff7a107a, 0xff7a7a10, 0xff7a7a7a,
	0xff101010, 0xff1010e6, 0xff10e610, 0xff10e6e6, 0xffe61010, 0xffe610e6, 0xffe6e610, 0xffe6e6e6
};

template <bool MosaicA, bool MosaicB, bool OrderAB>
void mcd212_device::mix_lines(uint32_t *plane_a, bool *transparent_a, uint32_t *plane_b, bool *transparent_b, uint32_t *out, bool *external_video)
{
	const uint8_t icmA = get_icm<0>();
	const uint8_t icmB = get_icm<1>();
	uint16_t mosaic_count_a = (m_mosaic_hold[0] & 0x0000ff) << 1;
	uint16_t mosaic_count_b = (m_mosaic_hold[1] & 0x0000ff) << 1;
	const int width = get_screen_width();
	const int border_width = get_border_width();

	uint8_t *weight_a = &m_weight_factor[0][0];
	uint8_t *weight_b = &m_weight_factor[1][0];

	// Console Verified. CLUT4 pixels are drawn in pairs during VSR. So the mosaic here is halved.
	if (icmA == ICM_CLUT4)
		mosaic_count_a >>= 1;
	if (icmB == ICM_CLUT4)
		mosaic_count_b >>= 1;

	// If PAL and 'Standard' bit set, insert a 24px border on the left/right
	if (border_width)
	{
		std::fill_n(out, border_width, s_4bpp_color[0]);
		out += border_width;
	}

	for (int x = 0; x < width; x++)
	{
		if (transparent_a[x] && transparent_b[x])
		{
			out[x] = get_backdrop_plane();
			external_video[x + border_width] = mcd212_video::external_video_eligible(
				BIT(m_image_coding_method, ICM_EV_BIT), true, true);
			continue;
		}
		const std::size_t source_a = MosaicA
			? mcd212_video::mosaic_source_x(std::size_t(x), mosaic_count_a)
			: std::size_t(x);
		const std::size_t source_b = MosaicB
			? mcd212_video::mosaic_source_x(std::size_t(x), mosaic_count_b)
			: std::size_t(x);
		uint32_t plane_a_cur = plane_a[source_a];
		uint32_t plane_b_cur = plane_b[source_b];

		if (transparent_a[x])
		{
			plane_a_cur = 0;
		}
		else if (OrderAB && (m_transparency_control & TCR_DISABLE_MX))
		{
			plane_b_cur = 0;
		}

		if (transparent_b[x])
		{
			plane_b_cur = 0;
		}
		else if (!OrderAB && (m_transparency_control & TCR_DISABLE_MX))
		{
			plane_a_cur = 0;
		}

		const int32_t plane_a_r = 0xff & (plane_a_cur >> 16);
		const int32_t plane_a_g = 0xff & (plane_a_cur >> 8);
		const int32_t plane_a_b = 0xff & plane_a_cur;
		const int32_t plane_b_r = 0xff & (plane_b_cur >> 16);
		const int32_t plane_b_g = 0xff & (plane_b_cur >> 8);
		const int32_t plane_b_b = 0xff & plane_b_cur;

		const int32_t weighted_a_r = std::clamp((std::clamp(plane_a_r - 16, 0, 255) * weight_a[x]) >> 6, 0, 255);
		const int32_t weighted_a_g = std::clamp((std::clamp(plane_a_g - 16, 0, 255) * weight_a[x]) >> 6, 0, 255);
		const int32_t weighted_a_b = std::clamp((std::clamp(plane_a_b - 16, 0, 255) * weight_a[x]) >> 6, 0, 255);

		const int32_t weighted_b_r = std::clamp((std::clamp(plane_b_r - 16, 0, 255) * weight_b[x]) >> 6, 0, 255);
		const int32_t weighted_b_g = std::clamp((std::clamp(plane_b_g - 16, 0, 255) * weight_b[x]) >> 6, 0, 255);
		const int32_t weighted_b_b = std::clamp((std::clamp(plane_b_b - 16, 0, 255) * weight_b[x]) >> 6, 0, 255);

		const uint8_t out_r = std::clamp(weighted_a_r + weighted_b_r + 16, 0, 255);
		const uint8_t out_g = std::clamp(weighted_a_g + weighted_b_g + 16, 0, 255);
		const uint8_t out_b = std::clamp(weighted_a_b + weighted_b_b + 16, 0, 255);
		out[x] = 0xff000000 | (out_r << 16) | (out_g << 8) | out_b;
	}

	if (border_width)
	{
		std::fill_n(&out[width], border_width, s_4bpp_color[0]);
	}
}

void mcd212_device::draw_cursor(uint32_t *scanline, bool *external_video)
{
	if (!(m_cursor_control & CURCNT_EN))
		return; // Cursor is Disabled

	uint8_t color_index = m_cursor_control & CURCNT_COLOR;
	if (m_blink_active)
	{
		const bool invert = BIT(m_cursor_control, CURCNT_BLKC_SHIFT);
		if (!invert)
			return; // Normal Blink
		else
			color_index = color_index ^ 0x7; // Inverted Color Blink. MCD212 Section 7.5
	}

	const uint16_t cursor_x = m_cursor_position & 0x3ff;
	const uint16_t cursor_y = ((m_cursor_position >> 12) & 0x3ff) + m_active_start;
	const int32_t y = screen().vpos() / 2 - cursor_y;
	const int width = get_screen_width();

	if ((0 <= y) && (y < 16))
	{
		const uint32_t color = s_4bpp_color[color_index];
		const uint8_t resolution = (m_cursor_control & CURCNT_CUW) ? 1 : 2;
		for (int x = 0; x < 16; x++)
		{
			if (BIT(m_cursor_pattern[y], 15 - x))
			{
				for (uint32_t j = 0; j < resolution; j++)
				{
					const uint32_t index = cursor_x + x * resolution + j;
					if (index < width)
					{
						scanline[index] = color;
						external_video[index] = false;
					}
				}
			}
		}
	}
}

void mcd212_device::map(address_map &map)
{
	map(0x00, 0x01).w(FUNC(mcd212_device::csr2_w));
	map(0x01, 0x01).r(FUNC(mcd212_device::csr2_r));
	map(0x02, 0x03).rw(FUNC(mcd212_device::dcr2_r), FUNC(mcd212_device::dcr2_w));
	map(0x04, 0x05).rw(FUNC(mcd212_device::vsr2_r), FUNC(mcd212_device::vsr2_w));
	map(0x08, 0x09).rw(FUNC(mcd212_device::ddr2_r), FUNC(mcd212_device::ddr2_w));
	map(0x0a, 0x0b).rw(FUNC(mcd212_device::dca2_r), FUNC(mcd212_device::dca2_w));

	map(0x10, 0x11).w(FUNC(mcd212_device::csr1_w));
	map(0x11, 0x11).r(FUNC(mcd212_device::csr1_r));
	map(0x12, 0x13).rw(FUNC(mcd212_device::dcr1_r), FUNC(mcd212_device::dcr1_w));
	map(0x14, 0x15).rw(FUNC(mcd212_device::vsr1_r), FUNC(mcd212_device::vsr1_w));
	map(0x18, 0x19).rw(FUNC(mcd212_device::ddr1_r), FUNC(mcd212_device::ddr1_w));
	map(0x1a, 0x1b).rw(FUNC(mcd212_device::dca1_r), FUNC(mcd212_device::dca1_w));
}

uint8_t mcd212_device::csr1_r()
{
	LOGMASKED(LOG_STATUS, "%s: Control/Status Register 1 Read: %02x\n", machine().describe_context(), m_csrr[0]);
	return m_csrr[0];
}

void mcd212_device::csr1_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Control/Status Register 1 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);
	const uint16_t old_value = m_csrw[0];
	COMBINE_DATA(&m_csrw[0]);
	m_csrw[0] &= CSR1W_WRITE_MASK;

	if (BIT(m_csrw[0], CSR1W_ST_BIT) != BIT(old_value, CSR1W_ST_BIT))
	{
		update_screen_timing();
		update_matte_arrays();
	}
	update_interrupt_state();
}

uint16_t mcd212_device::dcr1_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: Display Command Register 1 Read: %04x & %08x\n", machine().describe_context(), m_dcr[0], mem_mask);
	return m_dcr[0];
}

void mcd212_device::dcr1_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Display Command Register 1 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);

	const uint16_t timing_mask =
		(1U << DCR_CF_BIT) | (1U << DCR_FD_BIT) | (1U << DCR_SM_BIT);
	const uint16_t old_value = m_dcr[0];

	COMBINE_DATA(&m_dcr[0]);
	m_dcr[0] &= DCR1_WRITE_MASK;

	if (((m_dcr[0] ^ old_value) & timing_mask) != 0)
		update_screen_timing();

	if (BIT(m_dcr[0], DCR_CF_BIT) != BIT(old_value, DCR_CF_BIT))
		update_matte_arrays();
}

uint16_t mcd212_device::vsr1_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: Video Start Register 1 Read: %04x & %08x\n", machine().describe_context(), m_vsr[0], mem_mask);
	return m_vsr[0];
}

void mcd212_device::vsr1_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Video Start Register 1 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_vsr[0]);
}

uint16_t mcd212_device::ddr1_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: Display Decoder Register 1 Read: %04x & %08x\n", machine().describe_context(), m_ddr[0], mem_mask);
	return m_ddr[0];
}

void mcd212_device::ddr1_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Display Decoder Register 1 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_ddr[0]);
	m_ddr[0] &= DDR_WRITE_MASK;
}

uint16_t mcd212_device::dca1_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: DCA Pointer 1 Read: %04x & %08x\n", machine().describe_context(), m_dcp[0], mem_mask);
	return m_dcp[0];
}

void mcd212_device::dca1_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: DCA Pointer 1 Write: %04x & %08x\n", machine().describe_context(), data & 0xfffc, mem_mask);
	COMBINE_DATA(&m_dcp[0]);
	m_dcp[0] &= 0xfffc;
}

uint8_t mcd212_device::csr2_r()
{
	if (machine().side_effects_disabled())
	{
		return m_csrr[1];
	}

	const uint8_t data = m_csrr[1];
	LOGMASKED(LOG_STATUS, "%s: Status Register 2: %02x\n", machine().describe_context(), data);

	m_csrr[1] &= ~(CSR2R_IT1 | CSR2R_IT2);
	update_interrupt_state();

	return data;
}

void mcd212_device::csr2_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Control/Status Register 2 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_csrw[1]);
	m_csrw[1] &= CSR2W_WRITE_MASK;
	update_interrupt_state();
}

uint16_t mcd212_device::dcr2_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: Display Command Register 2 Read: %04x & %08x\n", machine().describe_context(), m_dcr[1], mem_mask);
	return m_dcr[1];
}

void mcd212_device::dcr2_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Display Command Register 2 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_dcr[1]);
	m_dcr[1] &= DCR2_WRITE_MASK;
}

uint16_t mcd212_device::vsr2_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: Video Start Register 2 Read: %04x & %08x\n", machine().describe_context(), m_vsr[1], mem_mask);
	return m_vsr[1];
}

void mcd212_device::vsr2_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Video Start Register 2 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_vsr[1]);
}

uint16_t mcd212_device::ddr2_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: Display Decoder Register 2 Read: %04x & %08x\n", machine().describe_context(), m_ddr[1], mem_mask);
	return m_ddr[1];
}

void mcd212_device::ddr2_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: Display Decoder Register 2 Write: %04x & %08x\n", machine().describe_context(), data, mem_mask);
	COMBINE_DATA(&m_ddr[1]);
	m_ddr[1] &= DDR_WRITE_MASK;
}

uint16_t mcd212_device::dca2_r(offs_t offset, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_READS, "%s: DCA Pointer 2 Read: %04x & %08x\n", machine().describe_context(), m_dcp[1], mem_mask);
	return m_dcp[1];
}

void mcd212_device::dca2_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	LOGMASKED(LOG_MAIN_REG_WRITES, "%s: DCA Pointer 2 Write: %04x & %08x\n", machine().describe_context(), data & 0xfffc, mem_mask);
	COMBINE_DATA(&m_dcp[1]);
	m_dcp[1] &= 0xfffc;
}

TIMER_CALLBACK_MEMBER(mcd212_device::ica_tick)
{
	m_csrr[0] &= ~CSR1R_DA;

	// Process ICA
	if (BIT(m_dcr[0], DCR_ICA_BIT))
		process_ica<0>();
	if (BIT(m_dcr[1], DCR_ICA_BIT))
		process_ica<1>();

	if (BIT(m_dcr[0], DCR_DCA_BIT))
		m_dca[0] = get_dcp<0>();
	if (BIT(m_dcr[1], DCR_DCA_BIT))
		m_dca[1] = get_dcp<1>();

	m_ica_timer->adjust(screen().time_until_pos(0, 0));

	// Cursor Blink
	m_blink_time += 5 + BIT(m_dcr[0], DCR_FD_BIT); // FD bit * 8... Page 4-3 MCD
	// Adjust the blink time once per frame
	if (!m_blink_active && (m_blink_time >= ((m_cursor_control & CURCNT_CON) >> CURCNT_CON_SHIFT) * 60))
	{
		m_blink_active = true;
		m_blink_time = 0;
	}
	// If blink off time is 0, immediately turn back on.
	if (m_blink_active && (m_blink_time >= ((m_cursor_control & CURCNT_COF) >> CURCNT_COF_SHIFT) * 60))
	{
		m_blink_active = false;
		m_blink_time = 0;
	}
}

TIMER_CALLBACK_MEMBER(mcd212_device::dca_tick)
{
	// Process DCA
	if (BIT(m_dcr[0], DCR_DCA_BIT))
		process_dca<0>();
	if (BIT(m_dcr[1], DCR_DCA_BIT))
		process_dca<1>();

	const int scanline = screen().vpos() / 2;
	const bool interlace = BIT(m_dcr[0], DCR_SM_BIT);
	const bool odd_field = BIT(m_csrr[0], CSR1R_PA_BIT);
	if (scanline == m_active_start + m_active_height - 1)
	{
		// The following field has the opposite PA and therefore the opposite
		// half-line phase.
		const int next_field_offset = interlace && odd_field ? 1 : 0;
		m_dca_timer->adjust(screen().time_until_pos(m_active_start * 2 + next_field_offset, 784));
	}
	else
	{
		const int field_offset = interlace && !odd_field ? 1 : 0;
		m_dca_timer->adjust(screen().time_until_pos((scanline + 1) * 2 + field_offset, 784));
	}
}

uint32_t mcd212_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint32_t plane_a[768];
	uint32_t plane_b[768];
	bool transparent_a[768];
	bool transparent_b[768];

	(void)screen;

	const int min_x = std::max(cliprect.min_x, 0);
	const int max_x = std::min(cliprect.max_x, 767);

	auto copy_cached_row = [&](int raster_y, int scanline)
	{
		if (m_scanline_cache_scanline != scanline || min_x > max_x)
			return;

		uint32_t *const dest = &bitmap.pix(raster_y);
		std::copy_n(
				&m_scanline_cache[raster_y & 1][min_x],
				max_x - min_x + 1,
				dest + min_x);
	};

	for (int raster_y = cliprect.min_y; raster_y <= cliprect.max_y; raster_y++)
	{
		const int scanline = raster_y / 2;

		// Each MCD212 source line produces two MAME raster rows.  Process the
		// source line only on the first row; the second scanline callback uses
		// the cached sibling output without advancing VSR/display state again.
		if (raster_y & 1)
		{
			copy_cached_row(raster_y, scanline);
			continue;
		}

		std::fill_n(&m_scanline_cache[0][0], 2 * 768, 0);
		std::fill_n(&m_external_video_cache[0][0], 2 * 768, false);

		// The final raster row represents the half-line at the end of the field.
		if (scanline >= m_total_height)
			return 0;

		// Process VSR and mix if we're in the visible region
		if (scanline >= m_active_start && scanline < m_active_start + m_active_height)
		{
			uint32_t *const out = m_scanline_cache[BIT(~m_csrr[0], CSR1R_PA_BIT)];
			uint32_t *const out2 = m_scanline_cache[BIT(m_csrr[0], CSR1R_PA_BIT)];
			bool *const external_video = m_external_video_cache[BIT(~m_csrr[0], CSR1R_PA_BIT)];
			bool *const external_video2 = m_external_video_cache[BIT(m_csrr[0], CSR1R_PA_BIT)];
			const int active_line = scanline - m_active_start;

			m_csrr[0] |= CSR1R_DA;

			{
				process_vsr<0>(active_line, plane_a, transparent_a);
				process_vsr<1>(active_line, plane_b, transparent_b);

				const uint8_t mosaic_enable_a = (m_mosaic_hold[0] & 0x800000) >> 23;
				const uint8_t mosaic_enable_b = (m_mosaic_hold[1] & 0x800000) >> 22;
				const uint8_t mixing_mode = (mosaic_enable_a | mosaic_enable_b) | (BIT(m_plane_order, 0) << 2);
				switch (mixing_mode & 7)
				{
					case 0: // No Mosaic A/B, A->B->Backdrop plane ordering
						mix_lines<false, false, true>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
					case 1: // Mosaic A, No Mosaic B, A->B->Backdrop plane ordering
						mix_lines<true, false, true>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
					case 2: // No Mosaic A, Mosaic B, A->B->Backdrop plane ordering
						mix_lines<false, true, true>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
					case 3: // Mosaic A/B, A->B->Backdrop plane ordering
						mix_lines<true, true, true>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
					case 4: // No Mosaic A/B, B->A->Backdrop plane ordering
						mix_lines<false, false, false>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
					case 5: // Mosaic A, No Mosaic B, B->A->Backdrop plane ordering
						mix_lines<true, false, false>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
					case 6: // No Mosaic A, Mosaic B, B->A->Backdrop plane ordering
						mix_lines<false, true, false>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
					case 7: // Mosaic A/B, B->A->Backdrop plane ordering
						mix_lines<true, true, false>(plane_a, transparent_a, plane_b, transparent_b, out, external_video);
						break;
				}

				draw_cursor(out, external_video);
			}

			if (BIT(m_dcr[0], DCR_SM_BIT))
			{
				// Interlace Output: keep the previous-field pixel row
				// paired with the eligibility mask from that same row.
				std::copy_n(m_external_video_field[scanline], 768, external_video2);
				std::copy_n(external_video, 768, m_external_video_field[scanline]);
				std::copy_n(m_interlace_field[scanline], 768, out2);
				std::copy_n(out, 768, m_interlace_field[scanline]);
			}
			else
			{
				// Single Field Output duplicates pixels and their
				// external-video eligibility.
				std::copy_n(external_video, 768, external_video2);
				std::copy_n(out, 768, out2);
			}
		}

		// Toggle frame parity at the end of the visible frame (even in non-interlaced mode).
		if (scanline == (m_total_height - 1))
		{
			m_csrr[0] ^= CSR1R_PA;
		}

		m_scanline_cache_scanline = scanline;
		copy_cached_row(raster_y, scanline);
	}

	return 0;
}

template int mcd212_device::ram_dtack_cycle_count<0>();
template int mcd212_device::ram_dtack_cycle_count<1>();

template <int Path>
int mcd212_device::ram_dtack_cycle_count()
{
	// Per MCD-212 documentation, it takes 4 CLKs (2 SCC68070 clocks) for a VRAM access during the System timing slot.

	// No contending for Ch.1/Ch.2 timing slots if display is disabled
	if (!BIT(m_dcr[0], DCR_DE_BIT))
		return 2;

	// No contending for Ch.1/Ch.2 timing slots if a relevant Path is disabled
	if (!BIT(m_dcr[Path], DCR_ICA_BIT))
		return 2;

	const int x = screen().hpos();
	const int y = screen().vpos() / 2;
	const bool x_outside_active_display = (x >= 408);

	// No contending for Ch.1/Ch.2 timing slots during the final 8-pixel area on all lines
	if (x >= 472)
		return 2;

	// No contention in the free-run area during either part of vertical blanking.
	if ((y < m_active_start || y >= m_active_start + m_active_height) && x_outside_active_display)
		return 2;

	// No contending for Ch.1/Ch.2 timing slots during the free-run area of DCA lines if DCA is disabled
	if (!BIT(m_dcr[Path], DCR_DCA_BIT) && x_outside_active_display)
		return 2;

	// System access is restricted to the last 5 out of every 16 CLKs.
	const int slot_cycle = int(machine().time().as_ticks(clock()) & 0xf);
	if (slot_cycle >= 11)
		return 2;

	return 2 + std::max((11 - slot_cycle) >> 1, 1);
}

int mcd212_device::rom_dtack_cycle_count()
{
	static const int s_dd_values[4] = { 2, 3, 4, 5 };
	if (!BIT(m_csrw[0], CSR1W_DD_BIT))
		return 7;
	return s_dd_values[(m_csrw[0] & CSR1W_DD2) >> CSR1W_DD2_SHIFT];
}

void mcd212_device::device_reset()
{
	std::fill_n(m_csrr, 2, 0);
	std::fill_n(m_csrw, 2, 0);
	std::fill_n(m_dcr, 2, 0);
	std::fill_n(m_vsr, 2, 0);
	std::fill_n(m_ddr, 2, 0);
	std::fill_n(m_dcp, 2, 0);
	std::fill_n(m_dca, 2, 0);
	std::fill_n(m_clut, 256, 0);
	std::fill_n(m_qhy_levels, 8, 0);
	m_image_coding_method = 0;
	m_transparency_control = 0;
	m_plane_order = 0;
	std::fill_n(m_clut_bank, 2, 0);
	std::fill_n(m_transparent_color, 2, 0);
	std::fill_n(m_mask_color, 2, 0);
	std::fill_n(m_dyuv_abs_start, 2, 0);
	m_cursor_position = 0;
	m_cursor_control = 0;
	std::fill_n(m_cursor_pattern, std::size(m_cursor_pattern), 0);
	std::fill_n(m_matte_control, 8, 0);
	m_backdrop_color = 0;
	std::fill_n(m_mosaic_hold, 2, 0);
	std::fill_n(m_weight_factor[0], std::size(m_weight_factor[0]), 0);
	std::fill_n(m_weight_factor[1], std::size(m_weight_factor[1]), 0);
	std::fill_n(m_matte_flag[0], std::size(m_matte_flag[0]), false);
	std::fill_n(m_matte_flag[1], std::size(m_matte_flag[1]), false);

	m_blink_time = 0;
	m_blink_active = false;
	std::fill_n(&m_interlace_field[0][0], 312 * 768, 0);
	std::fill_n(&m_qhy_dyuv_field[0][0], 280 * 384, 0);
	std::fill_n(m_qhy_dyuv_valid, 280, false);

	m_int_callback(CLEAR_LINE);

	std::fill_n(&m_scanline_cache[0][0], 2 * 768, 0);
	std::fill_n(&m_external_video_cache[0][0], 2 * 768, false);
	std::fill_n(&m_external_video_field[0][0], 312 * 768, false);
	m_scanline_cache_scanline = -1;

	update_screen_timing();
}

//-------------------------------------------------
//  mcd212_device - constructor
//-------------------------------------------------

mcd212_device::mcd212_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, MCD212, tag, owner, clock)
	, device_video_interface(mconfig, *this)
	, m_int_callback(*this)
	, m_planea(*this, finder_base::DUMMY_TAG)
	, m_planeb(*this, finder_base::DUMMY_TAG)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void mcd212_device::device_start()
{
	static const uint8_t s_dyuv_deltas[16] = { 0, 1, 4, 9, 16, 27, 44, 79, 128, 177, 212, 229, 240, 247, 252, 255 };

	for (uint16_t d = 0; d < 0x100; d++)
	{
		m_delta_y_lut[d] = s_dyuv_deltas[d & 15];
		m_delta_uv_lut[d] = s_dyuv_deltas[d >> 4];
	}

	for (uint16_t w = 0; w < 0x300; w++)
	{
		const uint8_t limit = (w < 0x100) ? 0 : (w < 0x200) ? (w - 0x100) : 0xff;
		m_dyuv_limit_lut[w] = limit;
	}

	for (int16_t sw = 0; sw < 0x100; sw++)
	{
		m_dyuv_u_to_b[sw] = (444 * (sw - 128)) / 256;
		m_dyuv_u_to_g[sw] = - (86 * (sw - 128)) / 256;
		m_dyuv_v_to_g[sw] = - (179 * (sw - 128)) / 256;
		m_dyuv_v_to_r[sw] = (351 * (sw - 128)) / 256;
	}

	save_item(NAME(m_csrr));
	save_item(NAME(m_csrw));
	save_item(NAME(m_dcr));
	save_item(NAME(m_vsr));
	save_item(NAME(m_ddr));
	save_item(NAME(m_dcp));
	save_item(NAME(m_dca));
	save_item(NAME(m_clut));
	save_item(NAME(m_qhy_levels));
	save_item(NAME(m_image_coding_method));
	save_item(NAME(m_transparency_control));
	save_item(NAME(m_plane_order));
	save_item(NAME(m_clut_bank));
	save_item(NAME(m_transparent_color));
	save_item(NAME(m_mask_color));
	save_item(NAME(m_dyuv_abs_start));
	save_item(NAME(m_cursor_position));
	save_item(NAME(m_cursor_control));
	save_item(NAME(m_cursor_pattern));
	save_item(NAME(m_matte_control));
	save_item(NAME(m_backdrop_color));
	save_item(NAME(m_mosaic_hold));
	save_item(NAME(m_weight_factor[0]));
	save_item(NAME(m_weight_factor[1]));

	save_item(NAME(m_matte_flag));
	save_item(NAME(m_active_start));
	save_item(NAME(m_active_height));
	save_item(NAME(m_total_height));
	save_item(NAME(m_ica_lines));

	save_item(NAME(m_qhy_dyuv_field));
	save_item(NAME(m_qhy_dyuv_valid));
	save_item(NAME(m_blink_time));
	save_item(NAME(m_blink_active));

	save_item(NAME(m_interlace_field));
	save_item(NAME(m_external_video_field));
	save_item(NAME(m_scanline_cache));
	save_item(NAME(m_external_video_cache));
	save_item(NAME(m_scanline_cache_scanline));

	m_dca_timer = timer_alloc(FUNC(mcd212_device::dca_tick), this);
	m_dca_timer->adjust(attotime::never);

	m_ica_timer = timer_alloc(FUNC(mcd212_device::ica_tick), this);
	m_ica_timer->adjust(attotime::never);
}
