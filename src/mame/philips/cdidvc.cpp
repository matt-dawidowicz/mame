// license:BSD-3-Clause
// copyright-holders:Matt Jordan
/***************************************************************************

    Philips CD-i Digital Video Cartridge
    -------------------------------------

    VMPEG/DVC emulation for CD-i Mono-I systems.

    The current model includes VMPEG register access, interrupt generation and
    acknowledge behavior, SCC68070 DMA ingress, MPEG-1 program-stream parsing,
    PL_MPEG-backed audio/video decoding, audio output, and video presentation.

    Known limitations remain: DMA service pacing and A/V presentation policy
    are current implementation models pending broader runtime calibration;
    decoder/presentation save-state reconstruction is still incomplete; and
    E03018 MPEG-RAM visibility, several fixed register values, and presentation
    scale factors still lack independent physical-hardware attribution.

***************************************************************************/

#include "emu.h"
#include "cdidvc.h"
#include "cdidvc_fidelity.h"
#include "cdidvc_mpeg_format.h"
#include "cdidvc_save_state.h"
#include "cdidvc_utils.h"

#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"

#define LOG_REGISTERS    (1U << 1)
#define LOG_DMA          (1U << 2)
#define LOG_IRQ          (1U << 3)
#define LOG_MPEG         (1U << 4)
#define LOG_AUDIO        (1U << 5)
#define LOG_VIDEO        (1U << 6)
#define LOG_RAM_GATE     (1U << 7)
#define LOG_RAM_ACCESS   (1U << 8)
#define LOG_SEQUENCE     (1U << 9)

// Keep the low-volume gate transition visible for the 12G14 regression.
// Keep the low-volume interactive-presentation boundary trace visible while
// Dragon's Lair scene chaining is under investigation.  Per-frame video and
// general register diagnostics remain opt-in.
#define VERBOSE          (LOG_RAM_GATE | LOG_SEQUENCE)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(CDI_DVC, cdi_dvc_device, "cdidvc", "Philips CD-i Digital Video Cartridge")

cdi_dvc_device::cdi_dvc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, CDI_DVC, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_intreq_callback(*this)
	, m_dma_req_callback(*this)
	, m_driver_rom(*this, "^dvc_rom")
{
}

void cdi_dvc_device::device_start()
{
	m_mpeg_ram.fill(0);
	save_item(NAME(m_mpeg_ram));
	save_item(NAME(m_mpeg_ram_compat_visible));
	save_item(NAME(m_mpeg_ram_gated_reads));
	save_item(NAME(m_mpeg_ram_gated_writes));

	m_audio_stream = stream_alloc(0, 2, 48000);
	m_audio_output_rate = 48000;

	m_interrupt_timer = timer_alloc(FUNC(cdi_dvc_device::timer_tick), this);

	save_item(NAME(m_dclk_epoch_ticks));
	save_item(NAME(m_fmv_dclk_offset));
	save_item(NAME(m_fma_dclk_latch));

	save_item(NAME(m_vcd_control));

	save_item(NAME(m_fma_command));
	save_item(NAME(m_fma_status));
	save_item(NAME(m_fma_stream));
	save_item(NAME(m_fma_interrupt_vector));
	save_item(NAME(m_fma_interrupt_status));
	save_item(NAME(m_fma_interrupt_enable));

	save_item(NAME(m_fmv_interrupt_enable));
	save_item(NAME(m_fmv_interrupt_status));
	save_item(NAME(m_fmv_timer_compare));
	save_item(NAME(m_fmv_system_command));
	save_item(NAME(m_fmv_system_control));
	save_item(NAME(m_fmv_video_command));
	save_item(NAME(m_fmv_video_data_input_command));
	save_item(NAME(m_fmv_playback_active));
	save_item(NAME(m_fmv_single_step_pending));
	save_item(NAME(m_fmv_decoder_enabled));
	save_item(NAME(m_video_output_enabled));
	save_item(NAME(m_fmv_stream));
	save_item(NAME(m_fmv_interrupt_vector));

	save_item(NAME(m_fmv_transfer_word));
	save_item(NAME(m_fmv_transfer_words));

	save_item(NAME(m_dma_active));
	save_item(NAME(m_dma_for_fma));
	save_item(NAME(m_dma_transfer_words));
	save_item(NAME(m_dma_first_word));
	save_item(NAME(m_dma_last_word));

	save_item(NAME(m_dsp_bootstrap_write_count));
	save_item(NAME(m_dsp_bootstrap_last));
	save_item(NAME(m_av_last_audio_pts90));
	save_item(NAME(m_av_last_video_pts90));
	save_item(NAME(m_av_audio_pts_valid));
	save_item(NAME(m_av_video_pts_valid));
	save_item(NAME(m_av_last_cross_delta90));
	save_item(NAME(m_av_max_abs_cross_delta90));
	save_item(NAME(m_av_cross_observations));

	save_item(NAME(m_mpeg_prefix));
	save_item(NAME(m_mpeg_state));
	save_item(NAME(m_mpeg_stream_id));
	save_item(NAME(m_mpeg_packet_remaining));
	save_item(NAME(m_mpeg_skip_remaining));
	save_item(NAME(m_mpeg_selected));
	save_item(NAME(m_mpeg_packet_counted));
	save_item(NAME(m_fma_access_header_window));
	save_item(NAME(m_fma_access_frame_remaining));
	save_item(NAME(m_fma_access_header_bytes));
	save_item(NAME(m_mpeg_selected_packets));
	save_item(NAME(m_mpeg_payload_bytes));
	save_item(NAME(m_mpeg_first_payload));
	save_item(NAME(m_mpeg_last_payload));
	save_item(NAME(m_mpeg_have_payload));
	save_item(NAME(m_mpeg_pack_index));
	save_item(NAME(m_mpeg_scr_temp));
	save_item(NAME(m_mpeg_last_scr));
	save_item(NAME(m_mpeg_have_scr));
	save_item(NAME(m_mpeg_scr_events));
	save_item(NAME(m_mpeg_ts_mode));
	save_item(NAME(m_mpeg_ts_index));
	save_item(NAME(m_mpeg_pts_temp));
	save_item(NAME(m_mpeg_dts_temp));
	save_item(NAME(m_mpeg_packet_pts));
	save_item(NAME(m_mpeg_packet_dts));
	save_item(NAME(m_mpeg_packet_decode_ts));
	save_item(NAME(m_mpeg_packet_have_pts));
	save_item(NAME(m_mpeg_packet_have_dts));
	save_item(NAME(m_mpeg_pts_events));
	save_item(NAME(m_mpeg_dts_events));
	save_item(NAME(m_mpeg_scr_raw));
	save_item(NAME(m_mpeg_pts_raw));
	save_item(NAME(m_mpeg_dts_raw));
	save_item(NAME(m_mpeg_scr_marker_errors));
	save_item(NAME(m_mpeg_pts_marker_errors));
	save_item(NAME(m_mpeg_dts_marker_errors));
	save_item(NAME(m_mpeg_clock90));
	save_item(NAME(m_mpeg_clock_valid));
	save_item(NAME(m_mpeg_scr_dclk_anchor));
	save_item(NAME(m_mpeg_schedule_play_delta90));
	save_item(NAME(m_mpeg_schedule_decode_delta90));
	save_item(NAME(m_mpeg_schedule_play_delta45));
	save_item(NAME(m_mpeg_schedule_decode_delta45));
	save_item(NAME(m_mpeg_schedule_valid));
	save_item(NAME(m_mpeg_schedule_events));

	// Save guest-visible audio/video state not covered by the parser/register
	// block above. Dynamic containers are handled by presave/postload mirrors.
	save_item(NAME(m_audio_output_rate));
	save_item(NAME(m_audio_wait_samples));
	save_item(NAME(m_audio_silence_frames));
	save_item(NAME(m_audio_output_frames));
	save_item(NAME(m_audio_output_nonzero));
	save_item(NAME(m_audio_output_hash));
	save_item(NAME(m_audio_queue_events));
	save_item(NAME(m_audio_starvation_frames));
	save_item(NAME(m_audio_starvation_events));
	save_item(NAME(m_audio_output_started));
	save_item(NAME(m_audio_starved));
	save_item(NAME(m_audio_header_shift));
	save_item(NAME(m_audio_decoded_frames));
	save_item(NAME(m_audio_decoded_samples));
	save_item(NAME(m_audio_header_events));
	save_item(NAME(m_audio_decode_events));
	save_item(NAME(m_audio_profile_violations));
	save_item(NAME(m_audio_bitrate_kbps));
	save_item(NAME(m_audio_samplerate));
	save_item(NAME(m_audio_channel_mode));
	save_item(NAME(m_audio_backend_status));
	save_item(NAME(m_audio_have_es_header));
	save_item(NAME(m_audio_have_header));
	save_item(NAME(m_audio_decoder_end_signalled));
	save_item(NAME(m_audio_backend_ended));

	save_item(NAME(m_video_pts_anchor90));
	save_item(NAME(m_video_backend_anchor90));
	save_item(NAME(m_video_pts_anchor_valid));
	save_item(NAME(m_video_present_width));
	save_item(NAME(m_video_present_height));
	save_item(NAME(m_video_present_generation));
	save_item(NAME(m_video_present_valid));
	save_item(NAME(m_scheduler_decoded_frames));
	save_item(NAME(m_scheduler_presented_frames));
	save_item(NAME(m_scheduler_due_superseded));
	save_item(NAME(m_scheduler_flush_dropped));
	save_item(NAME(m_scheduler_wait_vblanks));
	save_item(NAME(m_scheduler_fallback_presented));
	save_item(NAME(m_scheduler_clocked_presented));
	save_item(NAME(m_scheduler_total_late90));
	save_item(NAME(m_scheduler_max_late90));
	save_item(NAME(m_scheduler_compat_frame_events));
	save_item(NAME(m_scheduler_max_queue_depth));
	save_item(NAME(m_scheduler_vblanks));
	save_item(NAME(m_video_screen_y_shadow));
	save_item(NAME(m_video_screen_x_shadow));
	save_item(NAME(m_video_window_h_shadow));
	save_item(NAME(m_video_window_w_shadow));
	save_item(NAME(m_video_crop_y_shadow));
	save_item(NAME(m_video_crop_x_shadow));
	save_item(NAME(m_video_screen_y));
	save_item(NAME(m_video_screen_x));
	save_item(NAME(m_video_window_h));
	save_item(NAME(m_video_window_w));
	save_item(NAME(m_video_crop_y));
	save_item(NAME(m_video_crop_x));
	save_item(NAME(m_video_geometry_frame_pending));
	save_item(NAME(m_video_geometry_vblank_pending));
	save_item(NAME(m_video_visible));
	save_item(NAME(m_video_show_on_next));
	save_item(NAME(m_video_overlay_hash));
	save_item(NAME(m_video_overlay_pixels));
	save_item(NAME(m_video_overlay_complete));
	save_item(NAME(m_video_overlay_total_pixels));
	save_item(NAME(m_video_overlay_top64_pixels));
	save_item(NAME(m_video_es_prefix));
	save_item(NAME(m_video_sequence_headers));
	save_item(NAME(m_video_gop_headers));
	save_item(NAME(m_video_picture_headers));
	save_item(NAME(m_video_picture_header_bytes));
	save_item(NAME(m_video_picture_marker_interrupts));
	save_item(NAME(m_video_reference_interrupts));
	save_item(NAME(m_video_reference_valid));
	save_item(NAME(m_video_decoded_frames));
	save_item(NAME(m_video_width));
	save_item(NAME(m_video_height));
	save_item(NAME(m_video_framerate_millihz));
	save_item(NAME(m_video_have_sequence));
	save_item(NAME(m_video_sequence_end_pending));
	save_item(NAME(m_video_decoder_flush_pending));
	save_item(NAME(m_video_decoder_waiting_for_input));
	save_item(NAME(m_video_sequence_end_events));
	save_item(NAME(m_video_last_picture_generation));
	save_item(NAME(m_video_last_picture_pending));

	m_save_audio_replay = std::make_unique<uint8_t[]>(cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY);
	m_save_video_replay = std::make_unique<uint8_t[]>(cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY);
	m_save_audio_pcm = std::make_unique<int16_t[]>(cdi_dvc::SAVE_AUDIO_PCM_VALUES);
	m_save_video_queue_pixels = std::make_unique<uint32_t[]>(cdi_dvc::SAVE_VIDEO_QUEUE_PIXELS);
	m_save_video_present_pixels = std::make_unique<uint32_t[]>(cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME);

	save_pointer(NAME(m_save_audio_replay), cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY);
	save_pointer(NAME(m_save_video_replay), cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY);
	save_pointer(NAME(m_save_audio_pcm), cdi_dvc::SAVE_AUDIO_PCM_VALUES);
	save_pointer(NAME(m_save_video_queue_pixels), cdi_dvc::SAVE_VIDEO_QUEUE_PIXELS);
	save_pointer(NAME(m_save_video_present_pixels), cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME);
	save_item(NAME(m_save_audio_replay_length));
	save_item(NAME(m_save_video_replay_length));
	save_item(NAME(m_save_audio_pcm_values));
	save_item(NAME(m_save_video_queue_count));
	save_item(NAME(m_save_video_present_pixel_count));
	save_item(NAME(m_save_picture_event_count));
	save_item(NAME(m_save_snapshot_valid));
	save_item(NAME(m_save_snapshot_serial));
	save_item(NAME(m_save_video_queue_width));
	save_item(NAME(m_save_video_queue_height));
	save_item(NAME(m_save_video_queue_generation));
	save_item(NAME(m_save_video_queue_interrupts));
	save_item(NAME(m_save_video_queue_timestamp90));
	save_item(NAME(m_save_video_queue_timestamp_valid));
	save_item(NAME(m_save_picture_events));
	save_item(NAME(m_save_video_replay_pump_count));
	save_item(NAME(m_save_video_replay_pump_events));
	save_item(NAME(m_audio_replay_overflow));
	save_item(NAME(m_video_replay_overflow));
	save_item(NAME(m_video_replay_pump_overflow));

	machine().save().register_presave(save_prepost_delegate(FUNC(cdi_dvc_device::save_state_presave), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(cdi_dvc_device::save_state_postload), this));
}

