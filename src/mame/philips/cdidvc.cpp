// license:BSD-3-Clause
// copyright-holders:Matt Jordan
/***************************************************************************

    Philips CD-i Digital Video Cartridge
    -------------------------------------

    VMPEG/DVC emulation for CD-i Mono-I systems.

    The current model includes VMPEG register access, interrupt generation and
    acknowledge behavior, SCC68070 DMA ingress, MPEG-1 program-stream parsing,
    PL_MPEG-backed audio/video decoding, audio output, and video presentation.

    Known limitations remain: DMA transfers are serviced synchronously, A/V
    presentation scheduling is incomplete, decoder/presentation state is not
    fully save-state serializable, and MPEG-RAM startup visibility currently
    uses a provisional MAME-only compatibility trigger. Several fixed register
    values and video-presentation scale factors also await independent hardware
    attribution.

***************************************************************************/

#include "emu.h"
#include "cdidvc.h"
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

// Keep the low-volume gate transition visible for the 12G14 regression.
// All high-volume diagnostics are opt-in.
#define VERBOSE          (LOG_RAM_GATE)
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
	save_item(NAME(m_mpeg_ram_compat_write_count));
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
	save_item(NAME(m_fmv_video_command));
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

	save_item(NAME(m_mpeg_prefix));
	save_item(NAME(m_mpeg_state));
	save_item(NAME(m_mpeg_stream_id));
	save_item(NAME(m_mpeg_packet_remaining));
	save_item(NAME(m_mpeg_skip_remaining));
	save_item(NAME(m_mpeg_selected));
	save_item(NAME(m_mpeg_packet_counted));
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
	save_item(NAME(m_mpeg_clock90));
	save_item(NAME(m_mpeg_clock_valid));
	save_item(NAME(m_mpeg_scr_dclk_anchor));
	save_item(NAME(m_mpeg_schedule_play_delta90));
	save_item(NAME(m_mpeg_schedule_decode_delta90));
	save_item(NAME(m_mpeg_schedule_play_delta45));
	save_item(NAME(m_mpeg_schedule_decode_delta45));
	save_item(NAME(m_mpeg_schedule_valid));
	save_item(NAME(m_mpeg_schedule_events));
}

