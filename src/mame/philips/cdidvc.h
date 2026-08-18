// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_H
#define MAME_PHILIPS_CDIDVC_H

#pragma once

#include <array>
#include <vector>

struct plm_buffer_t;
struct plm_video_t;
struct plm_audio_t;

class cdi_dvc_device : public device_t, public device_sound_interface
{
public:
	cdi_dvc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto intreq_callback() { return m_intreq_callback.bind(); }
	auto dma_req_callback() { return m_dma_req_callback.bind(); }

	uint16_t read(offs_t offset, uint16_t mem_mask = ~0);
	void write(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	uint16_t rom_r(offs_t offset, uint16_t mem_mask = ~0);

	uint16_t mpeg_ram_r(offs_t offset, uint16_t mem_mask = 0xffff);

	void mpeg_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = 0xffff);

	void rom_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint8_t intack_r();
	void dma_w(uint16_t data);
	void dma_done();

	void video_vblank();
	void video_overlay_scanline(uint32_t *pixels, unsigned pixel_count, int physical_y, int visible_top, int clip_min_x, int clip_max_x, bool const *external_video, unsigned external_count);

protected:
	virtual void device_start() override ATTR_COLD;
	void device_stop() override;
	virtual void device_reset() override ATTR_COLD;
	virtual void sound_stream_update(sound_stream &stream) override;

private:
	// Interrupt and clock handling.
	TIMER_CALLBACK_MEMBER(timer_tick);
	void update_interrupt_state();
	void update_timer();
	uint32_t current_fma_dclk();
	uint32_t current_fmv_dclk();
	void set_fmv_syscr(uint16_t data, uint16_t mem_mask);

	// DMA ingress for the current synchronous transfer model.
	void mpeg_word_w(bool for_fma, uint16_t data, uint16_t mem_mask = 0xffff);

	// MPEG-1 program-stream parsing and timestamp scheduling.
	void mpeg_parser_reset(unsigned target);
	void mpeg_byte_w(unsigned target, uint8_t data);
	void mpeg_begin_payload(unsigned target);
	void mpeg_payload_byte(unsigned target, uint8_t data);
	void mpeg_packet_done(unsigned target);
	void mpeg_scr_byte(unsigned target, uint8_t data);
	void mpeg_timestamp_start(unsigned target, uint8_t data, bool with_dts);
	void mpeg_timestamp_byte(unsigned target, uint8_t data);
	void mpeg_timestamp_commit(unsigned target);
	void mpeg_schedule_packet(unsigned target);
	bool mpeg_stream_selected(unsigned target, uint8_t stream_id) const;

	// MPEG audio decode and MAME sound output.
	void audio_output_reset();
	void audio_output_set_rate(uint32_t rate);
	void audio_decoder_reset();
	void audio_decoder_destroy();
	void audio_decoder_feed(uint8_t data);
	void audio_decoder_pump();
	void audio_decoder_flush();

	// MPEG video decode and MAME video presentation.
	void video_presentation_reset();
	void video_frame_clear();
	void video_latch_frame();
	void video_latch_geometry(bool at_vblank);
	void video_overlay_reset();
	void video_decoder_reset();
	void video_decoder_destroy();
	void video_decoder_feed(uint8_t data);
	void video_decoder_pump();
	void video_decoder_flush();
	void video_picture_event(uint8_t picture_type);
	void video_picture_events_flush();
	uint16_t video_picture_events_pop();

	// Provisional MPEG-RAM compatibility mechanism.
	void mpeg_ram_compat_reset();
	void mpeg_ram_compat_note_vmpeg_write();

	// Current MAME compatibility values. Their hardware attribution is pending;
	// keep them out of emulator-independent specifications until proven.
	static constexpr uint16_t FMA_STATUS_COMPAT_FIXED_BITS = 0x0200;
	static constexpr uint16_t FMA_E03004_COMPAT_READ_VALUE = 0x0007;
	static constexpr uint16_t FMA_E03006_COMPAT_READ_VALUE = 0x0900;
	static constexpr uint16_t FMA_E0300E_COMPAT_READ_VALUE = 0x0042;
	static constexpr uint16_t FMA_E03024_COMPAT_READ_VALUE = 0x0004;
	static constexpr uint16_t FMV_STATUS_COMPAT_INPUT_READY = 0x2000;
	static constexpr uint16_t FMV_TIMER_COMPARE_RESET_COMPAT_VALUE = 55;
	static constexpr uint8_t IDLE_IACK_COMPAT_VECTOR = 0x3c;

	// Device wiring and external resources.
	devcb_write_line m_intreq_callback;
	devcb_write_line m_dma_req_callback;
	optional_region_ptr<uint8_t> m_driver_rom;
	emu_timer *m_interrupt_timer = nullptr;

	// VMPEG clock state.  Both FMA and FMV clocks advance at 45 kHz.
	uint64_t m_dclk_epoch_ticks = 0;
	uint32_t m_fmv_dclk_offset = 0;
	uint16_t m_fma_dclk_latch = 0;