void cdi_dvc_device::save_state_presave()
{
	if (m_audio_stream)
		m_audio_stream->update();

	cdi_dvc::compact_consumed_audio_samples(m_audio_pcm_queue, m_audio_pcm_read);
	if (m_video_picture_event_read)
	{
		if (m_video_picture_event_read > m_video_picture_event_queue.size())
		{
			m_video_picture_event_queue.clear();
		}
		else
		{
			m_video_picture_event_queue.erase(
					m_video_picture_event_queue.begin(),
					m_video_picture_event_queue.begin() + m_video_picture_event_read);
		}
		m_video_picture_event_read = 0;
	}

	m_save_snapshot_valid = !m_audio_replay_overflow
			&& !m_video_replay_overflow
			&& !m_video_replay_pump_overflow;
	m_save_audio_replay_length = uint32_t(m_audio_replay_journal.size());
	m_save_video_replay_length = uint32_t(m_video_replay_journal.size());
	m_save_audio_pcm_values = uint32_t(m_audio_pcm_queue.size());
	m_save_video_queue_count = uint16_t(m_video_queue.size());
	m_save_picture_event_count = uint16_t(m_video_picture_event_queue.size());
	m_save_video_present_pixel_count = uint32_t(m_video_present_frame.size());
	m_save_video_replay_pump_count = uint32_t(m_video_replay_pump_events.size());

	m_save_snapshot_valid &= cdi_dvc::save_replay_fits(
			m_audio_replay_journal.size(), cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY);
	m_save_snapshot_valid &= cdi_dvc::save_replay_fits(
			m_video_replay_journal.size(), cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY);
	m_save_snapshot_valid &= cdi_dvc::save_audio_pcm_fits(m_audio_pcm_queue.size());
	m_save_snapshot_valid &= cdi_dvc::save_video_queue_fits(m_video_queue.size());
	m_save_snapshot_valid &= cdi_dvc::save_picture_events_fit(m_video_picture_event_queue.size());
	m_save_snapshot_valid &= cdi_dvc::save_video_replay_pumps_fit(m_video_replay_pump_events.size());
	std::size_t previous_pump_offset = 0;
	uint32_t previous_pump_frames = 0;
	for (uint64_t const event : m_video_replay_pump_events)
	{
		std::size_t const offset = cdi_dvc::save_video_replay_pump_offset(event);
		uint32_t const frames = cdi_dvc::save_video_replay_pump_frames(event);
		if (!cdi_dvc::save_video_replay_pump_offset_valid(
				previous_pump_offset, offset, m_video_replay_journal.size())
				|| frames < previous_pump_frames || frames > m_video_decoded_frames)
		{
			m_save_snapshot_valid = false;
			break;
		}
		previous_pump_offset = offset;
		previous_pump_frames = frames;
	}
	m_save_snapshot_valid &= cdi_dvc::video_present_frame_fits(
			m_video_present_valid, m_video_present_width,
			m_video_present_height, m_video_present_frame.size());

	if (m_save_snapshot_valid)
	{
		std::copy(m_audio_replay_journal.begin(), m_audio_replay_journal.end(), m_save_audio_replay.get());
		std::copy(m_video_replay_journal.begin(), m_video_replay_journal.end(), m_save_video_replay.get());
		std::copy(m_audio_pcm_queue.begin(), m_audio_pcm_queue.end(), m_save_audio_pcm.get());
		std::copy(m_video_picture_event_queue.begin(), m_video_picture_event_queue.end(), m_save_picture_events.begin());
		std::copy(m_video_replay_pump_events.begin(), m_video_replay_pump_events.end(),
				m_save_video_replay_pump_events.begin());
		std::copy(m_video_present_frame.begin(), m_video_present_frame.end(), m_save_video_present_pixels.get());

		std::size_t index = 0;
		for (queued_video_frame const &frame : m_video_queue)
		{
			if (!cdi_dvc::save_video_frame_fits(frame.width, frame.height, frame.pixels.size()))
			{
				m_save_snapshot_valid = false;
				break;
			}
			m_save_video_queue_width[index] = frame.width;
			m_save_video_queue_height[index] = frame.height;
			m_save_video_queue_generation[index] = frame.generation;
			m_save_video_queue_interrupts[index] = frame.interrupts;
			m_save_video_queue_timestamp90[index] = frame.timestamp90;
			m_save_video_queue_timestamp_valid[index] = frame.timestamp_valid ? 1U : 0U;
			std::copy(frame.pixels.begin(), frame.pixels.end(),
					m_save_video_queue_pixels.get() + index * cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME);
			++index;
		}
	}

	++m_save_snapshot_serial;
	logerror("DVC_SAVE_STATE_SNAPSHOT serial=%u valid=%u audio_replay=%u video_replay=%u pcm_values=%u queue=%u present_pixels=%u picture_events=%u audio_overflow=%u video_overflow=%u\n",
			m_save_snapshot_serial, m_save_snapshot_valid ? 1U : 0U,
			m_save_audio_replay_length, m_save_video_replay_length,
			m_save_audio_pcm_values, m_save_video_queue_count,
			m_save_video_present_pixel_count, m_save_picture_event_count,
			m_audio_replay_overflow ? 1U : 0U, m_video_replay_overflow ? 1U : 0U);
	unsigned replay_flushes = 0;
	for (uint64_t const event : m_video_replay_pump_events)
		replay_flushes += cdi_dvc::save_video_replay_pump_flush(event) ? 1U : 0U;
	logerror("DVC_SAVE_STATE_REPLAY_SCHEDULE serial=%u pumps=%u flushes=%u tail_bytes=%u pump_overflow=%u\n",
			m_save_snapshot_serial, m_save_video_replay_pump_count, replay_flushes,
			unsigned(m_save_video_replay_length - (m_video_replay_pump_events.empty()
					? 0U
					: cdi_dvc::save_video_replay_pump_offset(m_video_replay_pump_events.back()))),
			m_video_replay_pump_overflow ? 1U : 0U);
}

bool cdi_dvc_device::save_state_rebuild_audio_decoder()
{
	uint32_t const wanted_frames = m_audio_decoded_frames;
	bool const wanted_end_signalled = m_audio_decoder_end_signalled;
	bool const wanted_backend_ended = m_audio_backend_ended;
	audio_decoder_destroy();
	m_audio_buffer = plm_buffer_create_with_capacity(128 * 1024);
	m_audio_decoder = plm_audio_create_with_buffer(m_audio_buffer, 1);
	if (!m_audio_buffer || !m_audio_decoder)
		return false;

	if (m_save_audio_replay_length)
		plm_buffer_write(m_audio_buffer, m_save_audio_replay.get(), m_save_audio_replay_length);

	if (m_audio_have_header && !plm_audio_has_header(m_audio_decoder))
		return false;

	for (uint32_t frame = 0; frame < wanted_frames; ++frame)
	{
		if (!plm_audio_decode(m_audio_decoder))
			return false;
	}

	// PL_MPEG's end marker belongs to its opaque input buffer rather than the
	// journalled bytes.  Reapply it after replaying those bytes.  If the live
	// backend had already observed the end, reproduce the final failed decode
	// that sets PL_MPEG's has-ended latch; it must not reveal another frame.
	if (wanted_end_signalled)
		plm_buffer_signal_end(m_audio_buffer);
	if (wanted_backend_ended)
	{
		if (!wanted_end_signalled || plm_audio_decode(m_audio_decoder))
			return false;
	}
	if (bool(plm_audio_has_ended(m_audio_decoder)) != wanted_backend_ended)
		return false;
	return true;
}

bool cdi_dvc_device::save_state_rebuild_video_decoder()
{
	uint32_t const wanted_frames = m_video_decoded_frames;
	video_decoder_destroy();
	m_video_buffer = plm_buffer_create_with_capacity(256 * 1024);
	m_video_decoder = plm_video_create_with_buffer(m_video_buffer, 1);
	if (!m_video_buffer || !m_video_decoder)
		return false;

	std::size_t replay_cursor = 0;
	uint32_t rebuilt_frames = 0;
	bool replay_have_header = false;
	unsigned replay_flushes = 0;

	auto refresh_header = [&]()
	{
		if (!replay_have_header)
			replay_have_header = plm_video_has_header(m_video_decoder) != 0;
	};

	auto pump_backend = [&](bool signal_end, uint32_t event_index, uint32_t target_frames) -> bool
	{
		refresh_header();
		if (signal_end)
		{
			plm_buffer_signal_end(m_video_buffer);
			++replay_flushes;
		}
		if (!replay_have_header)
			return target_frames == rebuilt_frames;

		if (target_frames < rebuilt_frames || target_frames > wanted_frames)
		{
			logerror("DVC_SAVE_STATE_VIDEO_REPLAY_FAIL serial=%u reason=invalid_frame_target event=%u rebuilt=%u target=%u wanted=%u\n",
					m_save_snapshot_serial, event_index, rebuilt_frames,
					target_frames, wanted_frames);
			return false;
		}

		while (rebuilt_frames < target_frames)
		{
			plm_frame_t *const frame = plm_video_decode(m_video_decoder);
			if (!frame)
			{
				logerror("DVC_SAVE_STATE_VIDEO_REPLAY_FAIL serial=%u reason=frame_underrun event=%u rebuilt=%u target=%u wanted=%u\n",
						m_save_snapshot_serial, event_index, rebuilt_frames,
						target_frames, wanted_frames);
				return false;
			}
			++rebuilt_frames;
		}
		return true;
	};

	for (uint32_t event_index = 0; event_index < m_save_video_replay_pump_count; ++event_index)
	{
		uint64_t const event = m_save_video_replay_pump_events[event_index];
		std::size_t const offset = cdi_dvc::save_video_replay_pump_offset(event);
		if (!cdi_dvc::save_video_replay_pump_offset_valid(
				replay_cursor, offset, m_save_video_replay_length))
		{
			logerror("DVC_SAVE_STATE_VIDEO_REPLAY_FAIL serial=%u reason=invalid_pump_offset event=%u previous=%u offset=%u bytes=%u\n",
					m_save_snapshot_serial, event_index, unsigned(replay_cursor),
					unsigned(offset), m_save_video_replay_length);
			return false;
		}

		if (offset > replay_cursor)
		{
			plm_buffer_write(m_video_buffer,
				m_save_video_replay.get() + replay_cursor, offset - replay_cursor);
			replay_cursor = offset;
		}

		// Every live bounded pump records both its byte offset and exact decoded
		// frame target.  Reproduce that schedule so refills after presentation do
		// not collapse into an unbounded replay-time decode.
		if (!pump_backend(cdi_dvc::save_video_replay_pump_flush(event), event_index,
				cdi_dvc::save_video_replay_pump_frames(event)))
			return false;
	}

	// Bytes after the final completed packet were already written into the
	// live decoder buffer at save time, but had not yet reached packet_done().
	// Restore those bytes without decoding them so a mid-packet save does not
	// advance the reconstructed backend beyond the saved point.
	if (replay_cursor < m_save_video_replay_length)
	{
		plm_buffer_write(m_video_buffer, m_save_video_replay.get() + replay_cursor,
			m_save_video_replay_length - replay_cursor);
		replay_cursor = m_save_video_replay_length;
		refresh_header();
	}

	if (replay_have_header != m_video_have_sequence)
	{
		logerror("DVC_SAVE_STATE_VIDEO_REPLAY_FAIL serial=%u reason=header_mismatch replay=%u saved=%u rebuilt=%u wanted=%u\n",
				m_save_snapshot_serial, replay_have_header ? 1U : 0U,
				m_video_have_sequence ? 1U : 0U, rebuilt_frames, wanted_frames);
		return false;
	}
	if (rebuilt_frames != wanted_frames)
	{
		logerror("DVC_SAVE_STATE_VIDEO_REPLAY_FAIL serial=%u reason=frame_count rebuilt=%u wanted=%u pumps=%u flushes=%u tail_bytes=%u\n",
				m_save_snapshot_serial, rebuilt_frames, wanted_frames,
				m_save_video_replay_pump_count, replay_flushes,
				unsigned(m_save_video_replay_length - (m_save_video_replay_pump_count
						? cdi_dvc::save_video_replay_pump_offset(
							m_save_video_replay_pump_events[m_save_video_replay_pump_count - 1])
						: 0U)));
		return false;
	}

	logerror("DVC_SAVE_STATE_VIDEO_REPLAY_OK serial=%u bytes=%u pumps=%u flushes=%u frames=%u tail_bytes=%u\n",
			m_save_snapshot_serial, m_save_video_replay_length,
			m_save_video_replay_pump_count, replay_flushes, rebuilt_frames,
			unsigned(m_save_video_replay_length - (m_save_video_replay_pump_count
					? cdi_dvc::save_video_replay_pump_offset(
						m_save_video_replay_pump_events[m_save_video_replay_pump_count - 1])
					: 0U)));
	return true;
}

void cdi_dvc_device::save_state_restore_failed()
{
	m_save_snapshot_valid = false;
	m_fmv_playback_active = false;
	m_fmv_single_step_pending = false;
	m_fmv_decoder_enabled = false;

	m_dma_active = false;
	m_dma_for_fma = false;
	m_dma_transfer_words = 0;
	m_dma_first_word = 0;
	m_dma_last_word = 0;
	m_dma_req_callback(CLEAR_LINE);

	// Recreate both opaque PL_MPEG decoders in a known empty state.  The
	// parser resets also discard replay journals, PCM, decoded-picture queues,
	// and picture-event bookkeeping that cannot be trusted after a failed load.
	mpeg_parser_reset(MPEG_FMA);
	mpeg_parser_reset(MPEG_FMV);
	video_presentation_reset();

	update_interrupt_state();
	update_timer();
}