void cdi_dvc_device::device_stop()
{
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
	m_fmv_video_command = 0;
	m_fmv_stream = 0;
	m_fmv_interrupt_vector = 0;

	m_fmv_transfer_word = 0;
	m_fmv_transfer_words = 0;

	m_dma_active = false;
	m_dma_for_fma = false;
	m_dma_transfer_words = 0;
	m_dma_first_word = 0;
	m_dma_last_word = 0;

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
		result = m_fma_stream & 0x000f;
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
		result = FMV_STATUS_COMPAT_INPUT_READY;
		break;
	case 0xe04060:
		result = m_fmv_interrupt_enable;
		break;
	case 0xe04062:
		result = m_fmv_interrupt_status;
		if (!machine().side_effects_disabled())
		{
			m_fmv_interrupt_status = 0;
			update_interrupt_state();
		}
		break;
	case 0xe04064:
		result = m_fmv_timer_compare;
		break;
	case 0xe04098:
		result = uint16_t(current_fmv_dclk() >> 6);
		break;

	case 0xe0409c:
		result = 0;
		break;
	case 0xe0409e:
		result = 1;
		break;
	case 0xe040a4:
		result = 0;
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
	m_audio_output_started = false;
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
		int16_t left = 0;
		int16_t right = 0;
		bool have_pcm = false;

		if (m_audio_wait_samples)
		{
			--m_audio_wait_samples;
			++m_audio_silence_frames;
		}
		else if (m_audio_pcm_read + 1 < m_audio_pcm_queue.size())
		{
			if (!m_audio_output_started)
			{
				m_audio_output_started = true;
				uint32_t const pending = uint32_t((m_audio_pcm_queue.size() - m_audio_pcm_read) / 2);
				LOGMASKED(LOG_AUDIO,
						"%s: DVC AUDIO output start rate=%u silence=%llu pending=%u\n",
						machine().describe_context(), m_audio_output_rate,
						(unsigned long long)m_audio_silence_frames, pending);
			}

			left = m_audio_pcm_queue[m_audio_pcm_read++];
			right = m_audio_pcm_queue[m_audio_pcm_read++];
			have_pcm = true;

			auto hash_sample = [this](int16_t sample)
			{
				uint16_t const u = uint16_t(sample);
				m_audio_output_hash ^= uint8_t(u & 0xff);
				m_audio_output_hash *= 16777619U;
				m_audio_output_hash ^= uint8_t(u >> 8);
				m_audio_output_hash *= 16777619U;
			};
			hash_sample(left);
			hash_sample(right);
			++m_audio_output_frames;
			if (left != 0 || right != 0)
				++m_audio_output_nonzero;
		}

		stream.put_int(0, i, left, 32768);
		stream.put_int(1, i, right, 32768);

		if (have_pcm && m_audio_pcm_read >= m_audio_pcm_queue.size())
		{
			LOGMASKED(LOG_AUDIO,
					"%s: DVC AUDIO output drain frames=%u nonzero=%u silence=%llu fnv=%08x events=%u\n",
					machine().describe_context(), m_audio_output_frames,
					m_audio_output_nonzero, (unsigned long long)m_audio_silence_frames,
					m_audio_output_hash, m_audio_queue_events);
			m_audio_pcm_queue.clear();
			m_audio_pcm_read = 0;
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

	m_audio_header_shift = 0;
	m_audio_decoded_frames = 0;
	m_audio_decoded_samples = 0;
	m_audio_header_events = 0;
	m_audio_decode_events = 0;
	m_audio_bitrate_kbps = 0;
	m_audio_samplerate = 0;
	m_audio_channel_mode = 0;
	m_audio_backend_status = 0;
	m_audio_have_es_header = false;
	m_audio_have_header = false;

	m_audio_buffer = plm_buffer_create_with_capacity(128 * 1024);
	m_audio_decoder = plm_audio_create_with_buffer(m_audio_buffer, 1);
}

void cdi_dvc_device::audio_decoder_feed(uint8_t data)
{
	m_audio_header_shift = (m_audio_header_shift << 8) | data;

	if (!m_audio_have_es_header)
	{
		uint32_t const header = m_audio_header_shift;
		unsigned const sync = (header >> 21) & 0x7ff;
		unsigned const version = (header >> 19) & 0x03;
		unsigned const layer = (header >> 17) & 0x03;
		unsigned const bitrate_index = (header >> 12) & 0x0f;
		unsigned const samplerate_index = (header >> 10) & 0x03;
		unsigned const channel_mode = (header >> 6) & 0x03;
		static constexpr uint16_t bitrate_kbps[16] =
			{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 };
		static constexpr uint16_t samplerate_hz[4] = { 44100, 48000, 32000, 0 };

		if (sync == 0x7ff && version == 3 && layer == 2
			&& bitrate_kbps[bitrate_index] != 0 && samplerate_hz[samplerate_index] != 0)
		{
			m_audio_have_es_header = true;
			m_audio_bitrate_kbps = bitrate_kbps[bitrate_index];
			m_audio_samplerate = samplerate_hz[samplerate_index];
			m_audio_channel_mode = channel_mode;
			m_audio_backend_status |= 0x01;
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
	if (!m_audio_decoder || !m_audio_have_header)
		return;

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
			float const sample = samples->interleaved[i];
			float const scaled = sample * 32767.0f;
			int32_t q = scaled >= 0.0f ? int32_t(scaled + 0.5f) : int32_t(scaled - 0.5f);
			if (q < -32768)
				q = -32768;
			if (q > 32767)
				q = 32767;
			int16_t const pcm = int16_t(q);
			uint16_t const u = uint16_t(pcm);
			hash ^= uint8_t(u & 0xff);
			hash *= 16777619U;
			hash ^= uint8_t(u >> 8);
			hash *= 16777619U;
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
	m_video_compat_interrupts = 0;
	m_video_compat_generation = 0;
	m_video_compat_frame_pending = false;
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
	if (m_video_queue.empty())
		return;

	std::size_t selected_index = 0;
	std::size_t consume_count = 1;
	bool timestamp_driven = false;
	uint64_t clock90 = 0;

	if (m_video_queue.front().timestamp_valid && m_mpeg_have_scr[MPEG_FMV])
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
	else
	{
		// Compatibility fallback for streams that have not established both a
		// frame timestamp and an FMV SCR clock.  Preserve queued order and do not
		// reintroduce the old single-slot overwrite behavior.
		++m_scheduler_fallback_presented;
	}

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

	video_overlay_reset();

	LOGMASKED(LOG_VIDEO,
			"%s: DVC VIDEO presented frame=%u size=%ux%u pts_valid=%u pts=%llu clock=%llu queue=%u superseded=%u\n",
			machine().describe_context(), m_video_present_generation,
			m_video_present_width, m_video_present_height,
			selected.timestamp_valid ? 1U : 0U,
			(unsigned long long)selected.timestamp90,
			(unsigned long long)clock90,
			unsigned(m_video_queue.size()), unsigned(selected_index));
}

void cdi_dvc_device::video_compat_frame_event()
{
	if (!m_video_compat_frame_pending)
		return;

	++m_scheduler_compat_frame_events;

	// Preserve the previously validated guest-visible event boundary while
	// frame pixels themselves are selected independently by the PTS queue.
	video_latch_geometry(false);
	if (m_video_show_on_next)
	{
		m_video_visible = true;
		m_video_show_on_next = false;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO compat show-next frame=%u\n",
				machine().describe_context(), m_video_compat_generation);
	}

	video_overlay_reset();
	uint16_t interrupts = cdi_dvc::FMV_IRQ_PICTURE | m_video_compat_interrupts;
	if (m_video_last_picture_pending && m_video_compat_generation == m_video_last_picture_generation)
	{
		interrupts |= cdi_dvc::FMV_IRQ_END_OF_DATA;
		m_video_last_picture_pending = false;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO compat last picture frame=%u\n",
				machine().describe_context(), m_video_compat_generation);
	}
	m_fmv_interrupt_status |= interrupts;
	update_interrupt_state();

	LOGMASKED(LOG_VIDEO,
			"%s: DVC VIDEO compat frame-event generation=%u interrupts=%04x queue=%u\n",
			machine().describe_context(), m_video_compat_generation,
			interrupts, unsigned(m_video_queue.size()));

	m_video_compat_interrupts = 0;
	m_video_compat_generation = 0;
	m_video_compat_frame_pending = false;
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
	video_compat_frame_event();
	video_latch_frame();
}

void cdi_dvc_device::video_overlay_scanline(uint32_t *pixels, unsigned pixel_count, int physical_y, int visible_top,
		int clip_min_x, int clip_max_x, bool const *external_video, unsigned external_count)
{
	if (!pixels || !external_video || !m_video_output_enabled || !m_video_visible || !m_video_present_valid)
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

	// Current MAME presentation mapping; hardware provenance for these scale
	// factors is still pending.
	int const dst_x = int(m_video_screen_x) * 4;
	int const dst_y = visible_top + int(m_video_screen_y) * 2;
	int const rel_y = physical_y - dst_y;
	if (rel_y < 0 || rel_y >= int(window_h * 2))
		return;

	unsigned const src_y = unsigned(m_video_crop_y) + unsigned(rel_y / 2);
	for (unsigned x = 0; x < window_w; ++x)
	{
		unsigned const src_x = unsigned(m_video_crop_x) + x;
		uint32_t const color = m_video_present_frame[size_t(src_y) * m_video_present_width + src_x];
		for (unsigned repeat = 0; repeat < 2; ++repeat)
		{
			int const out_x = dst_x + int(x * 2 + repeat);
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
		}
	}

	if (!m_video_overlay_complete && physical_y == dst_y + int(window_h * 2) - 1 && m_video_overlay_pixels)
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
	video_frame_clear();
	video_decoder_destroy();

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
	m_video_sequence_end_events = 0;
	m_video_last_picture_generation = 0;
	m_video_last_picture_pending = false;

	m_video_buffer = plm_buffer_create_with_capacity(256 * 1024);
	m_video_decoder = plm_video_create_with_buffer(m_video_buffer, 1);
}

void cdi_dvc_device::video_decoder_feed(uint8_t data)
{
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

void cdi_dvc_device::video_decoder_pump()
{
	if (!m_video_decoder || !m_video_have_sequence)
		return;

	for (;;)
	{
		plm_frame_t *const frame = plm_video_decode(m_video_decoder);
		if (!frame)
			break;

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
		m_video_compat_interrupts |= queued.interrupts;
		m_video_compat_generation = queued.generation;
		m_video_compat_frame_pending = true;

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
}

void cdi_dvc_device::video_decoder_flush()
{
	if (!m_video_buffer || !m_video_decoder)
		return;

	video_picture_events_flush();
	plm_buffer_signal_end(m_video_buffer);
	video_decoder_pump();

	uint32_t const last_generation = !m_video_queue.empty()
			? m_video_queue.back().generation
			: (m_video_present_valid ? m_video_present_generation : 0);
	if (last_generation)
	{
		m_video_last_picture_generation = last_generation;
		m_video_last_picture_pending = true;
	}
}

void cdi_dvc_device::mpeg_parser_reset(unsigned target)
{
	if (target > MPEG_FMV)
		return;

	if (target == MPEG_FMA)
		audio_decoder_reset();
	else if (target == MPEG_FMV)
		video_decoder_reset();

	m_mpeg_prefix[target] = 0;
	m_mpeg_state[target] = MPEG_SCAN;
	m_mpeg_stream_id[target] = 0;
	m_mpeg_packet_remaining[target] = 0;
	m_mpeg_skip_remaining[target] = 0;
	m_mpeg_selected[target] = false;
	m_mpeg_packet_counted[target] = false;

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

	switch (m_mpeg_pack_index[target])
	{
	case 0:
		m_mpeg_scr_temp[target] = uint64_t(data & 0x0e) << 29;
		break;
	case 1:
		m_mpeg_scr_temp[target] |= uint64_t(data) << 22;
		break;
	case 2:
		m_mpeg_scr_temp[target] |= uint64_t(data & 0xfe) << 14;
		break;
	case 3:
		m_mpeg_scr_temp[target] |= uint64_t(data) << 7;
		break;
	case 4:
		m_mpeg_scr_temp[target] |= uint64_t(data >> 1);
		m_mpeg_last_scr[target] = m_mpeg_scr_temp[target] & ((uint64_t(1) << 33) - 1);
		m_mpeg_have_scr[target] = true;
		m_mpeg_scr_dclk_anchor[target] = target == MPEG_FMA
				? current_fma_dclk()
				: current_fmv_dclk();
		++m_mpeg_scr_events[target];
		m_mpeg_clock90 = m_mpeg_last_scr[target];
		m_mpeg_clock_valid = true;
		LOGMASKED(LOG_MPEG, "%s: DVC MPEG SCR %s scr=%llu event=%u clock=%llu\n",
				machine().describe_context(), target == MPEG_FMA ? "FMA" : "FMV",
				(unsigned long long)m_mpeg_last_scr[target], m_mpeg_scr_events[target],
				(unsigned long long)m_mpeg_clock90);
		break;
	default:
		break;
	}

	if (m_mpeg_pack_index[target] != 0xff)
		++m_mpeg_pack_index[target];
}

void cdi_dvc_device::mpeg_timestamp_start(unsigned target, uint8_t data, bool with_dts)
{
	m_mpeg_ts_mode[target] = with_dts ? 2 : 1;
	m_mpeg_ts_index[target] = 0;
	m_mpeg_pts_temp[target] = uint64_t(data & 0x0e) << 29;
	m_mpeg_dts_temp[target] = 0;
}

void cdi_dvc_device::mpeg_timestamp_byte(unsigned target, uint8_t data)
{
	if (target > MPEG_FMV || m_mpeg_ts_mode[target] == 0)
		return;

	uint8_t const index = ++m_mpeg_ts_index[target];
	switch (index)
	{
	case 1: m_mpeg_pts_temp[target] |= uint64_t(data) << 22; break;
	case 2: m_mpeg_pts_temp[target] |= uint64_t(data & 0xfe) << 14; break;
	case 3: m_mpeg_pts_temp[target] |= uint64_t(data) << 7; break;
	case 4:
		m_mpeg_pts_temp[target] |= uint64_t(data >> 1);
		if (m_mpeg_ts_mode[target] == 1)
			mpeg_timestamp_commit(target);
		break;
	case 5: m_mpeg_dts_temp[target] = uint64_t(data & 0x0e) << 29; break;
	case 6: m_mpeg_dts_temp[target] |= uint64_t(data) << 22; break;
	case 7: m_mpeg_dts_temp[target] |= uint64_t(data & 0xfe) << 14; break;
	case 8: m_mpeg_dts_temp[target] |= uint64_t(data) << 7; break;
	case 9:
		m_mpeg_dts_temp[target] |= uint64_t(data >> 1);
		mpeg_timestamp_commit(target);
		break;
	default:
		break;
	}
}

void cdi_dvc_device::mpeg_timestamp_commit(unsigned target)
{
	static constexpr uint64_t mask = (uint64_t(1) << 33) - 1;
	bool const explicit_dts = m_mpeg_ts_mode[target] == 2;
	m_mpeg_packet_pts[target] = m_mpeg_pts_temp[target] & mask;
	m_mpeg_packet_dts[target] = explicit_dts
		? (m_mpeg_dts_temp[target] & mask)
		: m_mpeg_packet_pts[target];
	m_mpeg_packet_decode_ts[target] = m_mpeg_packet_dts[target];
	m_mpeg_packet_have_pts[target] = true;
	m_mpeg_packet_have_dts[target] = explicit_dts;
	++m_mpeg_pts_events[target];
	if (explicit_dts)
		++m_mpeg_dts_events[target];

	LOGMASKED(LOG_MPEG, "%s: DVC MPEG TS %s pts=%llu dts=%llu explicit_dts=%u pts_event=%u dts_event=%u\n",
			machine().describe_context(), target == MPEG_FMA ? "FMA" : "FMV",
			(unsigned long long)m_mpeg_packet_pts[target],
			(unsigned long long)m_mpeg_packet_dts[target], explicit_dts ? 1U : 0U,
			m_mpeg_pts_events[target], m_mpeg_dts_events[target]);

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

bool cdi_dvc_device::mpeg_stream_selected(unsigned target, uint8_t stream_id) const
{
	bool const for_fma = target == MPEG_FMA;
	uint16_t const selected_stream = for_fma ? m_fma_stream : m_fmv_stream;
	return cdi_dvc::mpeg_stream_selected(for_fma, stream_id, selected_stream);
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
		video_decoder_pump();
		if (m_video_sequence_end_pending)
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
	case MPEG_SCAN:
		m_mpeg_prefix[target] = ((m_mpeg_prefix[target] << 8) | data) & 0x00ffffffU;
		if (m_mpeg_prefix[target] == 0x000001U)
			m_mpeg_state[target] = MPEG_STREAM_ID;
		break;

	case MPEG_STREAM_ID:
		m_mpeg_stream_id[target] = data;
		m_mpeg_prefix[target] = 0;
		m_mpeg_selected[target] = false;
		m_mpeg_packet_counted[target] = false;
		m_mpeg_packet_have_pts[target] = false;
		m_mpeg_packet_have_dts[target] = false;
		m_mpeg_ts_mode[target] = 0;
		m_mpeg_ts_index[target] = 0;

		if (data == 0xba)
		{
			// MPEG-1 pack header: first five bytes carry the 33-bit SCR;
			// three mux-rate bytes follow.
			m_mpeg_pack_index[target] = 0;
			m_mpeg_scr_temp[target] = 0;
			m_mpeg_skip_remaining[target] = 8;
			m_mpeg_state[target] = MPEG_PACK_SKIP;
		}
		else if (data == 0xb9)
		{
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
		}
		else
		{
			// PES/system packets carry a big-endian 16-bit packet length.
			m_mpeg_selected[target] = mpeg_stream_selected(target, data);
			m_mpeg_packet_remaining[target] = 0;
			m_mpeg_state[target] = MPEG_LENGTH_HI;
		}
		break;

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
		if (m_mpeg_packet_remaining[target] == 0)
		{
			mpeg_packet_done(target);
			break;
		}

		--m_mpeg_packet_remaining[target];

		if (data == 0xff)
		{
			// MPEG-1 PES stuffing byte.
			break;
		}

		if ((data & 0xc0) == 0x40)
		{
			// First byte of the optional STD buffer field.
			m_mpeg_state[target] = MPEG_PES_STD_SECOND;
			break;
		}

		if ((data & 0xf0) == 0x20)
		{
			// MPEG-1 PTS: current byte plus four more bytes.
			mpeg_timestamp_start(target, data, false);
			m_mpeg_skip_remaining[target] = 4;
			m_mpeg_state[target] = MPEG_PES_HEADER_SKIP;
			break;
		}

		if ((data & 0xf0) == 0x30)
		{
			// MPEG-1 PTS + DTS: current PTS byte, four PTS bytes,
			// then a five-byte DTS field.
			mpeg_timestamp_start(target, data, true);
			m_mpeg_skip_remaining[target] = 9;
			m_mpeg_state[target] = MPEG_PES_HEADER_SKIP;
			break;
		}

		if (data == 0x0f)
		{
			// No PTS/DTS. Payload starts with the next byte.
			mpeg_begin_payload(target);
			m_mpeg_state[target] = MPEG_PAYLOAD;

			if (m_mpeg_packet_remaining[target] == 0)
				mpeg_packet_done(target);
			break;
		}

		// Defensive fallback for malformed/variant MPEG-1 PES headers:
		// treat the unexpected byte as the first payload byte.
		mpeg_payload_byte(target, data);
		m_mpeg_state[target] = MPEG_PAYLOAD;

		if (m_mpeg_packet_remaining[target] == 0)
			mpeg_packet_done(target);
		break;

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

void cdi_dvc_device::mpeg_ram_compat_reset()
{
	m_mpeg_ram_compat_write_count = 0;
	m_mpeg_ram_compat_visible = false;
	m_mpeg_ram_gated_reads = 0;
	m_mpeg_ram_gated_writes = 0;
}

void cdi_dvc_device::mpeg_ram_compat_note_vmpeg_write()
{
	// PROVISIONAL COMPATIBILITY MECHANISM, NOT HARDWARE SPECIFICATION.
	// Stage12G29 validated E03018 bit-0 state as a replacement visibility
	// boundary. Keep the old VMPEG-write hook inert until dead compatibility
	// bookkeeping is removed in a separately reviewed no-behavior cleanup.
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
	mpeg_ram_compat_note_vmpeg_write();

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
			// Assert the external DMA request.  The SCC68070 channel-1 callback
			// services the current synchronous transfer model.
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
		m_fma_stream &= 0x000f;
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
	case 0xe03024:
		break;

	case 0xe04060:
		COMBINE_DATA(&m_fmv_interrupt_enable);
		update_interrupt_state();
		break;
	case 0xe04062:
		break;
	case 0xe04064:
		COMBINE_DATA(&m_fmv_timer_compare);
		update_timer();
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
	case 0xe040c0:
		COMBINE_DATA(&m_fmv_system_command);

		if (m_fmv_system_command & 0x0100)
			mpeg_parser_reset(MPEG_FMV);

		if (m_fmv_system_command & 0x8000)
		{
			m_dma_active = true;
			m_dma_for_fma = false;
			m_dma_transfer_words = 0;
			m_dma_first_word = 0;
			m_dma_last_word = 0;
			m_dma_req_callback(ASSERT_LINE);
		}
		if (m_fmv_system_command & 0x8000)
			LOGMASKED(LOG_DMA, "%s: FMV DMA requested\n", machine().describe_context());
		break;
	case 0xe040c2:
	{
		COMBINE_DATA(&m_fmv_video_command);
		cdi_dvc::video_command_effects const effects = cdi_dvc::decode_video_command(m_fmv_video_command);

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
	case 0xe040c4:
		COMBINE_DATA(&m_fmv_stream);
		m_fmv_stream &= 0x000f;
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
