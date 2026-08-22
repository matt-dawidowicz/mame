// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#ifndef MAME_PHILIPS_CDIDVC_H
#define MAME_PHILIPS_CDIDVC_H

#pragma once

#include "cdidvc_save_state.h"

#include <array>
#include <deque>
#include <memory>
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
	uint64_t current_mpeg_clock90(unsigned target);
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
	void video_decoder_pump(bool end_signalled = false);
	void video_decoder_flush();
	void video_picture_event(uint8_t picture_type);
	void video_picture_events_flush();
	uint16_t video_picture_events_pop();

	// Provisional MPEG-RAM compatibility mechanism.
	void mpeg_ram_compat_reset();

	// Measurement-only diagnostics; no guest-visible behavior.
	void record_dsp_bootstrap_write(unsigned index, uint32_t address, uint16_t data, uint16_t mem_mask);
	void av_clock_observe();

	// SAVE-STATE IMPLEMENTATION MODEL: dynamic queues are mirrored into
	// fixed registered storage and opaque PL_MPEG state is reconstructed
	// by replaying elementary-stream bytes on postload.
	void save_state_presave();
	void save_state_postload();
	void save_state_restore_failed();
	bool save_state_rebuild_audio_decoder();
	bool save_state_rebuild_video_decoder();

	// Current MAME compatibility values. Their hardware attribution is pending;
	// keep them out of emulator-independent specifications until proven.
	static constexpr uint16_t FMA_STATUS_COMPAT_FIXED_BITS = 0x0200;
	static constexpr uint16_t FMA_E03004_COMPAT_READ_VALUE = 0x0007;
	static constexpr uint16_t FMA_E03006_COMPAT_READ_VALUE = 0x0900;
	static constexpr uint16_t FMA_E0300E_COMPAT_READ_VALUE = 0x0042;
	static constexpr uint16_t FMA_E03024_COMPAT_READ_VALUE = 0x0004;
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
	uint16_t m_fmv_video_data_input_command = 0;
	bool m_fmv_playback_active = false;
	bool m_fmv_single_step_pending = false;
	bool m_fmv_decoder_enabled = false;
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

	// Measurement-only firmware bootstrap register telemetry.
	std::array<uint32_t, 6> m_dsp_bootstrap_write_count{};
	std::array<uint16_t, 6> m_dsp_bootstrap_last{};

	// Measurement-only common-clock telemetry.
	uint64_t m_av_last_audio_pts90 = 0;
	uint64_t m_av_last_video_pts90 = 0;
	bool m_av_audio_pts_valid = false;
	bool m_av_video_pts_valid = false;
	int64_t m_av_last_cross_delta90 = 0;
	uint64_t m_av_max_abs_cross_delta90 = 0;
	uint64_t m_av_cross_observations = 0;

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

	// Measurement-only raw timestamp fields and marker diagnostics.
	std::array<uint64_t, 2> m_mpeg_scr_raw{};
	std::array<uint64_t, 2> m_mpeg_pts_raw{};
	std::array<uint64_t, 2> m_mpeg_dts_raw{};
	std::array<uint32_t, 2> m_mpeg_scr_marker_errors{};
	std::array<uint32_t, 2> m_mpeg_pts_marker_errors{};
	std::array<uint32_t, 2> m_mpeg_dts_marker_errors{};

	uint64_t m_mpeg_clock90 = 0;
	bool m_mpeg_clock_valid = false;
	uint32_t m_mpeg_scr_dclk_anchor[2] = { 0, 0 };
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

	// CURRENT IMPLEMENTATION MODEL, NOT HARDWARE SPECIFICATION:
	// retain decoded pictures until their presentation time is examined.
	struct queued_video_frame
	{
		std::vector<uint32_t> pixels;
		uint16_t width = 0;
		uint16_t height = 0;
		uint32_t generation = 0;
		uint16_t interrupts = 0;
		uint64_t timestamp90 = 0;
		bool timestamp_valid = false;
	};
	std::deque<queued_video_frame> m_video_queue;
	uint64_t m_video_pts_anchor90 = 0;
	uint64_t m_video_backend_anchor90 = 0;
	bool m_video_pts_anchor_valid = false;

	std::vector<uint32_t> m_video_present_frame;
	uint16_t m_video_present_width = 0;
	uint16_t m_video_present_height = 0;
	uint32_t m_video_present_generation = 0;
	bool m_video_present_valid = false;

	// Runtime evidence counters for the queue implementation.  These are
	// diagnostic only and do not participate in guest-visible behavior.
	uint64_t m_scheduler_decoded_frames = 0;
	uint64_t m_scheduler_presented_frames = 0;
	uint64_t m_scheduler_due_superseded = 0;
	uint64_t m_scheduler_flush_dropped = 0;
	uint64_t m_scheduler_wait_vblanks = 0;
	uint64_t m_scheduler_fallback_presented = 0;
	uint64_t m_scheduler_clocked_presented = 0;
	uint64_t m_scheduler_total_late90 = 0;
	uint64_t m_scheduler_max_late90 = 0;
	uint64_t m_scheduler_compat_frame_events = 0;
	uint64_t m_scheduler_max_queue_depth = 0;
	uint64_t m_scheduler_vblanks = 0;

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
	// Measurement-only attribution counters. They answer whether DVC
	// composition wrote the first 64 visible rows; they do not diagnose
	// MCD212 plane/backdrop/cursor semantics by themselves.
	uint64_t m_video_overlay_total_pixels = 0;
	uint64_t m_video_overlay_top64_pixels = 0;

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
	bool m_video_decoder_flush_pending = false;
	uint32_t m_video_sequence_end_events = 0;
	uint32_t m_video_last_picture_generation = 0;
	bool m_video_last_picture_pending = false;

	// SAVE-STATE IMPLEMENTATION MODEL, NOT HARDWARE SPECIFICATION.
	// PL_MPEG owns pointer-rich decoder state that MAME cannot serialize
	// directly.  Journal the ES byte stream and mirror dynamic queues into
	// fixed registered buffers; postload reconstructs the decoder to the saved
	// decoded-frame count without replaying guest-visible side effects.
	std::vector<uint8_t> m_audio_replay_journal;
	std::vector<uint8_t> m_video_replay_journal;
	std::vector<uint64_t> m_video_replay_pump_events;
	bool m_audio_replay_overflow = false;
	bool m_video_replay_overflow = false;
	bool m_video_replay_pump_overflow = false;

	std::unique_ptr<uint8_t[]> m_save_audio_replay;
	std::unique_ptr<uint8_t[]> m_save_video_replay;
	std::unique_ptr<int16_t[]> m_save_audio_pcm;
	std::unique_ptr<uint32_t[]> m_save_video_queue_pixels;
	std::unique_ptr<uint32_t[]> m_save_video_present_pixels;
	uint32_t m_save_audio_replay_length = 0;
	uint32_t m_save_video_replay_length = 0;
	uint32_t m_save_audio_pcm_values = 0;
	uint16_t m_save_video_queue_count = 0;
	uint32_t m_save_video_present_pixel_count = 0;
	uint16_t m_save_picture_event_count = 0;
	uint32_t m_save_video_replay_pump_count = 0;
	bool m_save_snapshot_valid = false;
	uint32_t m_save_snapshot_serial = 0;

	std::array<uint16_t, cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES> m_save_video_queue_width{};
	std::array<uint16_t, cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES> m_save_video_queue_height{};
	std::array<uint32_t, cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES> m_save_video_queue_generation{};
	std::array<uint16_t, cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES> m_save_video_queue_interrupts{};
	std::array<uint64_t, cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES> m_save_video_queue_timestamp90{};
	std::array<uint8_t, cdi_dvc::SAVE_VIDEO_QUEUE_FRAMES> m_save_video_queue_timestamp_valid{};
	std::array<uint16_t, cdi_dvc::SAVE_PICTURE_EVENTS> m_save_picture_events{};
	std::array<uint64_t, cdi_dvc::SAVE_VIDEO_REPLAY_PUMP_EVENTS> m_save_video_replay_pump_events{};

	// 512 KiB MPEG/DVC RAM at E80000-EFFFFF.
	//
	// VALIDATED BEHAVIORAL CONSTRAINT:
	//   MPEG RAM must not participate in the ordinary CD-RTOS boot-time RAM
	//   crawl, then must become usable by genuine DVC firmware later.
	//
	// PROVISIONAL COMPATIBILITY MECHANISM, NOT HARDWARE SPECIFICATION:
	//   firmware-visible E03018 bit 0 controls MAME-side MPEG-RAM visibility.
	//   This model is validated for the current PAL/NTSC workloads, but the
	//   physical meaning and lifecycle of E03018 remain unknown.
	std::array<uint16_t, 0x40000> m_mpeg_ram{};
	bool m_mpeg_ram_compat_visible = false;
	uint32_t m_mpeg_ram_gated_reads = 0;
	uint32_t m_mpeg_ram_gated_writes = 0;
};

DECLARE_DEVICE_TYPE(CDI_DVC, cdi_dvc_device)

#endif // MAME_PHILIPS_CDIDVC_H