void cdi_dvc_device::save_state_postload()
{
	bool snapshot_layout_valid =
		cdi_dvc::save_replay_fits(m_save_audio_replay_length, cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY)
		&& cdi_dvc::save_replay_fits(m_save_video_replay_length, cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY)
		&& cdi_dvc::save_audio_pcm_fits(m_save_audio_pcm_values)
		&& cdi_dvc::save_video_queue_fits(m_save_video_queue_count)
		&& cdi_dvc::save_picture_events_fit(m_save_picture_event_count)
		&& cdi_dvc::save_video_replay_pumps_fit(m_save_video_replay_pump_count)
		&& m_save_video_present_pixel_count <= cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME;

	if (snapshot_layout_valid)
	{
		snapshot_layout_valid = cdi_dvc::video_present_frame_fits(
				m_video_present_valid, m_video_present_width,
				m_video_present_height, m_save_video_present_pixel_count);
	}

	if (snapshot_layout_valid)
	{
		for (std::size_t index = 0; index < m_save_video_queue_count; ++index)
		{
			std::size_t const pixels =
					std::size_t(m_save_video_queue_width[index])
					* std::size_t(m_save_video_queue_height[index]);
			if (!cdi_dvc::save_video_frame_fits(
					m_save_video_queue_width[index],
					m_save_video_queue_height[index],
					pixels))
			{
				snapshot_layout_valid = false;
				break;
			}
		}
	}

	if (snapshot_layout_valid)
	{
		std::size_t previous_pump_offset = 0;
		uint32_t previous_pump_frames = 0;
		for (uint32_t event_index = 0; event_index < m_save_video_replay_pump_count; ++event_index)
		{
			uint64_t const event = m_save_video_replay_pump_events[event_index];
			std::size_t const offset = cdi_dvc::save_video_replay_pump_offset(
				event);
			uint32_t const frames = cdi_dvc::save_video_replay_pump_frames(event);
			if (!cdi_dvc::save_video_replay_pump_offset_valid(
					previous_pump_offset, offset, m_save_video_replay_length)
					|| frames < previous_pump_frames || frames > m_video_decoded_frames)
			{
				snapshot_layout_valid = false;
				break;
			}
			previous_pump_offset = offset;
			previous_pump_frames = frames;
		}
	}

	if (!m_save_snapshot_valid || !snapshot_layout_valid)
	{
		logerror("DVC_SAVE_STATE_RESTORE_UNSUPPORTED serial=%u reason=invalid_snapshot_layout audio_replay=%u video_replay=%u pcm_values=%u queue=%u present_pixels=%u picture_events=%u\n",
				m_save_snapshot_serial,
				m_save_audio_replay_length,
				m_save_video_replay_length,
				m_save_audio_pcm_values,
				m_save_video_queue_count,
				m_save_video_present_pixel_count,
				m_save_picture_event_count);
		save_state_restore_failed();
		return;
	}

	m_audio_replay_journal.assign(
			m_save_audio_replay.get(), m_save_audio_replay.get() + m_save_audio_replay_length);
	m_video_replay_journal.assign(
			m_save_video_replay.get(), m_save_video_replay.get() + m_save_video_replay_length);
	m_video_replay_pump_events.assign(
			m_save_video_replay_pump_events.begin(),
			m_save_video_replay_pump_events.begin() + m_save_video_replay_pump_count);
	m_audio_pcm_queue.assign(
			m_save_audio_pcm.get(), m_save_audio_pcm.get() + m_save_audio_pcm_values);
	m_audio_pcm_read = 0;

	m_video_queue.clear();
	for (std::size_t index = 0; index < m_save_video_queue_count; ++index)
	{
		queued_video_frame frame;
		frame.width = m_save_video_queue_width[index];
		frame.height = m_save_video_queue_height[index];
		frame.generation = m_save_video_queue_generation[index];
		frame.interrupts = m_save_video_queue_interrupts[index];
		frame.timestamp90 = m_save_video_queue_timestamp90[index];
		frame.timestamp_valid = m_save_video_queue_timestamp_valid[index] != 0;
		std::size_t const pixels = std::size_t(frame.width) * std::size_t(frame.height);
		uint32_t const *const base = m_save_video_queue_pixels.get()
				+ index * cdi_dvc::SAVE_VIDEO_PIXELS_PER_FRAME;
		frame.pixels.assign(base, base + pixels);
		m_video_queue.push_back(std::move(frame));
	}

	m_video_present_frame.assign(
			m_save_video_present_pixels.get(),
			m_save_video_present_pixels.get() + m_save_video_present_pixel_count);
	m_video_picture_event_queue.assign(
			m_save_picture_events.begin(),
			m_save_picture_events.begin() + m_save_picture_event_count);
	m_video_picture_event_read = 0;
	m_video_rgb24.clear();

	bool const audio_ok = save_state_rebuild_audio_decoder();
	bool const video_ok = save_state_rebuild_video_decoder();
	if (!audio_ok || !video_ok)
	{
		logerror("DVC_SAVE_STATE_RESTORE_UNSUPPORTED serial=%u reason=decoder_replay audio_ok=%u video_ok=%u audio_frames=%u video_frames=%u\n",
				m_save_snapshot_serial, audio_ok ? 1U : 0U, video_ok ? 1U : 0U,
				m_audio_decoded_frames, m_video_decoded_frames);
		save_state_restore_failed();
		return;
	}

	if (m_audio_stream)
		m_audio_stream->set_sample_rate(m_audio_output_rate ? m_audio_output_rate : 48'000);
	update_interrupt_state();
	update_timer();

	logerror("DVC_SAVE_STATE_RESTORE_OK serial=%u audio_replay=%u video_replay=%u pcm_values=%u queue=%u present_pixels=%u audio_frames=%u video_frames=%u\n",
			m_save_snapshot_serial, m_save_audio_replay_length, m_save_video_replay_length,
			m_save_audio_pcm_values, m_save_video_queue_count,
			m_save_video_present_pixel_count, m_audio_decoded_frames, m_video_decoded_frames);
}

void cdi_dvc_device::device_stop()
{
	logerror("DVC_TIMESTAMP_MARKER_TELEMETRY fma=%u fmv=%u scr_fma=%u pts_fma=%u dts_fma=%u scr_fmv=%u pts_fmv=%u dts_fmv=%u\n",
			m_mpeg_scr_marker_errors[MPEG_FMA] + m_mpeg_pts_marker_errors[MPEG_FMA] + m_mpeg_dts_marker_errors[MPEG_FMA],
			m_mpeg_scr_marker_errors[MPEG_FMV] + m_mpeg_pts_marker_errors[MPEG_FMV] + m_mpeg_dts_marker_errors[MPEG_FMV],
			m_mpeg_scr_marker_errors[MPEG_FMA], m_mpeg_pts_marker_errors[MPEG_FMA], m_mpeg_dts_marker_errors[MPEG_FMA],
			m_mpeg_scr_marker_errors[MPEG_FMV], m_mpeg_pts_marker_errors[MPEG_FMV], m_mpeg_dts_marker_errors[MPEG_FMV]);
	logerror("DVC_TOP_DISPLAY_ATTRIBUTION total_overlay_pixels=%llu top64_overlay_pixels=%llu top64_rows=64\n",
			(unsigned long long)m_video_overlay_total_pixels,
			(unsigned long long)m_video_overlay_top64_pixels);
	logerror("DVC_DSP_BOOTSTRAP_TELEMETRY e03022_count=%u e03022_last=%04x e03024_count=%u e03024_last=%04x e0406c_count=%u e0406c_last=%04x e0406e_count=%u e0406e_last=%04x e04070_count=%u e04070_last=%04x e04072_count=%u e04072_last=%04x\n",
			m_dsp_bootstrap_write_count[0], m_dsp_bootstrap_last[0],
			m_dsp_bootstrap_write_count[1], m_dsp_bootstrap_last[1],
			m_dsp_bootstrap_write_count[2], m_dsp_bootstrap_last[2],
			m_dsp_bootstrap_write_count[3], m_dsp_bootstrap_last[3],
			m_dsp_bootstrap_write_count[4], m_dsp_bootstrap_last[4],
			m_dsp_bootstrap_write_count[5], m_dsp_bootstrap_last[5]);
	logerror("DVC_AV_CLOCK_TELEMETRY observations=%llu last_delta90=%lld last_delta_us=%lld max_abs_delta90=%llu max_abs_delta_us=%lld audio_pts_valid=%u audio_pts=%llu video_pts_valid=%u video_pts=%llu\n",
			(unsigned long long)m_av_cross_observations,
			(long long)m_av_last_cross_delta90,
			(long long)cdi_dvc::clock90_delta_microseconds(m_av_last_cross_delta90),
			(unsigned long long)m_av_max_abs_cross_delta90,
			(long long)cdi_dvc::clock90_delta_microseconds(int64_t(m_av_max_abs_cross_delta90)),
			m_av_audio_pts_valid ? 1U : 0U, (unsigned long long)m_av_last_audio_pts90,
			m_av_video_pts_valid ? 1U : 0U, (unsigned long long)m_av_last_video_pts90);
	logerror("DVC_PRESENTATION_QUEUE_TELEMETRY decoded=%llu presented=%llu due_superseded=%llu flush_dropped=%llu waits=%llu fallback=%llu clocked=%llu total_late90=%llu max_late90=%llu compat_events=%llu max_depth=%llu vblanks=%llu queued=%u\n",
			(unsigned long long)m_scheduler_decoded_frames,
			(unsigned long long)m_scheduler_presented_frames,
			(unsigned long long)m_scheduler_due_superseded,
			(unsigned long long)m_scheduler_flush_dropped,
			(unsigned long long)m_scheduler_wait_vblanks,
			(unsigned long long)m_scheduler_fallback_presented,
			(unsigned long long)m_scheduler_clocked_presented,
			(unsigned long long)m_scheduler_total_late90,
			(unsigned long long)m_scheduler_max_late90,
			(unsigned long long)m_scheduler_compat_frame_events,
			(unsigned long long)m_scheduler_max_queue_depth,
			(unsigned long long)m_scheduler_vblanks,
			unsigned(m_video_queue.size()));
	audio_decoder_destroy();
	video_decoder_destroy();
}

void cdi_dvc_device::device_reset()
{
	mpeg_ram_compat_reset();

	video_presentation_reset();
	m_scheduler_decoded_frames = 0;
	m_scheduler_presented_frames = 0;
	m_scheduler_due_superseded = 0;
	m_scheduler_flush_dropped = 0;
	m_scheduler_wait_vblanks = 0;
	m_scheduler_fallback_presented = 0;
	m_scheduler_clocked_presented = 0;
	m_scheduler_total_late90 = 0;
	m_scheduler_max_late90 = 0;
	m_scheduler_compat_frame_events = 0;
	m_scheduler_max_queue_depth = 0;
	m_scheduler_vblanks = 0;
	m_dclk_epoch_ticks = machine().time().as_ticks(45'000);
	m_fmv_dclk_offset = 0;
	m_fma_dclk_latch = 0;

	m_vcd_control = 0;

	m_fma_command = 0;
	m_fma_status = 0;
	m_fma_stream = 0;
	m_fma_interrupt_vector = 0;
	m_fma_interrupt_status = 0;
	m_fma_interrupt_enable = 0;

	m_fmv_interrupt_enable = 0;
	m_fmv_interrupt_status = 0;
	m_fmv_timer_compare = FMV_TIMER_COMPARE_RESET_COMPAT_VALUE;
	m_fmv_system_command = 0;
	m_fmv_system_control = 0;
	m_fmv_video_command = 0;
	m_fmv_video_data_input_command = 0;
	m_fmv_playback_active = false;
	m_fmv_single_step_pending = false;
	m_fmv_decoder_enabled = false;
	m_fmv_stream = 0;
	m_fmv_interrupt_vector = 0;

	m_fmv_transfer_word = 0;
	m_fmv_transfer_words = 0;

	m_dma_active = false;
	m_dma_for_fma = false;
	m_dma_transfer_words = 0;
	m_dma_first_word = 0;
	m_dma_last_word = 0;

	m_video_overlay_total_pixels = 0;
	m_video_overlay_top64_pixels = 0;
	m_dsp_bootstrap_write_count.fill(0);
	m_dsp_bootstrap_last.fill(0);
	m_av_last_audio_pts90 = 0;
	m_av_last_video_pts90 = 0;
	m_av_audio_pts_valid = false;
	m_av_video_pts_valid = false;
	m_av_last_cross_delta90 = 0;
	m_av_max_abs_cross_delta90 = 0;
	m_av_cross_observations = 0;
	m_mpeg_scr_raw.fill(0);
	m_mpeg_pts_raw.fill(0);
	m_mpeg_dts_raw.fill(0);
	m_mpeg_scr_marker_errors.fill(0);
	m_mpeg_pts_marker_errors.fill(0);
	m_mpeg_dts_marker_errors.fill(0);

	m_intreq_callback(CLEAR_LINE);
	m_dma_req_callback(CLEAR_LINE);
	update_timer();

	m_mpeg_clock90 = 0;
	m_mpeg_clock_valid = false;
	mpeg_parser_reset(MPEG_FMA);
	mpeg_parser_reset(MPEG_FMV);
}

uint32_t cdi_dvc_device::current_fma_dclk()
{
	const uint64_t now = machine().time().as_ticks(45'000);
	return uint32_t(now - m_dclk_epoch_ticks);
}

uint32_t cdi_dvc_device::current_fmv_dclk()
{
	return current_fma_dclk() + m_fmv_dclk_offset;
}

uint64_t cdi_dvc_device::current_mpeg_clock90(unsigned target)
{
	if (target > MPEG_FMV || !m_mpeg_have_scr[target])
		return m_mpeg_clock90;

	uint32_t const current45 = target == MPEG_FMA
			? current_fma_dclk()
			: current_fmv_dclk();
	return cdi_dvc::mpeg_clock_from_dclk(
		m_mpeg_last_scr[target], m_mpeg_scr_dclk_anchor[target], current45);
}

void cdi_dvc_device::set_fmv_syscr(uint16_t data, uint16_t mem_mask)
{
	const uint32_t current = current_fmv_dclk();
	uint16_t syscr = uint16_t(current >> 6);

	COMBINE_DATA(&syscr);

	// GEN_SYSCR exposes/writes bits 21:6 of the FMV 45 kHz clock.
	const uint32_t desired =
		(current & ~0x003fffc0U) | (uint32_t(syscr) << 6);

	m_fmv_dclk_offset += desired - current;
}

void cdi_dvc_device::update_timer()
{
	// VMPEG's timer source is based on a 45 kHz clock. The observed compare
	// behavior produces an event after (TCNT + 1) * 8 of those ticks.
	const uint64_t ticks = (uint64_t(m_fmv_timer_compare) + 1U) * 8U;
	const attotime period = attotime::from_ticks(ticks, 45'000);
	m_interrupt_timer->adjust(period, 0, period);
}

TIMER_CALLBACK_MEMBER(cdi_dvc_device::timer_tick)
{
	// FMA POLL and FMV TIM are the same periodic VMPEG timer event.
	m_fma_interrupt_status |= cdi_dvc::FMA_IRQ_TIMER;
	m_fmv_interrupt_status |= cdi_dvc::FMV_IRQ_TIMER;
	update_interrupt_state();
}

void cdi_dvc_device::update_interrupt_state()
{
	const bool fma_pending = (m_fma_interrupt_status & m_fma_interrupt_enable) != 0;
	const bool fmv_pending = (m_fmv_interrupt_status & m_fmv_interrupt_enable) != 0;
	m_intreq_callback((fma_pending || fmv_pending) ? ASSERT_LINE : CLEAR_LINE);
}

uint8_t cdi_dvc_device::intack_r()
{
	if (m_fma_interrupt_status & m_fma_interrupt_enable)
	{
		const uint8_t vector = uint8_t(m_fma_interrupt_vector & 0x00ff);
		LOGMASKED(LOG_IRQ, "%s: VMPEG FMA interrupt acknowledge -> %02x\n", machine().describe_context(), vector);
		return vector;
	}

	if (m_fmv_interrupt_status & m_fmv_interrupt_enable)
	{
		const uint8_t vector = uint8_t((m_fmv_interrupt_vector >> 3) & 0x00ff);
		LOGMASKED(LOG_IRQ, "%s: VMPEG FMV interrupt acknowledge -> %02x\n", machine().describe_context(), vector);
		return vector;
	}

	return IDLE_IACK_COMPAT_VECTOR;
}

uint16_t cdi_dvc_device::read(offs_t offset, uint16_t mem_mask)
{
	const uint32_t address = 0xe00000U + (uint32_t(offset) << 1);

	// PROVISIONAL COMPATIBILITY MECHANISM: the firmware-visible E03018 bit 0
	// is modeled as readable state. Its actual hardware meaning remains unknown.
	if (address == 0xe03018U)
		return m_mpeg_ram_compat_visible ? 0x0001U : 0x0000U;
	uint16_t result = 0;

	switch (address)
	{
	case 0xe03000:
		result = m_fma_command;
		break;
	case 0xe03002:
		result = FMA_STATUS_COMPAT_FIXED_BITS | (m_fma_status & 0x00ff);
		break;
	case 0xe03004:
		result = FMA_E03004_COMPAT_READ_VALUE;
		break;
	case 0xe03006:
		result = FMA_E03006_COMPAT_READ_VALUE;
		break;
	case 0xe03008:
	case 0xe0300a:
		result = m_fma_stream & cdi_dvc::mpeg_stream_number_mask(true);
		break;
	case 0xe0300c:
		result = m_fma_interrupt_vector;
		break;
	case 0xe0300e:
		result = FMA_E0300E_COMPAT_READ_VALUE;
		break;
	case 0xe03010:
	{
		const uint32_t dclk = current_fma_dclk();
		result = uint16_t(dclk >> 16);

		if (!machine().side_effects_disabled())
			m_fma_dclk_latch = uint16_t(dclk);

		break;
	}

	case 0xe03012:
		result = m_fma_dclk_latch;
		break;

	case 0xe0301a:
		result = m_fma_interrupt_status;
		if (!machine().side_effects_disabled())
		{
			m_fma_interrupt_status = 0;
			update_interrupt_state();
		}
		break;
	case 0xe0301c:
		result = m_fma_interrupt_enable;
		break;
	case 0xe03024:
		result = FMA_E03024_COMPAT_READ_VALUE;
		break;

	case 0xe0405e:
	{
		size_t const buffered_video_bytes = m_video_buffer
				? plm_buffer_get_remaining(m_video_buffer) : 0;
		result = cdi_dvc::fmv_input_status_from_backend(
				buffered_video_bytes,
				m_video_decoder_waiting_for_input);
		if (!machine().side_effects_disabled())
		{
			LOGMASKED(LOG_SEQUENCE,
					"DVC_FMV_TRACE status-read value=%04x buffered=%u dma=%u words=%u queue=%u decoded=%u presented=%u ctx=%s\n",
					result, unsigned(buffered_video_bytes),
					m_dma_active && !m_dma_for_fma ? 1U : 0U,
					m_dma_transfer_words, unsigned(m_video_queue.size()),
					m_video_decoded_frames, m_video_present_generation,
					machine().describe_context());
		}
		break;
	}
	case 0xe04060:
		result = m_fmv_interrupt_enable;
		break;
	case 0xe04062:
		result = m_fmv_interrupt_status;
		if (!machine().side_effects_disabled())
		{
			LOGMASKED(LOG_SEQUENCE,
					"DVC_FMV_TRACE irq-read status=%04x enable=%04x queue=%u last_pending=%u last_generation=%u ctx=%s\n",
					result, m_fmv_interrupt_enable, unsigned(m_video_queue.size()),
					m_video_last_picture_pending ? 1U : 0U,
					m_video_last_picture_generation, machine().describe_context());
			m_fmv_interrupt_status = 0;
			update_interrupt_state();
		}
		break;
	case 0xe04064:
		result = m_fmv_timer_compare;
		break;
	case 0xe0408c:
	{
		result = m_fmv_video_data_input_command;
		if (!machine().side_effects_disabled())
		{
			LOGMASKED(LOG_SEQUENCE,
					"DVC_FMV_TRACE vdi-read value=%04x bit14=%u ctx=%s\n",
					result,
					(result & cdi_dvc::FMV_VDI_DECODING_TIMESTAMP_UPDATED) ? 1U : 0U,
					machine().describe_context());
		}
		break;
	}
	case 0xe04098:
		result = uint16_t(current_fmv_dclk() >> 6);
		break;

	case 0xe0409c:
		result = 0;
		break;
	case 0xe0409e:
		result = 1;
		break;
	case 0xe040a0:
	{
		result = cdi_dvc::fmv_reduced_decoding_timestamp(
				m_mpeg_packet_decode_ts[MPEG_FMV]);

		if (!machine().side_effects_disabled())
		{
			LOGMASKED(LOG_SEQUENCE,
					"DVC_FMV_TRACE a0-read value=%04x ts90=%llu vdi=%04x presented=%u hostq=%u ctx=%s\n",
					result,
					(unsigned long long)m_mpeg_packet_decode_ts[MPEG_FMV],
					m_fmv_video_data_input_command,
					m_video_present_generation,
					unsigned(m_video_queue.size()),
					machine().describe_context());
		}
		break;
	}
	case 0xe040a4:
	{
		/*
		 * Guest-visible GEN_PICTURES_IN_FIFO must not expose the host
		 * decode-ahead queue directly.
		 *
		 * MiSTer's VMPEG model counts:
		 *
		 *   input pictures + output-FIFO pictures
		 *
		 * while deliberately excluding pictures held inside the MPEG
		 * decoder for reference-frame reordering.
		 *
		 * Stock PL_MPEG has the same distinction.  A decoded reference
		 * picture can be held internally without yet being returned by
		 * plm_video_decode(), so picture_headers - decoded_frames alone
		 * over-counts the compressed-input backlog.
		 *
		 * MAME also keeps a larger host presentation queue so PL_MPEG can
		 * decode ahead without deadlocking at the VMPEG compressed-input
		 * high-water mark.  That host staging depth is not guest-visible
		 * VMPEG output-FIFO capacity.
		 */
		std::size_t const decoder_held_pictures =
				(m_video_decoder && m_video_decoder->has_reference_frame)
						? 1U
						: 0U;

		std::size_t const decoder_consumed_pictures =
				std::size_t(m_video_decoded_frames) + decoder_held_pictures;

		std::size_t const input_pictures =
				std::size_t(m_video_picture_headers) > decoder_consumed_pictures
						? std::size_t(m_video_picture_headers) - decoder_consumed_pictures
						: 0U;

		std::size_t const output_pictures =
				std::min<std::size_t>(
						m_video_queue.size(),
						cdi_dvc::FMV_OUTPUT_FIFO_PICTURES);

		/*
		 * Before PLAY, retain the existing predecode approximation for
		 * this A/B.  MiSTer uses its per-picture DTS FIFO here; MAME does
		 * not yet maintain an equivalent independent queue.
		 *
		 * During PLAY, use the separated guest-visible model.
		 */
		std::size_t const guest_pictures = m_fmv_playback_active
				? input_pictures + output_pictures
				: std::size_t(m_video_picture_headers);

		result = cdi_dvc::fmv_pictures_in_fifo(guest_pictures);

		if (!machine().side_effects_disabled())
		{
			LOGMASKED(LOG_SEQUENCE,
					"DVC_FMV_TRACE a4-read value=%04x guest=%u input=%u held=%u headers=%u decoded=%u hostq=%u output=%u presented=%u active=%u ctx=%s\n",
					result,
					unsigned(guest_pictures),
					unsigned(input_pictures),
					unsigned(decoder_held_pictures),
					m_video_picture_headers,
					m_video_decoded_frames,
					unsigned(m_video_queue.size()),
					unsigned(output_pictures),
					m_video_present_generation,
					m_fmv_playback_active ? 1U : 0U,
					machine().describe_context());
		}
		break;
	}
	case 0xe040a8:
		result = cdi_dvc::fmv_frame_period_90khz(m_video_framerate_millihz);
		break;
	case 0xe04074: return m_video_screen_y_shadow;
	case 0xe04076: return m_video_screen_x_shadow;
	case 0xe04078: return m_video_window_h_shadow;
	case 0xe0407a: return m_video_window_w_shadow;
	case 0xe0407c: return m_video_crop_y_shadow;
	case 0xe0407e: return m_video_crop_x_shadow;
	case 0xe040c0:
		result = m_fmv_system_command;
		break;
	case 0xe040c2:
		result = m_fmv_video_command;
		break;
	case 0xe040c4:
		result = m_fmv_stream & 0x000f;
		break;
	case 0xe040dc:
		result = m_fmv_interrupt_vector;
		break;
	case 0xe040e6:
		result = 0;
		break;
	default:
		result = 0;
		break;
	}

	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_REGISTERS, "%s: VMPEG read  %08x -> %04x & %04x\n",
				machine().describe_context(), address, result, mem_mask);
	}

	return result;
}

void cdi_dvc_device::audio_output_reset()
{
	if (m_audio_stream)
		m_audio_stream->update();

	uint32_t const dropped = m_audio_pcm_read < m_audio_pcm_queue.size()
		? uint32_t((m_audio_pcm_queue.size() - m_audio_pcm_read) / 2)
		: 0;
	if (dropped || m_audio_wait_samples)
	{
		LOGMASKED(LOG_AUDIO,
				"%s: DVC AUDIO output reset dropped=%u wait=%llu emitted=%u fnv=%08x\n",
				machine().describe_context(), dropped,
				(unsigned long long)m_audio_wait_samples,
				m_audio_output_frames, m_audio_output_hash);
	}

	m_audio_pcm_queue.clear();
	m_audio_pcm_read = 0;
	m_audio_wait_samples = 0;
	m_audio_silence_frames = 0;
	m_audio_output_frames = 0;
	m_audio_output_nonzero = 0;
	m_audio_output_hash = 2166136261U;
	m_audio_queue_events = 0;
	m_audio_starvation_frames = 0;
	m_audio_starvation_events = 0;
	m_audio_output_started = false;
	m_audio_starved = false;
}

void cdi_dvc_device::audio_output_set_rate(uint32_t rate)
{
	if (!rate || rate == m_audio_output_rate)
		return;

	if (m_audio_stream)
	{
		m_audio_stream->update();
		m_audio_stream->set_sample_rate(rate);
	}
	m_audio_output_rate = rate;
	LOGMASKED(LOG_AUDIO, "%s: DVC AUDIO output rate=%u\n",
			machine().describe_context(), m_audio_output_rate);
}

void cdi_dvc_device::sound_stream_update(sound_stream &stream)
{
	for (int i = 0; i < stream.samples(); ++i)
	{
		cdi_dvc::audio_output_frame const output =
			cdi_dvc::take_audio_output_frame(
				m_audio_pcm_queue, m_audio_pcm_read, m_audio_wait_samples);
		bool const have_pcm = output.kind == cdi_dvc::audio_output_kind::pcm;

		if (output.kind == cdi_dvc::audio_output_kind::scheduled_silence)
		{
			++m_audio_silence_frames;
		}
		else if (have_pcm)
		{
			if (m_audio_starved)
			{
				m_audio_starved = false;
				LOGMASKED(LOG_AUDIO,
						"%s: DVC AUDIO refill after starvation events=%u frames=%llu pending=%u\n",
						machine().describe_context(), m_audio_starvation_events,
						(unsigned long long)m_audio_starvation_frames,
						unsigned(output.pending_frames_before));
			}
			if (!m_audio_output_started)
			{
				m_audio_output_started = true;
				LOGMASKED(LOG_AUDIO,
						"%s: DVC AUDIO output start rate=%u silence=%llu pending=%u\n",
						machine().describe_context(), m_audio_output_rate,
						(unsigned long long)m_audio_silence_frames,
						unsigned(output.pending_frames_before));
			}

			m_audio_output_hash = cdi_dvc::hash_pcm16_sample(
					m_audio_output_hash, output.left);
			m_audio_output_hash = cdi_dvc::hash_pcm16_sample(
					m_audio_output_hash, output.right);
			++m_audio_output_frames;
			if (output.left != 0 || output.right != 0)
				++m_audio_output_nonzero;
		}
		else if (m_audio_output_started)
		{
			++m_audio_starvation_frames;
			if (!m_audio_starved)
			{
				m_audio_starved = true;
				++m_audio_starvation_events;
				LOGMASKED(LOG_AUDIO,
						"%s: DVC AUDIO starvation event=%u emitted=%u queue_events=%u\n",
						machine().describe_context(), m_audio_starvation_events,
						m_audio_output_frames, m_audio_queue_events);
			}
		}

		stream.put_int(0, i, output.left, 32768);
		stream.put_int(1, i, output.right, 32768);

		if (have_pcm && output.drained)
		{
			LOGMASKED(LOG_AUDIO,
					"%s: DVC AUDIO output drain frames=%u nonzero=%u silence=%llu starvation=%llu fnv=%08x events=%u\n",
					machine().describe_context(), m_audio_output_frames,
					m_audio_output_nonzero, (unsigned long long)m_audio_silence_frames,
					(unsigned long long)m_audio_starvation_frames,
					m_audio_output_hash, m_audio_queue_events);
		}
	}
}

void cdi_dvc_device::audio_decoder_destroy()
{
	if (m_audio_decoder)
	{
		plm_audio_destroy(m_audio_decoder);
		m_audio_decoder = nullptr;
		m_audio_buffer = nullptr;
	}
	else if (m_audio_buffer)
	{
		plm_buffer_destroy(m_audio_buffer);
		m_audio_buffer = nullptr;
	}
}

void cdi_dvc_device::audio_decoder_reset()
{
	audio_output_reset();
	audio_decoder_destroy();
	m_audio_replay_journal.clear();
	m_audio_replay_overflow = false;

	m_audio_header_shift = 0;
	m_audio_decoded_frames = 0;
	m_audio_decoded_samples = 0;
	m_audio_header_events = 0;
	m_audio_decode_events = 0;
	m_audio_profile_violations = 0;
	m_audio_bitrate_kbps = 0;
	m_audio_samplerate = 0;
	m_audio_channel_mode = 0;
	m_audio_backend_status = 0;
	m_audio_have_es_header = false;
	m_audio_have_header = false;
	m_audio_decoder_end_signalled = false;
	m_audio_backend_ended = false;

	m_audio_buffer = plm_buffer_create_with_capacity(128 * 1024);
	m_audio_decoder = plm_audio_create_with_buffer(m_audio_buffer, 1);
}

void cdi_dvc_device::audio_decoder_feed(uint8_t data)
{
	// A write reopens PL_MPEG's dynamic ring after signal_end().  Mirror that
	// otherwise-opaque transition so a later save reconstructs it exactly.
	m_audio_decoder_end_signalled = false;
	m_audio_backend_ended = false;

	if (m_audio_replay_journal.size() < cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY)
	{
		m_audio_replay_journal.push_back(data);
	}
	else if (!m_audio_replay_overflow)
	{
		m_audio_replay_overflow = true;
		logerror("DVC_SAVE_STATE_AUDIO_REPLAY_OVERFLOW capacity=%u\n",
				unsigned(cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY));
	}

	m_audio_header_shift = (m_audio_header_shift << 8) | data;

	if (!m_audio_have_es_header)
	{
		cdi_dvc::mpeg1_layer2_audio_header const header =
			cdi_dvc::decode_mpeg1_layer2_audio_header(m_audio_header_shift);
		if (header.valid)
		{
			m_audio_have_es_header = true;
			m_audio_bitrate_kbps = header.bitrate_kbps;
			m_audio_samplerate = header.sample_rate_hz;
			m_audio_channel_mode = header.channel_mode;
			m_audio_backend_status |= 0x01;
			uint8_t const profile_violations =
				cdi_dvc::cdi_full_motion_layer2_profile_violations(header);
			if (profile_violations)
			{
				m_audio_backend_status |= 0x08;
				++m_audio_profile_violations;
				LOGMASKED(LOG_AUDIO,
						"%s: DVC AUDIO Green Book profile violation flags=%02x bitrate=%u rate=%u mode=%u private=%u emphasis=%u event=%u\n",
						machine().describe_context(), profile_violations,
						header.bitrate_kbps, header.sample_rate_hz,
						header.channel_mode, header.private_bit ? 1U : 0U,
						header.emphasis, m_audio_profile_violations);
			}
			++m_audio_header_events;
			LOGMASKED(LOG_AUDIO, "%s: DVC AUDIO ES header bitrate=%u rate=%u mode=%u status=%02x event=%u\n",
				machine().describe_context(), m_audio_bitrate_kbps, m_audio_samplerate,
				m_audio_channel_mode, m_audio_backend_status, m_audio_header_events);
		}
	}

	if (!m_audio_buffer || !m_audio_decoder)
		return;

	plm_buffer_write(m_audio_buffer, &data, 1);

	if (!m_audio_have_header && plm_audio_has_header(m_audio_decoder))
	{
		m_audio_have_header = true;
		unsigned const backend_rate = unsigned(plm_audio_get_samplerate(m_audio_decoder));
		audio_output_set_rate(backend_rate);
		if (!m_audio_samplerate)
			m_audio_samplerate = backend_rate;
		m_audio_backend_status |= 0x02;
		m_fma_status |= cdi_dvc::FMA_IRQ_DECODING_STARTED;
		m_fma_interrupt_status |= cdi_dvc::FMA_IRQ_DECODING_STARTED;
		update_interrupt_state();
		++m_audio_header_events;
		LOGMASKED(LOG_AUDIO, "%s: DVC AUDIO backend header rate=%u status=%02x event=%u\n",
				machine().describe_context(), backend_rate,
				m_audio_backend_status, m_audio_header_events);
	}
}

void cdi_dvc_device::audio_decoder_pump()
{
	if (!m_audio_decoder)
		return;
	if (!m_audio_have_header)
	{
		m_audio_backend_ended = plm_audio_has_ended(m_audio_decoder) != 0;
		return;
	}

	// Synchronize the stream before changing its future sample queue. This is
	// the standard MAME sound-device rule for state changes that affect output.
	if (m_audio_stream)
		m_audio_stream->update();

	cdi_dvc::compact_consumed_audio_samples(m_audio_pcm_queue, m_audio_pcm_read);
	bool const queue_was_empty = m_audio_pcm_queue.empty();
	uint32_t const decoded_before = m_audio_decoded_samples;

	for (;;)
	{
		plm_samples_t *const samples = plm_audio_decode(m_audio_decoder);
		if (!samples)
			break;

		uint32_t hash = 2166136261U;
		size_t const values = size_t(samples->count) * 2;
		m_audio_pcm_queue.reserve(m_audio_pcm_queue.size() + values);
		for (size_t i = 0; i < values; ++i)
		{
			int16_t const pcm = cdi_dvc::quantize_plm_audio_sample(samples->interleaved[i]);
			hash = cdi_dvc::hash_pcm16_sample(hash, pcm);
			m_audio_pcm_queue.push_back(pcm);
		}

		++m_audio_decoded_frames;
		m_audio_decoded_samples += samples->count;
		++m_audio_decode_events;
		m_fma_status |= cdi_dvc::FMA_IRQ_FRAME_DECODED;
		m_fma_interrupt_status |= cdi_dvc::FMA_IRQ_FRAME_DECODED;
		update_interrupt_state();
		m_audio_backend_status |= 0x04;
		LOGMASKED(LOG_AUDIO, "%s: DVC AUDIO decoded frames=%u frame_samples=%u total_samples=%u event=%u status=%02x fnv=%08x\n",
				machine().describe_context(), m_audio_decoded_frames, samples->count,
				m_audio_decoded_samples, m_audio_decode_events,
				m_audio_backend_status, hash);
	}
	m_audio_backend_ended = plm_audio_has_ended(m_audio_decoder) != 0;

	uint32_t const added_frames = m_audio_decoded_samples - decoded_before;
	if (!added_frames)
		return;

	if (queue_was_empty)
	{
		m_audio_wait_samples = 0;
		if (m_mpeg_schedule_valid[MPEG_FMA] && m_mpeg_schedule_play_delta45[MPEG_FMA] > 0)
		{
			uint64_t const ticks45 = uint64_t(uint32_t(m_mpeg_schedule_play_delta45[MPEG_FMA]));
			m_audio_wait_samples = (ticks45 * m_audio_output_rate + 44999U) / 45000U;
		}
	}

	++m_audio_queue_events;
	if (m_mpeg_packet_have_pts[MPEG_FMA])
	{
		m_av_last_audio_pts90 = m_mpeg_packet_pts[MPEG_FMA];
		m_av_audio_pts_valid = true;
		av_clock_observe();
	}
	uint32_t const pending = uint32_t((m_audio_pcm_queue.size() - m_audio_pcm_read) / 2);
	LOGMASKED(LOG_AUDIO,
			"%s: DVC AUDIO queue event=%u added=%u pending=%u rate=%u play45=%d wait=%llu\n",
			machine().describe_context(), m_audio_queue_events, added_frames, pending,
			m_audio_output_rate,
			m_mpeg_schedule_valid[MPEG_FMA] ? m_mpeg_schedule_play_delta45[MPEG_FMA] : 0,
			(unsigned long long)m_audio_wait_samples);
}

void cdi_dvc_device::audio_decoder_flush()
{
	if (!m_audio_buffer || !m_audio_decoder)
		return;

	plm_buffer_signal_end(m_audio_buffer);
	m_audio_decoder_end_signalled = true;
	audio_decoder_pump();
}


void cdi_dvc_device::video_overlay_reset()
{
	m_video_overlay_hash = 2166136261U;
	m_video_overlay_pixels = 0;
	m_video_overlay_complete = false;
}

void cdi_dvc_device::video_frame_clear()
{
	m_video_rgb24.clear();
	m_scheduler_flush_dropped += m_video_queue.size();
	m_video_queue.clear();
	m_video_pts_anchor90 = 0;
	m_video_backend_anchor90 = 0;
	m_video_pts_anchor_valid = false;
	m_video_present_frame.clear();
	m_video_present_width = 0;
	m_video_present_height = 0;
	m_video_present_generation = 0;
	m_video_present_valid = false;
	video_overlay_reset();
}

void cdi_dvc_device::video_presentation_reset()
{
	video_frame_clear();
	m_video_screen_y_shadow = 0;
	m_video_screen_x_shadow = 0;
	m_video_window_h_shadow = 0;
	m_video_window_w_shadow = 0;
	m_video_crop_y_shadow = 0;
	m_video_crop_x_shadow = 0;
	m_video_screen_y = 0;
	m_video_screen_x = 0;
	m_video_window_h = 0;
	m_video_window_w = 0;
	m_video_crop_y = 0;
	m_video_crop_x = 0;
	m_video_geometry_frame_pending = false;
	m_video_geometry_vblank_pending = false;
	m_video_visible = false;
	m_video_output_enabled = false;
	m_video_show_on_next = false;
}

void cdi_dvc_device::video_latch_frame()
{
	if ((!m_fmv_playback_active && !m_fmv_single_step_pending)
			|| m_video_queue.empty())
		return;

	bool const single_step = m_fmv_single_step_pending;
	std::size_t selected_index = 0;
	std::size_t consume_count = 1;
	bool timestamp_driven = false;
	uint64_t clock90 = 0;

	if (!single_step
			&& (m_fmv_system_control & 0x0004U)
			&& m_video_queue.front().timestamp_valid
			&& m_mpeg_have_scr[MPEG_FMV])
	{
		std::vector<uint64_t> timestamps90;
		timestamps90.reserve(m_video_queue.size());
		for (queued_video_frame const &queued : m_video_queue)
		{
			if (!queued.timestamp_valid)
				break;
			timestamps90.push_back(queued.timestamp90);
		}

		clock90 = current_mpeg_clock90(MPEG_FMV);
		cdi_dvc::presentation_selection const selection =
			cdi_dvc::select_latest_due_presentation(
				timestamps90.data(), timestamps90.size(), clock90);
		if (!selection.valid)
		{
			++m_scheduler_wait_vblanks;
			return;
		}

		selected_index = selection.selected_index;
		consume_count = selection.consume_count;
		timestamp_driven = true;
	}
	else if (!single_step)
	{
		// Compatibility fallback for streams that have not established both a
		// frame timestamp and an FMV SCR clock.  Preserve queued order and do not
		// reintroduce the old single-slot overwrite behavior.
		++m_scheduler_fallback_presented;
	}

	uint16_t marker_interrupts = 0;
	for (std::size_t index = 0; index < consume_count; ++index)
		marker_interrupts |= m_video_queue[index].interrupts;

	queued_video_frame selected = std::move(m_video_queue[selected_index]);
	for (std::size_t index = 0; index < consume_count; ++index)
		m_video_queue.pop_front();

	if (timestamp_driven && selected_index)
		m_scheduler_due_superseded += selected_index;
	if (timestamp_driven)
	{
		++m_scheduler_clocked_presented;
		int64_t const delta90 = cdi_dvc::mpeg_timestamp_delta(selected.timestamp90, clock90);
		uint64_t const late90 = delta90 < 0 ? uint64_t(-delta90) : 0;
		m_scheduler_total_late90 += late90;
		if (late90 > m_scheduler_max_late90)
			m_scheduler_max_late90 = late90;
	}

	++m_scheduler_presented_frames;
	m_video_present_frame = std::move(selected.pixels);
	m_video_present_width = selected.width;
	m_video_present_height = selected.height;
	m_video_present_generation = selected.generation;
	m_video_present_valid = true;
	if (selected.timestamp_valid)
	{
		m_av_last_video_pts90 = selected.timestamp90;
		m_av_video_pts_valid = true;
		av_clock_observe();
	}

	video_latch_geometry(false);
	if (m_video_show_on_next)
	{
		m_video_visible = true;
		m_video_show_on_next = false;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO show-next presented frame=%u\n",
				machine().describe_context(), m_video_present_generation);
	}

	cdi_dvc::presentation_picture_event const picture_event =
		cdi_dvc::make_presentation_picture_event(
			marker_interrupts, m_video_last_picture_pending,
			m_video_present_generation, m_video_last_picture_generation);
	if (picture_event.end_of_data)
	{
		m_video_last_picture_pending = false;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO presented last picture frame=%u\n",
				machine().describe_context(), m_video_present_generation);
	}
	++m_scheduler_compat_frame_events;
	m_fmv_interrupt_status |= picture_event.interrupts;
	update_interrupt_state();

	video_overlay_reset();

	LOGMASKED(LOG_VIDEO,
			"%s: DVC VIDEO presented frame=%u size=%ux%u pts_valid=%u pts=%llu clock=%llu queue=%u superseded=%u interrupts=%04x\n",
			machine().describe_context(), m_video_present_generation,
			m_video_present_width, m_video_present_height,
			selected.timestamp_valid ? 1U : 0U,
			(unsigned long long)selected.timestamp90,
			(unsigned long long)clock90,
			unsigned(m_video_queue.size()), unsigned(selected_index),
			picture_event.interrupts);

	if (single_step)
		m_fmv_single_step_pending = false;
	video_decoder_pump();
}