	uint16_t m_vcd_control = 0;

	uint16_t m_fma_command = 0;
	uint16_t m_fma_status = 0;
	uint16_t m_fma_stream = 0;
	uint16_t m_fma_interrupt_vector = 0;
	uint16_t m_fma_interrupt_status = 0;
	uint16_t m_fma_interrupt_enable = 0;

	uint16_t m_fmv_interrupt_enable = 0;
	uint16_t m_fmv_interrupt_status = 0;
	uint16_t m_fmv_timer_compare = FMV_TIMER_COMPARE_RESET_COMPAT_VALUE;
	uint16_t m_fmv_system_command = 0;
	uint16_t m_fmv_video_command = 0;
	uint16_t m_fmv_stream = 0;
	uint16_t m_fmv_interrupt_vector = 0;

	uint16_t m_fmv_transfer_word = 0;
	uint32_t m_fmv_transfer_words = 0;

	// DMA ingress bookkeeping shared by the FMA and FMV paths.
	bool m_dma_active = false;
	bool m_dma_for_fma = false;
	uint32_t m_dma_transfer_words = 0;
	uint16_t m_dma_first_word = 0;
	uint16_t m_dma_last_word = 0;

	// MPEG-1 program-stream parser and timestamp scheduling.
	static constexpr unsigned MPEG_FMA = 0;
	static constexpr unsigned MPEG_FMV = 1;

	enum : uint8_t
	{
		MPEG_SCAN = 0,
		MPEG_STREAM_ID,
		MPEG_LENGTH_HI,
		MPEG_LENGTH_LO,
		MPEG_PACK_SKIP,
		MPEG_PACKET_SKIP,
		MPEG_PES_HEADER,
		MPEG_PES_STD_SECOND,
		MPEG_PES_HEADER_SKIP,
		MPEG_PAYLOAD
	};

	uint32_t m_mpeg_prefix[2] = { 0, 0 };
	uint8_t m_mpeg_state[2] = { MPEG_SCAN, MPEG_SCAN };
	uint8_t m_mpeg_stream_id[2] = { 0, 0 };
	uint16_t m_mpeg_packet_remaining[2] = { 0, 0 };
	uint8_t m_mpeg_skip_remaining[2] = { 0, 0 };
	bool m_mpeg_selected[2] = { false, false };
	bool m_mpeg_packet_counted[2] = { false, false };

	uint8_t m_mpeg_pack_index[2] = { 0, 0 };
	uint64_t m_mpeg_scr_temp[2] = { 0, 0 };
	uint64_t m_mpeg_last_scr[2] = { 0, 0 };
	bool m_mpeg_have_scr[2] = { false, false };
	uint32_t m_mpeg_scr_events[2] = { 0, 0 };

	uint8_t m_mpeg_ts_mode[2] = { 0, 0 };
	uint8_t m_mpeg_ts_index[2] = { 0, 0 };
	uint64_t m_mpeg_pts_temp[2] = { 0, 0 };
	uint64_t m_mpeg_dts_temp[2] = { 0, 0 };
	uint64_t m_mpeg_packet_pts[2] = { 0, 0 };
	uint64_t m_mpeg_packet_dts[2] = { 0, 0 };
	uint64_t m_mpeg_packet_decode_ts[2] = { 0, 0 };
	bool m_mpeg_packet_have_pts[2] = { false, false };
	bool m_mpeg_packet_have_dts[2] = { false, false };
	uint32_t m_mpeg_pts_events[2] = { 0, 0 };
	uint32_t m_mpeg_dts_events[2] = { 0, 0 };

	uint64_t m_mpeg_clock90 = 0;
	bool m_mpeg_clock_valid = false;
	int64_t m_mpeg_schedule_play_delta90[2] = { 0, 0 };
	int64_t m_mpeg_schedule_decode_delta90[2] = { 0, 0 };
	int32_t m_mpeg_schedule_play_delta45[2] = { 0, 0 };
	int32_t m_mpeg_schedule_decode_delta45[2] = { 0, 0 };
	bool m_mpeg_schedule_valid[2] = { false, false };
	uint32_t m_mpeg_schedule_events[2] = { 0, 0 };

	uint32_t m_mpeg_selected_packets[2] = { 0, 0 };
	uint32_t m_mpeg_payload_bytes[2] = { 0, 0 };
	uint8_t m_mpeg_first_payload[2] = { 0, 0 };
	uint8_t m_mpeg_last_payload[2] = { 0, 0 };
	bool m_mpeg_have_payload[2] = { false, false };

