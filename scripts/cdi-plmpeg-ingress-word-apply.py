#!/usr/bin/env python3
from pathlib import Path
import re

CPP = Path("src/mame/philips/cdidvc.cpp")
HDR = Path("src/mame/philips/cdidvc.h")


def sub_once(text: str, pattern: str, replacement: str, label: str) -> str:
    new, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, got {count}")
    return new


cpp = CPP.read_text(encoding="utf-8")
hdr = HDR.read_text(encoding="utf-8")

hdr = hdr.replace(
    "\tvoid audio_decoder_feed(uint8_t data);\n\tvoid audio_decoder_pump();",
    "\tvoid audio_decoder_observe_byte(uint8_t data);\n"
    "\tvoid audio_decoder_commit_bytes(uint8_t *data, unsigned count);\n"
    "\tvoid audio_decoder_feed(uint8_t data);\n"
    "\tvoid audio_decoder_feed_word(uint8_t high, uint8_t low);\n"
    "\tvoid audio_decoder_pump();",
    1,
)
hdr = hdr.replace(
    "\tvoid video_decoder_feed(uint8_t data);\n\tvoid video_decoder_pump(bool end_signalled = false);",
    "\tvoid video_decoder_observe_byte(uint8_t data);\n"
    "\tvoid video_decoder_commit_bytes(uint8_t *data, unsigned count);\n"
    "\tvoid video_decoder_feed(uint8_t data);\n"
    "\tvoid video_decoder_feed_word(uint8_t high, uint8_t low);\n"
    "\tvoid video_decoder_pump(bool end_signalled = false);",
    1,
)
hdr = hdr.replace(
    "\tvoid mpeg_payload_byte(unsigned target, uint8_t data);\n\tvoid mpeg_packet_done(unsigned target);",
    "\tvoid mpeg_payload_byte(unsigned target, uint8_t data);\n"
    "\tvoid mpeg_payload_word(unsigned target, uint8_t high, uint8_t low);\n"
    "\tvoid mpeg_packet_done(unsigned target);",
    1,
)

if "audio_decoder_feed_word" not in hdr or "video_decoder_feed_word" not in hdr or "mpeg_payload_word" not in hdr:
    raise SystemExit("header declaration replacement failed")

new_audio = r'''void cdi_dvc_device::audio_decoder_observe_byte(uint8_t data)
{
	// A write reopens PL_MPEG's dynamic ring after signal_end(). Mirror that
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
			m_audio_emphasis = header.emphasis;
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
}

void cdi_dvc_device::audio_decoder_commit_bytes(uint8_t *data, unsigned count)
{
	if (!count || !m_audio_buffer || !m_audio_decoder)
		return;

	plm_buffer_write(m_audio_buffer, data, count);

	if (!m_audio_have_header && plm_audio_has_header(m_audio_decoder))
	{
		m_audio_have_header = true;
		cdi_dvc::mpeg_audio_control_state const control {
			m_fma_stream,
			m_fma_current_stream,
			m_fma_stream_change_pending,
			m_fma_program_ended
		};
		uint8_t const stream_id = cdi_dvc::mpeg_audio_stream_id(
				m_mpeg_stream_id[MPEG_FMA])
			? m_mpeg_stream_id[MPEG_FMA]
			: uint8_t(0xc0U | m_fma_stream);
		cdi_dvc::mpeg_audio_stream_commit_result const committed =
			cdi_dvc::commit_mpeg_audio_stream(control, stream_id);
		m_fma_current_stream = committed.state.current_stream;
		m_fma_stream_change_pending = committed.state.stream_change_pending;
		if (committed.signal_stream_change)
		{
			m_fma_status |= cdi_dvc::FMA_IRQ_STREAM_CHANGE;
			m_fma_interrupt_status |= cdi_dvc::FMA_IRQ_STREAM_CHANGE;
			update_interrupt_state();
		}
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

void cdi_dvc_device::audio_decoder_feed(uint8_t data)
{
	audio_decoder_observe_byte(data);
	audio_decoder_commit_bytes(&data, 1);
}

void cdi_dvc_device::audio_decoder_feed_word(uint8_t high, uint8_t low)
{
	uint8_t data[2] = { high, low };
	audio_decoder_observe_byte(high);
	audio_decoder_observe_byte(low);
	audio_decoder_commit_bytes(data, 2);
}

void cdi_dvc_device::audio_decoder_pump()'''

cpp = sub_once(
    cpp,
    r"void cdi_dvc_device::audio_decoder_feed\(uint8_t data\)\n\{.*?\n\}\n\nvoid cdi_dvc_device::audio_decoder_pump\(\)",
    new_audio,
    "audio feed refactor",
)