void cdi_dvc_device::video_latch_geometry(bool at_vblank)
{
	bool &pending = at_vblank ? m_video_geometry_vblank_pending : m_video_geometry_frame_pending;
	if (!pending)
		return;

	m_video_screen_y = m_video_screen_y_shadow;
	m_video_screen_x = m_video_screen_x_shadow;
	m_video_window_h = m_video_window_h_shadow;
	m_video_window_w = m_video_window_w_shadow;
	m_video_crop_y = m_video_crop_y_shadow;
	m_video_crop_x = m_video_crop_x_shadow;
	pending = false;
	video_overlay_reset();

	LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO geometry %s crop=%u,%u window=%ux%u screen=%u,%u\n",
			machine().describe_context(), at_vblank ? "vblank" : "frame",
			m_video_crop_x, m_video_crop_y, m_video_window_w, m_video_window_h,
			m_video_screen_x, m_video_screen_y);

	m_fmv_interrupt_status |= cdi_dvc::FMV_IRQ_GEOMETRY_LATCH;
	update_interrupt_state();
}

void cdi_dvc_device::video_vblank()
{
	++m_scheduler_vblanks;
	m_fmv_interrupt_status |= cdi_dvc::FMV_IRQ_VSYNC;
	update_interrupt_state();
	video_latch_geometry(true);
	video_latch_frame();
}