	// MPEG audio decode and MAME sound output.
	sound_stream *m_audio_stream = nullptr;
	std::vector<int16_t> m_audio_pcm_queue;
	size_t m_audio_pcm_read = 0;
	uint32_t m_audio_output_rate = 48000;
	uint64_t m_audio_wait_samples = 0;
	uint64_t m_audio_silence_frames = 0;
	uint32_t m_audio_output_frames = 0;
	uint32_t m_audio_output_nonzero = 0;
	uint32_t m_audio_output_hash = 2166136261U;
	uint32_t m_audio_queue_events = 0;
	bool m_audio_output_started = false;

	plm_buffer_t *m_audio_buffer = nullptr;
	plm_audio_t *m_audio_decoder = nullptr;
	uint32_t m_audio_header_shift = 0;
	uint32_t m_audio_decoded_frames = 0;
	uint32_t m_audio_decoded_samples = 0;
	uint32_t m_audio_header_events = 0;
	uint32_t m_audio_decode_events = 0;
	uint16_t m_audio_bitrate_kbps = 0;
	uint16_t m_audio_samplerate = 0;
	uint8_t m_audio_channel_mode = 0;
	uint8_t m_audio_backend_status = 0;
	bool m_audio_have_es_header = false;
	bool m_audio_have_header = false;

	// MPEG video decode and MAME video presentation.
	std::vector<uint8_t> m_video_rgb24;
	std::vector<uint32_t> m_video_decode_frame;
	uint16_t m_video_decode_width = 0;
	uint16_t m_video_decode_height = 0;
	uint32_t m_video_decode_generation = 0;
	bool m_video_decode_valid = false;
	uint16_t m_video_decode_interrupts = 0;

	std::vector<uint32_t> m_video_present_frame;
	uint16_t m_video_present_width = 0;
	uint16_t m_video_present_height = 0;
	uint32_t m_video_present_generation = 0;
	bool m_video_present_valid = false;

	uint16_t m_video_screen_y_shadow = 0;
	uint16_t m_video_screen_x_shadow = 0;
	uint16_t m_video_window_h_shadow = 0;
	uint16_t m_video_window_w_shadow = 0;
	uint16_t m_video_crop_y_shadow = 0;
	uint16_t m_video_crop_x_shadow = 0;
	uint16_t m_video_screen_y = 0;
	uint16_t m_video_screen_x = 0;
	uint16_t m_video_window_h = 0;
	uint16_t m_video_window_w = 0;
	uint16_t m_video_crop_y = 0;
	uint16_t m_video_crop_x = 0;
	bool m_video_geometry_frame_pending = false;
	bool m_video_geometry_vblank_pending = false;
	bool m_video_visible = false;
	bool m_video_output_enabled = false;
	bool m_video_show_on_next = false;

	uint32_t m_video_overlay_hash = 2166136261U;
	uint32_t m_video_overlay_pixels = 0;
	bool m_video_overlay_complete = false;

	plm_buffer_t *m_video_buffer = nullptr;
	plm_video_t *m_video_decoder = nullptr;
	uint32_t m_video_es_prefix = 0;
	uint32_t m_video_sequence_headers = 0;
	uint32_t m_video_gop_headers = 0;
	uint32_t m_video_picture_headers = 0;
	uint8_t m_video_picture_header_bytes = 0;
	uint16_t m_video_picture_marker_interrupts = 0;
	uint16_t m_video_reference_interrupts = 0;
	bool m_video_reference_valid = false;
	std::vector<uint16_t> m_video_picture_event_queue;
	size_t m_video_picture_event_read = 0;
	uint32_t m_video_decoded_frames = 0;
	uint16_t m_video_width = 0;
	uint16_t m_video_height = 0;
	uint32_t m_video_framerate_millihz = 0;
	bool m_video_have_sequence = false;
	bool m_video_sequence_end_pending = false;
	uint32_t m_video_sequence_end_events = 0;
	uint32_t m_video_last_picture_generation = 0;
	bool m_video_last_picture_pending = false;

	// 512 KiB MPEG/DVC RAM at E80000-EFFFFF.
	//
	// VALIDATED BEHAVIORAL CONSTRAINT:
	//   MPEG RAM must not participate in the ordinary CD-RTOS boot-time RAM
	//   crawl, then must become usable by genuine DVC firmware later.
	//
	// MAME-ONLY COMPATIBILITY MECHANISM:
	//   64 VMPEG writes currently make the RAM visible.  This threshold is not
	//   known to represent a hardware register, bit, counter, or timing rule.
	//   Keep it isolated here until the real ownership/enable mechanism is
	//   identified.
	static constexpr uint8_t MPEG_RAM_COMPAT_ACTIVATION_WRITES = 64;
	std::array<uint16_t, 0x40000> m_mpeg_ram{};
	uint8_t m_mpeg_ram_compat_write_count = 0;
	bool m_mpeg_ram_compat_visible = false;
	uint32_t m_mpeg_ram_gated_reads = 0;
	uint32_t m_mpeg_ram_gated_writes = 0;
};

DECLARE_DEVICE_TYPE(CDI_DVC, cdi_dvc_device)

#endif // MAME_PHILIPS_CDIDVC_H