new_video = r'''void cdi_dvc_device::video_decoder_observe_byte(uint8_t data)
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

	for (unsigned i = 0; i < 3; ++i)
	{
		m_video_prefix_pts[i] = m_video_prefix_pts[i + 1];
		m_video_prefix_serial[i] = m_video_prefix_serial[i + 1];
	}
	m_video_prefix_pts[3] = m_video_packet_pts;
	m_video_prefix_serial[3] = m_video_packet_serial;
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
		m_video_picture_pts = m_video_prefix_pts[0];
		if (m_video_prefix_serial[0] == m_video_packet_serial)
			m_video_packet_pts = UINT64_MAX;
		++m_video_picture_headers;
		m_video_picture_header_bytes = 2;
		if (!m_video_sequence_end_pending)
			m_video_last_picture_pending = false;
		LOGMASKED(LOG_VIDEO, "%s: DVC VIDEO ES picture headers=%u\n",
				machine().describe_context(), m_video_picture_headers);
	}
}

void cdi_dvc_device::video_decoder_commit_bytes(uint8_t *data, unsigned count)
{
	if (!count || !m_video_buffer || !m_video_decoder)
		return;

	plm_buffer_write(m_video_buffer, data, count);

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

void cdi_dvc_device::video_decoder_feed(uint8_t data)
{
	video_decoder_observe_byte(data);
	video_decoder_commit_bytes(&data, 1);
}

void cdi_dvc_device::video_decoder_feed_word(uint8_t high, uint8_t low)
{
	uint8_t data[2] = { high, low };
	video_decoder_observe_byte(high);
	video_decoder_observe_byte(low);
	video_decoder_commit_bytes(data, 2);
}

void cdi_dvc_device::video_picture_event(uint8_t picture_type)'''

cpp = sub_once(
    cpp,
    r"void cdi_dvc_device::video_decoder_feed\(uint8_t data\)\n\{.*?\n\}\n\nvoid cdi_dvc_device::video_picture_event\(uint8_t picture_type\)",
    new_video,
    "video feed refactor",
)

payload_word = r'''void cdi_dvc_device::mpeg_payload_word(unsigned target, uint8_t high, uint8_t low)
{
	mpeg_begin_payload(target);

	if (!m_mpeg_have_payload[target])
	{
		m_mpeg_first_payload[target] = high;
		m_mpeg_have_payload[target] = true;
	}

	m_mpeg_last_payload[target] = low;
	m_mpeg_payload_bytes[target] += 2;

	if (target == MPEG_FMA)
		audio_decoder_feed_word(high, low);
	else if (target == MPEG_FMV)
		video_decoder_feed_word(high, low);
}

'''

cpp = sub_once(
    cpp,
    r"(void cdi_dvc_device::mpeg_payload_byte\(unsigned target, uint8_t data\)\n\{.*?\n\}\n\n)(void cdi_dvc_device::mpeg_packet_done\(unsigned target\))",
    r"\1" + payload_word + r"\2",
    "payload word insertion",
)

new_word = r'''void cdi_dvc_device::mpeg_word_w(bool for_fma, uint16_t data, uint16_t mem_mask)
{
	const unsigned target = for_fma ? MPEG_FMA : MPEG_FMV;

	// Once the raw decoder has established its header, a full DVC bus word
	// entirely inside a selected PES payload can be committed to PL_MPEG in one
	// two-byte write. Parser state, ES/header observation, replay journals and
	// timestamp bookkeeping still advance byte-by-byte. Packet boundaries,
	// partial bus writes and initial decoder-header discovery deliberately stay
	// on the original byte path.
	bool const full_word = (mem_mask & 0xff00U) && (mem_mask & 0x00ffU);
	bool const backend_ready = target == MPEG_FMA ? m_audio_have_header : m_video_have_sequence;
	bool const audio_accepting = target != MPEG_FMA
			|| cdi_dvc::mpeg_audio_input_accepting({
				m_fma_stream,
				m_fma_current_stream,
				m_fma_stream_change_pending,
				m_fma_program_ended
			});
	if (full_word && backend_ready && audio_accepting
			&& m_mpeg_state[target] == MPEG_PAYLOAD
			&& m_mpeg_selected[target]
			&& m_mpeg_packet_remaining[target] >= 2)
	{
		m_mpeg_packet_remaining[target] -= 2;
		mpeg_payload_word(target, uint8_t(data >> 8), uint8_t(data));
		if (m_mpeg_packet_remaining[target] == 0)
			mpeg_packet_done(target);
		return;
	}

	if (mem_mask & 0xff00)
		mpeg_byte_w(target, uint8_t(data >> 8));

	if (mem_mask & 0x00ff)
		mpeg_byte_w(target, uint8_t(data));
}

void cdi_dvc_device::dma_w(uint16_t data)'''

cpp = sub_once(
    cpp,
    r"void cdi_dvc_device::mpeg_word_w\(bool for_fma, uint16_t data, uint16_t mem_mask\)\n\{.*?\n\}\n\nvoid cdi_dvc_device::dma_w\(uint16_t data\)",
    new_word,
    "word fast path",
)

CPP.write_text(cpp, encoding="utf-8")
HDR.write_text(hdr, encoding="utf-8")
print("Applied guarded PL_MPEG two-byte ingress batching")