void cdi_dvc_device::video_overlay_scanline(uint32_t *pixels, unsigned pixel_count, int physical_y, int visible_top,
		int clip_min_x, int clip_max_x, bool const *external_video, unsigned external_count)
{
	if (!pixels || !external_video || !m_video_output_enabled || !m_video_visible || !m_video_present_valid)
		return;
	if (!cdi_dvc::video_present_frame_fits(
			true, m_video_present_width, m_video_present_height,
			m_video_present_frame.size()))
		return;
	if (!m_video_window_w || !m_video_window_h)
		return;
	if (m_video_crop_x >= m_video_present_width || m_video_crop_y >= m_video_present_height)
		return;

	unsigned window_w = m_video_window_w;
	unsigned window_h = m_video_window_h;
	unsigned const avail_w = unsigned(m_video_present_width - m_video_crop_x);
	unsigned const avail_h = unsigned(m_video_present_height - m_video_crop_y);
	if (window_w > avail_w)
		window_w = avail_w;
	if (window_h > avail_h)
		window_h = avail_h;
	if (!window_w || !window_h)
		return;

	cdi_dvc::video_present_geometry const geometry =
		cdi_dvc::current_video_present_geometry(
			m_video_screen_x, m_video_screen_y, visible_top, window_w, window_h);
	int const dst_x = geometry.dst_x;
	int const dst_y = geometry.dst_y;
	int const rel_y = physical_y - dst_y;
	if (rel_y < 0 || rel_y >= int(geometry.output_height))
		return;

	unsigned const src_y = unsigned(m_video_crop_y)
		+ unsigned(rel_y / int(cdi_dvc::VIDEO_PIXEL_Y_SCALE));
	for (unsigned x = 0; x < window_w; ++x)
	{
		unsigned const src_x = unsigned(m_video_crop_x) + x;
		uint32_t const color = m_video_present_frame[size_t(src_y) * m_video_present_width + src_x];
		for (unsigned repeat = 0; repeat < cdi_dvc::VIDEO_PIXEL_X_SCALE; ++repeat)
		{
			int const out_x = dst_x + int(x * cdi_dvc::VIDEO_PIXEL_X_SCALE + repeat);
			if (out_x < 0 || out_x >= int(pixel_count) || out_x >= int(external_count))
				continue;
			if (out_x < clip_min_x || out_x > clip_max_x || !external_video[out_x])
				continue;

			pixels[out_x] = color;
			uint8_t const r = uint8_t(color >> 16);
			uint8_t const g = uint8_t(color >> 8);
			uint8_t const b = uint8_t(color);
			m_video_overlay_hash ^= r;
			m_video_overlay_hash *= 16777619U;
			m_video_overlay_hash ^= g;
			m_video_overlay_hash *= 16777619U;
			m_video_overlay_hash ^= b;
			m_video_overlay_hash *= 16777619U;
			++m_video_overlay_pixels;
			++m_video_overlay_total_pixels;
			if (physical_y >= visible_top && physical_y < visible_top + 64)
				++m_video_overlay_top64_pixels;
		}
	}

	if (!m_video_overlay_complete && physical_y == dst_y + int(geometry.output_height) - 1 && m_video_overlay_pixels)
	{
		m_video_overlay_complete = true;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO overlay complete frame=%u crop=%u,%u window=%ux%u dst=%d,%d pixels=%u fnv=%08x\n",
				machine().describe_context(), m_video_present_generation,
				m_video_crop_x, m_video_crop_y, window_w, window_h,
				dst_x, dst_y, m_video_overlay_pixels, m_video_overlay_hash);
	}
}

void cdi_dvc_device::video_decoder_destroy()
{
	if (m_video_decoder)
	{
		plm_video_destroy(m_video_decoder);
		m_video_decoder = nullptr;
		m_video_buffer = nullptr;
	}
	else if (m_video_buffer)
	{
		plm_buffer_destroy(m_video_buffer);
		m_video_buffer = nullptr;
	}
}

void cdi_dvc_device::video_decoder_reset()
{
	LOGMASKED(LOG_SEQUENCE,
			"DVC_FMV_TRACE decoder-reset queue=%u decoded=%u presented=%u irq=%04x sequence_ends=%u dma=%u words=%u ctx=%s\n",
			unsigned(m_video_queue.size()), m_video_decoded_frames,
			m_video_present_generation, m_fmv_interrupt_status,
			m_video_sequence_end_events,
			m_dma_active && !m_dma_for_fma ? 1U : 0U, m_dma_transfer_words,
			machine().describe_context());

	video_frame_clear();
	video_decoder_destroy();
	m_video_replay_journal.clear();
	m_video_replay_pump_events.clear();
	m_video_replay_overflow = false;
	m_video_replay_pump_overflow = false;

	m_video_es_prefix = 0;
	m_video_sequence_headers = 0;
	m_video_gop_headers = 0;
	m_video_picture_headers = 0;
	m_video_picture_header_bytes = 0;
	m_video_picture_marker_interrupts = 0;
	m_video_reference_interrupts = 0;
	m_video_reference_valid = false;
	m_video_picture_event_queue.clear();
	m_video_picture_event_read = 0;
	m_video_decoded_frames = 0;
	m_video_width = 0;
	m_video_height = 0;
	m_video_framerate_millihz = 0;
	m_video_have_sequence = false;
	m_video_sequence_end_pending = false;
	m_video_decoder_flush_pending = false;
	m_video_decoder_waiting_for_input = false;
	m_video_sequence_end_events = 0;
	m_video_last_picture_generation = 0;
	m_video_last_picture_pending = false;

	m_video_buffer = plm_buffer_create_with_capacity(256 * 1024);
	m_video_decoder = plm_video_create_with_buffer(m_video_buffer, 1);
}

void cdi_dvc_device::video_decoder_feed(uint8_t data)
{
	if (m_video_replay_journal.size() < cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY)
	{
		m_video_replay_journal.push_back(data);
	}
	else if (!m_video_replay_overflow)
	{
		m_video_replay_overflow = true;
		logerror("DVC_SAVE_STATE_VIDEO_REPLAY_OVERFLOW capacity=%u\n",
				unsigned(cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY));
	}

	if (m_video_picture_header_bytes)
	{
		--m_video_picture_header_bytes;
		if (!m_video_picture_header_bytes)
			video_picture_event((data >> 3) & 0x07);
	}

	m_video_es_prefix = ((m_video_es_prefix << 8) | data) & 0xffffffffU;

	if (m_video_es_prefix == 0x000001b3U)
	{
		++m_video_sequence_headers;
		m_video_picture_marker_interrupts |= cdi_dvc::FMV_IRQ_SEQUENCE;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO ES sequence headers=%u\n",
				machine().describe_context(), m_video_sequence_headers);
	}
	else if (m_video_es_prefix == 0x000001b7U)
	{
		++m_video_sequence_end_events;
		video_picture_events_flush();
		m_video_sequence_end_pending = true;
		m_fmv_interrupt_status |= cdi_dvc::FMV_IRQ_END_SEQUENCE;
		update_interrupt_state();
		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE sequence-end event=%u queue=%u decoded=%u irq=%04x ctx=%s\n",
				m_video_sequence_end_events, unsigned(m_video_queue.size()),
				m_video_decoded_frames, m_fmv_interrupt_status,
				machine().describe_context());
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO ES sequence end events=%u\n",
				machine().describe_context(), m_video_sequence_end_events);
	}
	else if (m_video_es_prefix == 0x000001b8U)
	{
		++m_video_gop_headers;
		m_video_picture_marker_interrupts |= cdi_dvc::FMV_IRQ_GOP;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO ES GOP headers=%u\n",
				machine().describe_context(), m_video_gop_headers);
	}
	else if (m_video_es_prefix == 0x00000100U)
	{
		++m_video_picture_headers;
		m_video_picture_header_bytes = 2;
		// More picture data after a flushed sequence means the previously
		// queued picture was not the end of the complete presentation.
		if (!m_video_sequence_end_pending)
			m_video_last_picture_pending = false;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO ES picture headers=%u\n",
				machine().describe_context(), m_video_picture_headers);
	}

	if (!m_video_buffer || !m_video_decoder)
		return;

	plm_buffer_write(m_video_buffer, &data, 1);

	if (!m_video_have_sequence && plm_video_has_header(m_video_decoder))
	{
		m_video_have_sequence = true;
		m_video_width = uint16_t(plm_video_get_width(m_video_decoder));
		m_video_height = uint16_t(plm_video_get_height(m_video_decoder));
		m_video_framerate_millihz = uint32_t(plm_video_get_framerate(m_video_decoder) * 1000.0 + 0.5);

		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO backend sequence=%ux%u fps_milli=%u\n",
				machine().describe_context(),
				m_video_width, m_video_height, m_video_framerate_millihz);
	}
}

void cdi_dvc_device::video_picture_event(uint8_t picture_type)
{
	uint16_t picture_interrupts = 0;
	if (picture_type == 1)
	{
		picture_interrupts = m_video_picture_marker_interrupts;
		m_video_picture_marker_interrupts = 0;
	}

	cdi_dvc::picture_event_reorder_result const result = cdi_dvc::reorder_picture_events(
			{ m_video_reference_interrupts, m_video_reference_valid }, picture_type, picture_interrupts);
	m_video_reference_interrupts = result.state.reference_interrupts;
	m_video_reference_valid = result.state.reference_valid;
	if (result.output_valid)
		m_video_picture_event_queue.push_back(result.output_interrupts);
}

void cdi_dvc_device::video_picture_events_flush()
{
	cdi_dvc::picture_event_reorder_result const result = cdi_dvc::flush_picture_events(
			{ m_video_reference_interrupts, m_video_reference_valid });
	m_video_reference_interrupts = result.state.reference_interrupts;
	m_video_reference_valid = result.state.reference_valid;
	if (result.output_valid)
		m_video_picture_event_queue.push_back(result.output_interrupts);
}

uint16_t cdi_dvc_device::video_picture_events_pop()
{
	if (m_video_picture_event_read >= m_video_picture_event_queue.size())
	{
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO picture-event metadata underflow\n",
				machine().describe_context());
		return 0;
	}

	uint16_t const interrupts = m_video_picture_event_queue[m_video_picture_event_read++];
	if (m_video_picture_event_read == m_video_picture_event_queue.size())
	{
		m_video_picture_event_queue.clear();
		m_video_picture_event_read = 0;
	}
	return interrupts;
}

void cdi_dvc_device::video_decoder_pump(bool end_signalled)
{
	uint32_t const decoded_before = m_video_decoded_frames;
	std::size_t const queue_before = m_video_queue.size();
	std::size_t const buffered_before = m_video_buffer
			? plm_buffer_get_remaining(m_video_buffer) : 0;
	if (!m_video_decoder || !m_video_have_sequence || !m_fmv_decoder_enabled)
	{
		m_video_decoder_waiting_for_input = false;
		if (end_signalled
				&& m_video_replay_pump_events.size() < cdi_dvc::SAVE_VIDEO_REPLAY_PUMP_EVENTS)
		{
			m_video_replay_pump_events.push_back(cdi_dvc::save_video_replay_pump_event(
					m_video_replay_journal.size(), true, m_video_decoded_frames));
		}
		return;
	}

	m_video_decoder_waiting_for_input = false;
	bool decoder_exhausted = false;

	// TEMPORARY DVC A/B PROBE:
	// Decouple host MPEG decode-ahead depth from the nominal three-picture
	// VMPEG presentation/FIFO model.  MiSTer's PL_MPEG backend is allowed
	// to decode ahead into a substantially deeper host queue so compressed
	// input consumption is not artificially stalled by presentation depth.
	constexpr std::size_t backend_decode_ahead_pictures = 26;

	while (m_video_queue.size() < backend_decode_ahead_pictures)
	{
		plm_frame_t *const frame = plm_video_decode(m_video_decoder);
		if (!frame)
		{
			decoder_exhausted = true;
			m_video_decoder_waiting_for_input = !end_signalled;
			break;
		}

		++m_scheduler_decoded_frames;
		++m_video_decoded_frames;

		uint32_t frame_hash = 2166136261U;
		auto hash_plane = [&frame_hash](plm_plane_t const &plane)
		{
			size_t const bytes = size_t(plane.width) * size_t(plane.height);
			for (size_t i = 0; i < bytes; ++i)
			{
				frame_hash ^= plane.data[i];
				frame_hash *= 16777619U;
			}
		};
		hash_plane(frame->y);
		hash_plane(frame->cr);
		hash_plane(frame->cb);

		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO decoded frames=%u size=%ux%u time=%f fnv=%08x\n",
				machine().describe_context(),
				m_video_decoded_frames, frame->width, frame->height, frame->time, frame_hash);

		queued_video_frame queued;
		queued.width = uint16_t(frame->width);
		queued.height = uint16_t(frame->height);
		queued.generation = m_video_decoded_frames;
		queued.interrupts = video_picture_events_pop();

		// PL_MPEG advances its raw-video decoder clock by one coded-frame
		// duration per returned frame.  Use only that *relative* cadence here,
		// anchored once to the first video PES PTS seen after decoder reset.
		// This is an emulator presentation model, not a VMPEG FIFO claim.
		uint64_t const backend_time90 = uint64_t(
			frame->time * double(cdi_dvc::MPEG_SYSTEM_CLOCK_HZ) + 0.5);
		if (!m_video_pts_anchor_valid && m_mpeg_packet_have_pts[MPEG_FMV])
		{
			m_video_pts_anchor90 = m_mpeg_packet_pts[MPEG_FMV];
			m_video_backend_anchor90 = backend_time90;
			m_video_pts_anchor_valid = true;
			LOGMASKED(LOG_VIDEO,
					"%s: DVC VIDEO PTS anchor pts=%llu backend90=%llu frame=%u\n",
					machine().describe_context(),
					(unsigned long long)m_video_pts_anchor90,
					(unsigned long long)m_video_backend_anchor90,
					queued.generation);
		}
		if (m_video_pts_anchor_valid)
		{
			int64_t const relative90 =
				int64_t(backend_time90) - int64_t(m_video_backend_anchor90);
			queued.timestamp90 = cdi_dvc::mpeg_timestamp_normalize(
				m_video_pts_anchor90 + uint64_t(relative90));
			queued.timestamp_valid = true;
		}

		m_video_rgb24.resize(size_t(frame->width) * size_t(frame->height) * 3);
		plm_frame_to_rgb(frame, m_video_rgb24.data(), frame->width * 3);
		queued.pixels.resize(size_t(frame->width) * size_t(frame->height));
		for (size_t i = 0; i < queued.pixels.size(); ++i)
		{
			size_t const off = i * 3;
			queued.pixels[i] = 0xff000000U
					| (uint32_t(m_video_rgb24[off + 0]) << 16)
					| (uint32_t(m_video_rgb24[off + 1]) << 8)
					| uint32_t(m_video_rgb24[off + 2]);
		}

		uint32_t const generation = queued.generation;
		uint64_t const timestamp90 = queued.timestamp90;
		bool const timestamp_valid = queued.timestamp_valid;
		m_video_queue.push_back(std::move(queued));
		if (m_video_queue.size() > m_scheduler_max_queue_depth)
			m_scheduler_max_queue_depth = m_video_queue.size();

		LOGMASKED(LOG_VIDEO,
				"%s: DVC VIDEO queued frame=%u size=%ux%u pts_valid=%u pts=%llu depth=%u\n",
				machine().describe_context(), generation, frame->width, frame->height,
				timestamp_valid ? 1U : 0U, (unsigned long long)timestamp90,
				unsigned(m_video_queue.size()));
	}

	if (m_video_decoder_flush_pending && decoder_exhausted)
	{
		uint32_t const last_generation = !m_video_queue.empty()
				? m_video_queue.back().generation
				: (m_video_present_valid ? m_video_present_generation : 0);
		if (last_generation)
		{
			m_video_last_picture_generation = last_generation;
			m_video_last_picture_pending = true;
		}
		m_video_decoder_flush_pending = false;
	}

	if (decoded_before != m_video_decoded_frames || end_signalled)
	{
		if (m_video_replay_pump_events.size() < cdi_dvc::SAVE_VIDEO_REPLAY_PUMP_EVENTS)
		{
			m_video_replay_pump_events.push_back(cdi_dvc::save_video_replay_pump_event(
					m_video_replay_journal.size(), end_signalled, m_video_decoded_frames));
		}
		else if (!m_video_replay_pump_overflow)
		{
			m_video_replay_pump_overflow = true;
			logerror("DVC_SAVE_STATE_VIDEO_PUMP_REPLAY_OVERFLOW capacity=%u\n",
					unsigned(cdi_dvc::SAVE_VIDEO_REPLAY_PUMP_EVENTS));
		}
	}
	std::size_t const buffered_after = m_video_buffer
			? plm_buffer_get_remaining(m_video_buffer) : 0;

	LOGMASKED(LOG_SEQUENCE,
			"DVC_FMV_TRACE pump end=%u active=%u decoder=%u queue=%u->%u buffered=%u->%u ready=%04x->%04x decoded=%u->%u exhausted=%u irq=%04x enable=%04x ctx=%s\n",
			end_signalled ? 1U : 0U,
			m_fmv_playback_active ? 1U : 0U,
			m_fmv_decoder_enabled ? 1U : 0U,
			unsigned(queue_before), unsigned(m_video_queue.size()),
			unsigned(buffered_before), unsigned(buffered_after),
			unsigned(cdi_dvc::fmv_input_status(buffered_before)),
			unsigned(cdi_dvc::fmv_input_status(buffered_after)),
			unsigned(decoded_before), unsigned(m_video_decoded_frames),
			decoder_exhausted ? 1U : 0U,
			unsigned(m_fmv_interrupt_status),
			unsigned(m_fmv_interrupt_enable),
			machine().describe_context());

}

void cdi_dvc_device::video_decoder_flush()
{
	if (!m_video_buffer || !m_video_decoder)
		return;

	video_picture_events_flush();
	m_video_decoder_flush_pending = true;
	plm_buffer_signal_end(m_video_buffer);
	video_decoder_pump(true);

	LOGMASKED(LOG_SEQUENCE,
			"DVC_FMV_TRACE decoder-flush queue=%u decoded=%u flush_pending=%u last_pending=%u last_generation=%u irq=%04x ctx=%s\n",
			unsigned(m_video_queue.size()), m_video_decoded_frames,
			m_video_decoder_flush_pending ? 1U : 0U,
			m_video_last_picture_pending ? 1U : 0U,
			m_video_last_picture_generation, m_fmv_interrupt_status,
			machine().describe_context());
}

void cdi_dvc_device::mpeg_parser_reset(unsigned target)
{
	if (target > MPEG_FMV)
		return;

	if (target == MPEG_FMA)
		audio_decoder_reset();
	else if (target == MPEG_FMV)
	{
		video_decoder_reset();
		m_fmv_video_data_input_command = 0;
	}

	m_mpeg_prefix[target] = 0;
	m_mpeg_state[target] = target == MPEG_FMA ? MPEG_AUDIO_ACCESS : MPEG_SCAN;
	m_mpeg_stream_id[target] = 0;
	m_mpeg_packet_remaining[target] = 0;
	m_mpeg_skip_remaining[target] = 0;
	m_mpeg_selected[target] = false;
	m_mpeg_packet_counted[target] = false;
	if (target == MPEG_FMA)
	{
		m_fma_access_header_window = 0;
		m_fma_access_frame_remaining = 0;
		m_fma_access_header_bytes = 0;
	}

	m_mpeg_pack_index[target] = 0;
	m_mpeg_scr_temp[target] = 0;
	m_mpeg_last_scr[target] = 0;
	m_mpeg_have_scr[target] = false;
	m_mpeg_scr_dclk_anchor[target] = 0;
	m_mpeg_scr_events[target] = 0;
	m_mpeg_ts_mode[target] = 0;
	m_mpeg_ts_index[target] = 0;
	m_mpeg_pts_temp[target] = 0;
	m_mpeg_dts_temp[target] = 0;
	m_mpeg_packet_pts[target] = 0;
	m_mpeg_packet_dts[target] = 0;
	m_mpeg_packet_decode_ts[target] = 0;
	m_mpeg_packet_have_pts[target] = false;
	m_mpeg_packet_have_dts[target] = false;
	m_mpeg_pts_events[target] = 0;
	m_mpeg_dts_events[target] = 0;
	m_mpeg_scr_raw[target] = 0;
	m_mpeg_pts_raw[target] = 0;
	m_mpeg_dts_raw[target] = 0;
	m_mpeg_schedule_play_delta90[target] = 0;
	m_mpeg_schedule_decode_delta90[target] = 0;
	m_mpeg_schedule_play_delta45[target] = 0;
	m_mpeg_schedule_decode_delta45[target] = 0;
	m_mpeg_schedule_valid[target] = false;
	m_mpeg_schedule_events[target] = 0;

	m_mpeg_selected_packets[target] = 0;
	m_mpeg_payload_bytes[target] = 0;
	m_mpeg_first_payload[target] = 0;
	m_mpeg_last_payload[target] = 0;
	m_mpeg_have_payload[target] = false;
}

void cdi_dvc_device::mpeg_scr_byte(unsigned target, uint8_t data)
{
	if (target > MPEG_FMV)
		return;

	uint8_t const index = m_mpeg_pack_index[target];
	if (index < 5)
	{
		m_mpeg_scr_raw[target] = (m_mpeg_scr_raw[target] << 8) | uint64_t(data);
		if (index == 4)
		{
			uint64_t const raw = m_mpeg_scr_raw[target];
			std::array<uint8_t, 5> const field = {
				uint8_t(raw >> 32), uint8_t(raw >> 24), uint8_t(raw >> 16),
				uint8_t(raw >> 8), uint8_t(raw)
			};
			if (!cdi_dvc::mpeg1_timestamp_marker_bits_valid(field, 0x2))
				++m_mpeg_scr_marker_errors[target];
			m_mpeg_scr_temp[target] = cdi_dvc::decode_mpeg1_timestamp_field(field);
			m_mpeg_last_scr[target] = m_mpeg_scr_temp[target];
			m_mpeg_have_scr[target] = true;
			m_mpeg_scr_dclk_anchor[target] = target == MPEG_FMA
					? current_fma_dclk()
					: current_fmv_dclk();
			++m_mpeg_scr_events[target];
			m_mpeg_clock90 = m_mpeg_last_scr[target];
			m_mpeg_clock_valid = true;
			LOGMASKED(LOG_MPEG, "%s: DVC MPEG SCR %s scr=%llu event=%u clock=%llu marker_errors=%u\n",
					machine().describe_context(), target == MPEG_FMA ? "FMA" : "FMV",
					(unsigned long long)m_mpeg_last_scr[target], m_mpeg_scr_events[target],
					(unsigned long long)m_mpeg_clock90, m_mpeg_scr_marker_errors[target]);
		}
	}

	if (m_mpeg_pack_index[target] != 0xff)
		++m_mpeg_pack_index[target];
}

void cdi_dvc_device::mpeg_timestamp_start(unsigned target, uint8_t data, bool with_dts)
{
	m_mpeg_ts_mode[target] = with_dts ? 2 : 1;
	m_mpeg_ts_index[target] = 0;
	m_mpeg_pts_raw[target] = data;
	m_mpeg_dts_raw[target] = 0;
	m_mpeg_pts_temp[target] = 0;
	m_mpeg_dts_temp[target] = 0;
}

void cdi_dvc_device::mpeg_timestamp_byte(unsigned target, uint8_t data)
{
	if (target > MPEG_FMV || m_mpeg_ts_mode[target] == 0)
		return;

	uint8_t const index = ++m_mpeg_ts_index[target];
	if (index <= 4)
	{
		m_mpeg_pts_raw[target] = (m_mpeg_pts_raw[target] << 8) | uint64_t(data);
		if (index == 4 && m_mpeg_ts_mode[target] == 1)
			mpeg_timestamp_commit(target);
	}
	else if (index == 5)
	{
		m_mpeg_dts_raw[target] = data;
	}
	else if (index <= 9)
	{
		m_mpeg_dts_raw[target] = (m_mpeg_dts_raw[target] << 8) | uint64_t(data);
		if (index == 9)
			mpeg_timestamp_commit(target);
	}
}

void cdi_dvc_device::mpeg_timestamp_commit(unsigned target)
{
	bool const explicit_dts = m_mpeg_ts_mode[target] == 2;
	auto const field_from_raw = [](uint64_t raw)
	{
		return std::array<uint8_t, 5>{
			uint8_t(raw >> 32), uint8_t(raw >> 24), uint8_t(raw >> 16),
			uint8_t(raw >> 8), uint8_t(raw)
		};
	};

	std::array<uint8_t, 5> const pts_field = field_from_raw(m_mpeg_pts_raw[target]);
	uint8_t const pts_prefix = explicit_dts ? 0x3 : 0x2;
	if (!cdi_dvc::mpeg1_timestamp_marker_bits_valid(pts_field, pts_prefix))
		++m_mpeg_pts_marker_errors[target];
	m_mpeg_packet_pts[target] = cdi_dvc::decode_mpeg1_timestamp_field(pts_field);
	m_mpeg_pts_temp[target] = m_mpeg_packet_pts[target];

	if (explicit_dts)
	{
		std::array<uint8_t, 5> const dts_field = field_from_raw(m_mpeg_dts_raw[target]);
		if (!cdi_dvc::mpeg1_timestamp_marker_bits_valid(dts_field, 0x1))
			++m_mpeg_dts_marker_errors[target];
		m_mpeg_packet_dts[target] = cdi_dvc::decode_mpeg1_timestamp_field(dts_field);
		m_mpeg_dts_temp[target] = m_mpeg_packet_dts[target];
	}
	else
	{
		m_mpeg_packet_dts[target] = m_mpeg_packet_pts[target];
		m_mpeg_dts_temp[target] = m_mpeg_packet_dts[target];
	}

	m_mpeg_packet_decode_ts[target] = m_mpeg_packet_dts[target];
	m_mpeg_packet_have_pts[target] = true;
	m_mpeg_packet_have_dts[target] = explicit_dts;
	if (target == MPEG_FMV)
	{
		m_fmv_video_data_input_command |=
				cdi_dvc::FMV_VDI_DECODING_TIMESTAMP_UPDATED;

		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE dts-update value=%04x ts90=%llu vdi=%04x ctx=%s\n",
				cdi_dvc::fmv_reduced_decoding_timestamp(
						m_mpeg_packet_decode_ts[MPEG_FMV]),
				(unsigned long long)m_mpeg_packet_decode_ts[MPEG_FMV],
				m_fmv_video_data_input_command,
				machine().describe_context());
	}
	++m_mpeg_pts_events[target];
	if (explicit_dts)
		++m_mpeg_dts_events[target];

	LOGMASKED(LOG_MPEG, "%s: DVC MPEG TS %s pts=%llu dts=%llu explicit_dts=%u pts_event=%u dts_event=%u pts_marker_errors=%u dts_marker_errors=%u\n",
			machine().describe_context(), target == MPEG_FMA ? "FMA" : "FMV",
			(unsigned long long)m_mpeg_packet_pts[target],
			(unsigned long long)m_mpeg_packet_dts[target], explicit_dts ? 1U : 0U,
			m_mpeg_pts_events[target], m_mpeg_dts_events[target],
			m_mpeg_pts_marker_errors[target], m_mpeg_dts_marker_errors[target]);

	m_mpeg_ts_mode[target] = 0;
	m_mpeg_ts_index[target] = 0;
}

void cdi_dvc_device::mpeg_schedule_packet(unsigned target)
{
	if (target > MPEG_FMV || !m_mpeg_selected[target] || !m_mpeg_packet_counted[target])
		return;

	m_mpeg_schedule_valid[target] = false;
	if (!m_mpeg_packet_have_pts[target] || !m_mpeg_clock_valid)
		return;

	uint64_t const play_ts = m_mpeg_packet_pts[target];
	uint64_t const decode_ts = target == MPEG_FMV
		? m_mpeg_packet_decode_ts[target]
		: m_mpeg_packet_pts[target];

	m_mpeg_schedule_play_delta90[target] = cdi_dvc::mpeg_timestamp_delta(play_ts, m_mpeg_clock90);
	m_mpeg_schedule_decode_delta90[target] = cdi_dvc::mpeg_timestamp_delta(decode_ts, m_mpeg_clock90);
	m_mpeg_schedule_play_delta45[target] = cdi_dvc::mpeg_dclk_delta(play_ts, m_mpeg_clock90);
	m_mpeg_schedule_decode_delta45[target] = cdi_dvc::mpeg_dclk_delta(decode_ts, m_mpeg_clock90);
	m_mpeg_schedule_valid[target] = true;
	++m_mpeg_schedule_events[target];

	LOGMASKED(LOG_MPEG,
			"%s: DVC MPEG SCHED %s scr=%llu pts=%llu dts=%llu explicit_dts=%u play90=%lld decode90=%lld play45=%d decode45=%d event=%u\n",
			machine().describe_context(), target == MPEG_FMA ? "FMA" : "FMV",
			(unsigned long long)m_mpeg_clock90,
			(unsigned long long)m_mpeg_packet_pts[target],
			(unsigned long long)m_mpeg_packet_dts[target],
			m_mpeg_packet_have_dts[target] ? 1U : 0U,
			(long long)m_mpeg_schedule_play_delta90[target],
			(long long)m_mpeg_schedule_decode_delta90[target],
			m_mpeg_schedule_play_delta45[target],
			m_mpeg_schedule_decode_delta45[target],
			m_mpeg_schedule_events[target]);
}

void cdi_dvc_device::mpeg_begin_payload(unsigned target)
{
	if (!m_mpeg_packet_counted[target])
	{
		++m_mpeg_selected_packets[target];
		m_mpeg_packet_counted[target] = true;
	}
}

void cdi_dvc_device::mpeg_payload_byte(unsigned target, uint8_t data)
{
	mpeg_begin_payload(target);

	if (!m_mpeg_have_payload[target])
	{
		m_mpeg_first_payload[target] = data;
		m_mpeg_have_payload[target] = true;
	}

	m_mpeg_last_payload[target] = data;
	++m_mpeg_payload_bytes[target];

	if (target == MPEG_FMA)
		audio_decoder_feed(data);
	else if (target == MPEG_FMV)
		video_decoder_feed(data);
}

void cdi_dvc_device::mpeg_packet_done(unsigned target)
{
	mpeg_schedule_packet(target);
	if (target == MPEG_FMA && m_mpeg_selected[target] && m_mpeg_packet_counted[target])
		audio_decoder_pump();
	else if (target == MPEG_FMV && m_mpeg_selected[target] && m_mpeg_packet_counted[target])
	{
		bool const sequence_flush = m_video_sequence_end_pending;
		video_decoder_pump();
		if (sequence_flush)
		{
			video_decoder_flush();
			m_video_sequence_end_pending = false;
		}

	}

	m_mpeg_prefix[target] = 0;
	m_mpeg_state[target] = MPEG_SCAN;
	m_mpeg_stream_id[target] = 0;
	m_mpeg_packet_remaining[target] = 0;
	m_mpeg_skip_remaining[target] = 0;
	m_mpeg_selected[target] = false;
	m_mpeg_packet_counted[target] = false;
}

void cdi_dvc_device::mpeg_byte_w(unsigned target, uint8_t data)
{
	if (target > MPEG_FMV)
		return;

	switch (m_mpeg_state[target])
	{
	case MPEG_AUDIO_ACCESS:
	{
		// Before the first pack is established, and between directly accessed
		// frames, accept either a system start-code prefix or a complete Layer II
		// header.  Do not scan inside a frame: its payload is bounded below by the
		// length derived from that header and may contain a coincidental prefix.
		cdi_dvc::mpeg1_audio_access_result const access =
			cdi_dvc::route_mpeg1_audio_access_byte(
				m_mpeg_prefix[target], m_fma_access_header_window,
				m_fma_access_frame_remaining, m_fma_access_header_bytes, data);
		m_mpeg_prefix[target] = access.start_code_prefix;
		m_fma_access_header_window = access.audio_header_window;
		m_fma_access_frame_remaining = access.frame_bytes_remaining;
		m_fma_access_header_bytes = access.audio_header_bytes;

		switch (access.route)
		{
		case cdi_dvc::mpeg1_audio_access_route::system_start_code:
			m_mpeg_state[target] = MPEG_STREAM_ID;
			break;

		case cdi_dvc::mpeg1_audio_access_route::audio_header:
			m_mpeg_selected[target] = true;
			for (unsigned byte = 0; byte < 4; ++byte)
				mpeg_payload_byte(target, uint8_t(access.detected_audio_header >> (24 - byte * 8)));
			break;

		case cdi_dvc::mpeg1_audio_access_route::audio_payload:
			mpeg_payload_byte(target, data);
			if (access.frame_complete)
				audio_decoder_pump();
			break;

		case cdi_dvc::mpeg1_audio_access_route::scanning:
			break;
		}
		break;
	}

	case MPEG_SCAN:
		m_mpeg_prefix[target] = ((m_mpeg_prefix[target] << 8) | data) & 0x00ffffffU;
		if (m_mpeg_prefix[target] == 0x000001U)
			m_mpeg_state[target] = MPEG_STREAM_ID;
		break;

	case MPEG_STREAM_ID:
	{
		m_mpeg_stream_id[target] = data;
		m_mpeg_prefix[target] = 0;
		m_mpeg_selected[target] = false;
		m_mpeg_packet_counted[target] = false;
		m_mpeg_packet_have_pts[target] = false;
		m_mpeg_packet_have_dts[target] = false;
		m_mpeg_ts_mode[target] = 0;
		m_mpeg_ts_index[target] = 0;

		cdi_dvc::mpeg1_start_code_route const route =
				cdi_dvc::classify_mpeg1_start_code(
					target == MPEG_FMA, data,
					target == MPEG_FMA ? m_fma_stream : m_fmv_stream);
		switch (route)
		{
		case cdi_dvc::mpeg1_start_code_route::pack_header:
			// MPEG-1 pack header: first five bytes carry the 33-bit SCR;
			// three mux-rate bytes follow.
			m_mpeg_pack_index[target] = 0;
			m_mpeg_scr_temp[target] = 0;
			m_mpeg_skip_remaining[target] = 8;
			m_mpeg_state[target] = MPEG_PACK_SKIP;
			break;

		case cdi_dvc::mpeg1_start_code_route::program_end:
			// ISO/IEC 11172 program end code.
			mpeg_packet_done(target);
			if (target == MPEG_FMA)
			{
				audio_decoder_flush();
				m_fma_status |= cdi_dvc::FMA_IRQ_END_ISO;
				m_fma_interrupt_status |= cdi_dvc::FMA_IRQ_END_ISO;
				update_interrupt_state();
				LOGMASKED(LOG_AUDIO, "%s: DVC AUDIO program end\n", machine().describe_context());
			}
			if (target == MPEG_FMV)
			{
				m_fmv_interrupt_status |= cdi_dvc::FMV_IRQ_END_ISO;
				update_interrupt_state();
				video_decoder_flush();
				LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO program end\n", machine().describe_context());
			}
			break;

		case cdi_dvc::mpeg1_start_code_route::selected_pes:
		case cdi_dvc::mpeg1_start_code_route::skipped_packet:
			// PES/system packets carry a big-endian 16-bit packet length.
			m_mpeg_selected[target] =
				route == cdi_dvc::mpeg1_start_code_route::selected_pes;
			m_mpeg_packet_remaining[target] = 0;
			m_mpeg_state[target] = MPEG_LENGTH_HI;
			break;
		}
		break;
	}

	case MPEG_LENGTH_HI:
		m_mpeg_packet_remaining[target] = uint16_t(data) << 8;
		m_mpeg_state[target] = MPEG_LENGTH_LO;
		break;

	case MPEG_LENGTH_LO:
		m_mpeg_packet_remaining[target] |= data;

		if (m_mpeg_packet_remaining[target] == 0)
		{
			// MPEG-1 CD-i streams normally use bounded PES packets.
			// A zero-length packet cannot be bounded by the current parser,
			// so return to start-code scanning rather than consuming forever.
			mpeg_packet_done(target);
		}
		else if (m_mpeg_selected[target])
		{
			m_mpeg_state[target] = MPEG_PES_HEADER;
		}
		else
		{
			m_mpeg_state[target] = MPEG_PACKET_SKIP;
		}
		break;

	case MPEG_PACK_SKIP:
		mpeg_scr_byte(target, data);
		if (m_mpeg_skip_remaining[target] != 0)
			--m_mpeg_skip_remaining[target];

		if (m_mpeg_skip_remaining[target] == 0)
			mpeg_packet_done(target);
		break;

	case MPEG_PACKET_SKIP:
		if (m_mpeg_packet_remaining[target] != 0)
			--m_mpeg_packet_remaining[target];

		if (m_mpeg_packet_remaining[target] == 0)
			mpeg_packet_done(target);
		break;

	case MPEG_PES_HEADER:
	{
		if (m_mpeg_packet_remaining[target] == 0)
		{
			mpeg_packet_done(target);
			break;
		}

		--m_mpeg_packet_remaining[target];

		cdi_dvc::mpeg1_pes_header_kind const header_kind =
				cdi_dvc::classify_mpeg1_pes_header_byte(data);
		switch (header_kind)
		{
		case cdi_dvc::mpeg1_pes_header_kind::stuffing:
			if (!cdi_dvc::mpeg1_pes_header_can_continue(
					header_kind, m_mpeg_packet_remaining[target]))
				mpeg_packet_done(target);
			break;

		case cdi_dvc::mpeg1_pes_header_kind::std_buffer:
			if (cdi_dvc::mpeg1_pes_header_can_continue(
					header_kind, m_mpeg_packet_remaining[target]))
				m_mpeg_state[target] = MPEG_PES_STD_SECOND;
			else
				mpeg_packet_done(target);
			break;

		case cdi_dvc::mpeg1_pes_header_kind::pts:
			mpeg_timestamp_start(target, data, false);
			if (cdi_dvc::mpeg1_pes_header_can_continue(
					header_kind, m_mpeg_packet_remaining[target]))
			{
				m_mpeg_skip_remaining[target] = 4;
				m_mpeg_state[target] = MPEG_PES_HEADER_SKIP;
			}
			else
				mpeg_packet_done(target);
			break;

		case cdi_dvc::mpeg1_pes_header_kind::pts_dts:
			mpeg_timestamp_start(target, data, true);
			if (cdi_dvc::mpeg1_pes_header_can_continue(
					header_kind, m_mpeg_packet_remaining[target]))
			{
				m_mpeg_skip_remaining[target] = 9;
				m_mpeg_state[target] = MPEG_PES_HEADER_SKIP;
			}
			else
				mpeg_packet_done(target);
			break;

		case cdi_dvc::mpeg1_pes_header_kind::no_timestamp:
			mpeg_begin_payload(target);
			m_mpeg_state[target] = MPEG_PAYLOAD;
			if (m_mpeg_packet_remaining[target] == 0)
				mpeg_packet_done(target);
			break;

		case cdi_dvc::mpeg1_pes_header_kind::payload_fallback:
			// Defensive fallback for malformed/variant MPEG-1 PES headers:
			// treat the unexpected byte as the first payload byte.
			mpeg_payload_byte(target, data);
			m_mpeg_state[target] = MPEG_PAYLOAD;
			if (m_mpeg_packet_remaining[target] == 0)
				mpeg_packet_done(target);
			break;
		}
		break;
	}

	case MPEG_PES_STD_SECOND:
		if (m_mpeg_packet_remaining[target] != 0)
			--m_mpeg_packet_remaining[target];

		if (m_mpeg_packet_remaining[target] == 0)
			mpeg_packet_done(target);
		else
			m_mpeg_state[target] = MPEG_PES_HEADER;
		break;

	case MPEG_PES_HEADER_SKIP:
		if (m_mpeg_packet_remaining[target] != 0)
			--m_mpeg_packet_remaining[target];

		mpeg_timestamp_byte(target, data);
		if (m_mpeg_skip_remaining[target] != 0)
			--m_mpeg_skip_remaining[target];

		if (m_mpeg_packet_remaining[target] == 0)
		{
			mpeg_packet_done(target);
		}
		else if (m_mpeg_skip_remaining[target] == 0)
		{
			mpeg_begin_payload(target);
			m_mpeg_state[target] = MPEG_PAYLOAD;
		}
		break;

	case MPEG_PAYLOAD:
		if (m_mpeg_packet_remaining[target] == 0)
		{
			mpeg_packet_done(target);
			break;
		}

		--m_mpeg_packet_remaining[target];
		mpeg_payload_byte(target, data);

		if (m_mpeg_packet_remaining[target] == 0)
			mpeg_packet_done(target);
		break;

	default:
		mpeg_packet_done(target);
		break;
	}
}

void cdi_dvc_device::mpeg_word_w(bool for_fma, uint16_t data, uint16_t mem_mask)
{
	const unsigned target = for_fma ? MPEG_FMA : MPEG_FMV;

	if (mem_mask & 0xff00)
		mpeg_byte_w(target, uint8_t(data >> 8));

	if (mem_mask & 0x00ff)
		mpeg_byte_w(target, uint8_t(data));
}

void cdi_dvc_device::dma_w(uint16_t data)
{
	if (!m_dma_active)
		return;

	if (m_dma_transfer_words == 0)
		m_dma_first_word = data;

	m_dma_last_word = data;
	++m_dma_transfer_words;
	mpeg_word_w(m_dma_for_fma, data);

	if (!m_dma_for_fma)
	{
		m_fmv_transfer_word = data;
		++m_fmv_transfer_words;
	}
}

void cdi_dvc_device::dma_done()
{
	if (!m_dma_active)
		return;

	LOGMASKED(LOG_DMA, "%s: DVC DMA complete %s words=%u first=%04x last=%04x\n",
			machine().describe_context(),
			m_dma_for_fma ? "FMA" : "FMV",
			m_dma_transfer_words,
			m_dma_first_word,
			m_dma_last_word);
	if (!m_dma_for_fma)
	{
		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE dma-done words=%u first=%04x last=%04x queue=%u decoded=%u irq=%04x ctx=%s\n",
				m_dma_transfer_words, m_dma_first_word, m_dma_last_word,
				unsigned(m_video_queue.size()), m_video_decoded_frames,
				m_fmv_interrupt_status, machine().describe_context());
	}

	if (m_dma_for_fma)
		m_fma_command &= ~0x8000;

	const unsigned mpeg_target = m_dma_for_fma ? MPEG_FMA : MPEG_FMV;
	LOGMASKED(LOG_MPEG, "%s: DVC MPEG %s packets=%u payload=%u first=%02x last=%02x\n",
			machine().describe_context(),
			mpeg_target == MPEG_FMA ? "FMA" : "FMV",
			m_mpeg_selected_packets[mpeg_target],
			m_mpeg_payload_bytes[mpeg_target],
			m_mpeg_have_payload[mpeg_target] ? m_mpeg_first_payload[mpeg_target] : 0,
			m_mpeg_have_payload[mpeg_target] ? m_mpeg_last_payload[mpeg_target] : 0);

	m_dma_active = false;
	m_dma_for_fma = false;
	m_dma_req_callback(CLEAR_LINE);
}

void cdi_dvc_device::record_dsp_bootstrap_write(
		unsigned index, uint32_t address, uint16_t data, uint16_t mem_mask)
{
	if (index >= m_dsp_bootstrap_last.size())
		return;

	COMBINE_DATA(&m_dsp_bootstrap_last[index]);
	++m_dsp_bootstrap_write_count[index];
	LOGMASKED(LOG_REGISTERS,
			"%s: DVC DSP bootstrap telemetry %08x <- %04x & %04x count=%u last=%04x\n",
			machine().describe_context(), address, data, mem_mask,
			m_dsp_bootstrap_write_count[index], m_dsp_bootstrap_last[index]);
}

void cdi_dvc_device::av_clock_observe()
{
	if (!m_av_audio_pts_valid || !m_av_video_pts_valid)
		return;

	m_av_last_cross_delta90 = cdi_dvc::mpeg_timestamp_delta(
			m_av_last_video_pts90, m_av_last_audio_pts90);
	uint64_t const absolute = m_av_last_cross_delta90 < 0
			? uint64_t(-m_av_last_cross_delta90)
			: uint64_t(m_av_last_cross_delta90);
	if (absolute > m_av_max_abs_cross_delta90)
		m_av_max_abs_cross_delta90 = absolute;
	++m_av_cross_observations;
}

void cdi_dvc_device::mpeg_ram_compat_reset()
{
	m_mpeg_ram_compat_visible = false;
	m_mpeg_ram_gated_reads = 0;
	m_mpeg_ram_gated_writes = 0;
}

uint16_t cdi_dvc_device::mpeg_ram_r(offs_t offset, uint16_t mem_mask)
{
	if (!m_mpeg_ram_compat_visible)
	{
		if (!machine().side_effects_disabled())
			++m_mpeg_ram_gated_reads;
		LOGMASKED(LOG_RAM_ACCESS, "%s: DVC MPEG RAM gated read %08x & %04x\n",
				machine().describe_context(), 0xe80000 + uint32_t(offset << 1), mem_mask);
		return 0;
	}

	return m_mpeg_ram[offset & 0x3ffff];
}

void cdi_dvc_device::mpeg_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (!m_mpeg_ram_compat_visible)
	{
		++m_mpeg_ram_gated_writes;
		LOGMASKED(LOG_RAM_ACCESS, "%s: DVC MPEG RAM gated write %08x <- %04x & %04x\n",
				machine().describe_context(), 0xe80000 + uint32_t(offset << 1), data, mem_mask);
		return;
	}

	COMBINE_DATA(&m_mpeg_ram[offset & 0x3ffff]);
}

uint16_t cdi_dvc_device::rom_r(offs_t offset, uint16_t mem_mask)
{
	const uint32_t address = 0xe40000U + (uint32_t(offset) << 1);
	uint16_t result = 0;

	if (m_driver_rom)
	{
		// The 128 KiB driver ROM is mirrored once across E40000-E7FFFF.
		const uint32_t byte_offset = (uint32_t(offset) & 0xffffU) << 1;
		result = (uint16_t(m_driver_rom[byte_offset]) << 8)
				| uint16_t(m_driver_rom[byte_offset + 1]);
	}

	if (!machine().side_effects_disabled())
	{
		LOGMASKED(LOG_REGISTERS, "%s: DVC ROM read %08x -> %04x & %04x\n",
				machine().describe_context(), address, result, mem_mask);
	}

	return result;
}

void cdi_dvc_device::rom_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	const uint32_t address = 0xe40000U + (uint32_t(offset) << 1);

	LOGMASKED(LOG_REGISTERS, "%s: DVC ROM write ignored %08x <- %04x & %04x\n",
			machine().describe_context(), address, data, mem_mask);
}

void cdi_dvc_device::write(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	const uint32_t address = 0xe00000U + (uint32_t(offset) << 1);

	// PROVISIONAL COMPATIBILITY MECHANISM: Stage12G29 validated this state
	// transition as a sufficient MPEG-RAM visibility boundary for native, PAL
	// and NTSC workloads. Do not interpret it as proven hardware ownership.
	if (address == 0xe03018U && (mem_mask & 0x0001U))
	{
		const bool old_state = m_mpeg_ram_compat_visible;
		m_mpeg_ram_compat_visible = bool(data & 0x0001U);
		LOGMASKED(LOG_RAM_GATE,
				"DVC_E03018_COMPAT_STATE old=%u new=%u data=%04x mask=%04x gated_reads=%u gated_writes=%u ctx=%s\n",
				old_state ? 1U : 0U, m_mpeg_ram_compat_visible ? 1U : 0U, data, mem_mask,
				m_mpeg_ram_gated_reads, m_mpeg_ram_gated_writes, machine().describe_context());
	}

	if ((address & 0xfffff000U) == 0xe01000U)
	{
		COMBINE_DATA(&m_vcd_control);
		m_vcd_control &= 0x0001;

		LOGMASKED(LOG_REGISTERS, "%s: VMPEG VCD control %08x <- %04x & %04x\n",
				machine().describe_context(), address, data, mem_mask);
		return;
	}

	switch (address)
	{
	case 0xe03000:
		// Current FMA command model:
		// 0001 = stop/reset, 0002 = start, bit 15 = DMA request.
		COMBINE_DATA(&m_fma_command);

		if (m_fma_command & 0x0001)
		{
			mpeg_parser_reset(MPEG_FMA);
			m_fma_status = 0;
			m_fma_interrupt_status = 0;
			update_interrupt_state();
		}

		if (m_fma_command & 0x8000)
		{
			// Assert the external DMA request. The host driver services channel 1
			// in scheduled bounded slices under the current timing model.
			m_fma_status &= ~0x0008;
			m_dma_active = true;
			m_dma_for_fma = true;
			m_dma_transfer_words = 0;
			m_dma_first_word = 0;
			m_dma_last_word = 0;
			m_dma_req_callback(ASSERT_LINE);
			LOGMASKED(LOG_DMA, "%s: FMA DMA requested, command %04x\n",
					machine().describe_context(), m_fma_command);
		}
		break;

	case 0xe03004:
		break;
	case 0xe03008:
		COMBINE_DATA(&m_fma_stream);
		m_fma_stream = cdi_dvc::normalize_mpeg_stream_number(true, m_fma_stream);
		break;
	case 0xe0300c:
		COMBINE_DATA(&m_fma_interrupt_vector);
		break;
	case 0xe03010:
	case 0xe03012:
		// FMA DCLK is free-running and not CPU-writable.
		break;

	case 0xe0301c:
		COMBINE_DATA(&m_fma_interrupt_enable);
		update_interrupt_state();
		break;
	case 0xe03022:
		record_dsp_bootstrap_write(0, address, data, mem_mask);
		break;
	case 0xe03024:
		record_dsp_bootstrap_write(1, address, data, mem_mask);
		break;

	case 0xe04060:
		COMBINE_DATA(&m_fmv_interrupt_enable);
		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE irq-enable value=%04x status=%04x ctx=%s\n",
				m_fmv_interrupt_enable, m_fmv_interrupt_status,
				machine().describe_context());
		update_interrupt_state();
		break;
	case 0xe04062:
		break;
	case 0xe04064:
		COMBINE_DATA(&m_fmv_timer_compare);
		update_timer();
		break;
	case 0xe0406c:
		record_dsp_bootstrap_write(2, address, data, mem_mask);
		break;
	case 0xe0406e:
		record_dsp_bootstrap_write(3, address, data, mem_mask);
		break;
	case 0xe04070:
		record_dsp_bootstrap_write(4, address, data, mem_mask);
		break;
	case 0xe04072:
		record_dsp_bootstrap_write(5, address, data, mem_mask);
		break;
	case 0xe040ae:
		update_timer();
		break;
	case 0xe04098:
		set_fmv_syscr(data, mem_mask);
		break;

	case 0xe04074:
		COMBINE_DATA(&m_video_screen_y_shadow);
		break;
	case 0xe04076:
		COMBINE_DATA(&m_video_screen_x_shadow);
		break;
	case 0xe04078:
		COMBINE_DATA(&m_video_window_h_shadow);
		break;
	case 0xe0407a:
		COMBINE_DATA(&m_video_window_w_shadow);
		break;
	case 0xe0407c:
		COMBINE_DATA(&m_video_crop_y_shadow);
		break;
	case 0xe0407e:
		COMBINE_DATA(&m_video_crop_x_shadow);
		break;
	case 0xe0408c:
	{
		uint16_t const old_value = m_fmv_video_data_input_command;
		COMBINE_DATA(&m_fmv_video_data_input_command);

		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE vdi-write old=%04x data=%04x mask=%04x new=%04x bit14=%u ctx=%s\n",
				old_value,
				data,
				mem_mask,
				m_fmv_video_data_input_command,
				(m_fmv_video_data_input_command
						& cdi_dvc::FMV_VDI_DECODING_TIMESTAMP_UPDATED) ? 1U : 0U,
				machine().describe_context());
		break;
	}
	case 0xe040c0:
	{
		COMBINE_DATA(&m_fmv_system_command);
		cdi_dvc::system_command_effects const effects =
			cdi_dvc::decode_system_command(m_fmv_system_command);
		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE system-command value=%04x mask=%04x stream=%u queue=%u irq=%04x active=%u step=%u decoder=%u ctx=%s\n",
				m_fmv_system_command, mem_mask, m_fmv_stream,
				unsigned(m_video_queue.size()), m_fmv_interrupt_status,
				m_fmv_playback_active ? 1U : 0U,
				m_fmv_single_step_pending ? 1U : 0U,
				m_fmv_decoder_enabled ? 1U : 0U,
				machine().describe_context());

		if (effects.clear_fifo)
		{
			m_fmv_playback_active = false;
			m_fmv_single_step_pending = false;
			mpeg_parser_reset(MPEG_FMV);
		}

		if (effects.decoder_on)
		{
			m_fmv_decoder_enabled = true;
			video_decoder_pump();
		}
		if (effects.decoder_off)
		{
			m_fmv_decoder_enabled = false;
			m_fmv_playback_active = false;
			m_fmv_single_step_pending = false;
		}

		if (effects.play || effects.continue_playback)
		{
			m_fmv_playback_active = true;
			m_fmv_single_step_pending = false;
			video_decoder_pump();
		}
		if (effects.pause)
		{
			m_fmv_playback_active = false;
			m_fmv_single_step_pending = false;
			m_fmv_interrupt_status |= cdi_dvc::FMV_IRQ_PAUSE;
			update_interrupt_state();
		}
		if (effects.step)
		{
			m_fmv_playback_active = false;
			m_fmv_single_step_pending = true;
			video_decoder_pump();
		}
		if (effects.stop)
		{
			m_fmv_playback_active = false;
			m_fmv_single_step_pending = false;
		}

		if (effects.dma)
		{
			m_dma_active = true;
			m_dma_for_fma = false;
			m_dma_transfer_words = 0;
			m_dma_first_word = 0;
			m_dma_last_word = 0;
			m_dma_req_callback(ASSERT_LINE);
		}
		if (effects.dma)
			LOGMASKED(LOG_DMA, "%s: FMV DMA requested\n", machine().describe_context());
		break;
	}
	case 0xe040c2:
	{
		COMBINE_DATA(&m_fmv_video_command);
		cdi_dvc::video_command_effects const effects = cdi_dvc::decode_video_command(m_fmv_video_command);
		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE video-command value=%04x mask=%04x queue=%u visible=%u output=%u ctx=%s\n",
				m_fmv_video_command, mem_mask, unsigned(m_video_queue.size()),
				m_video_visible ? 1U : 0U, m_video_output_enabled ? 1U : 0U,
				machine().describe_context());

		if (effects.register_update && effects.scroll)
		{
			m_video_geometry_vblank_pending = true;
			m_video_geometry_frame_pending = false;
		}
		else if (effects.register_update)
		{
			m_video_geometry_vblank_pending = false;
			m_video_geometry_frame_pending = true;
		}

		if (effects.hide)
		{
			m_video_visible = false;
			m_video_show_on_next = false;
			LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO hide\n", machine().describe_context());
		}
		if (effects.show_immediate)
		{
			m_video_visible = true;
			m_video_show_on_next = false;
			video_overlay_reset();
			LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO show immediate\n", machine().describe_context());
		}
		if (effects.show_on_next)
		{
			m_video_show_on_next = true;
			LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO show armed for next frame\n", machine().describe_context());
		}
		if (effects.video_off)
		{
			m_video_output_enabled = false;
			video_overlay_reset();
			LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO off\n", machine().describe_context());
		}
		if (effects.video_on)
		{
			m_video_output_enabled = true;
			video_overlay_reset();
			LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO on\n", machine().describe_context());
		}
		break;
	}
	case 0xe040c6:
		COMBINE_DATA(&m_fmv_system_control);
		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE system-control value=%04x mask=%04x sync=%u ctx=%s\n",
				m_fmv_system_control, mem_mask,
				(m_fmv_system_control & 0x0004U) ? 1U : 0U,
				machine().describe_context());
		break;

	case 0xe040c4:
		COMBINE_DATA(&m_fmv_stream);
		m_fmv_stream = cdi_dvc::normalize_mpeg_stream_number(false, m_fmv_stream);
		LOGMASKED(LOG_SEQUENCE,
				"DVC_FMV_TRACE stream value=%u mask=%04x ctx=%s\n",
				m_fmv_stream, mem_mask, machine().describe_context());
		break;
	case 0xe040dc:
		COMBINE_DATA(&m_fmv_interrupt_vector);
		break;
	case 0xe040de:
		COMBINE_DATA(&m_fmv_transfer_word);
		++m_fmv_transfer_words;
		LOGMASKED(LOG_DMA, "%s: FMV XFER word %08x = %04x (count=%u)\n",
				machine().describe_context(), address, m_fmv_transfer_word, m_fmv_transfer_words);
		mpeg_word_w(false, m_fmv_transfer_word, mem_mask);
		break;
	default:
		break;
	}

	LOGMASKED(LOG_REGISTERS, "%s: VMPEG write %08x <- %04x & %04x\n",
			machine().describe_context(), address, data, mem_mask);
}
