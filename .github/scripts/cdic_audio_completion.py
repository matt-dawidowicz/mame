#!/usr/bin/env python3

from pathlib import Path
import re

CPP = Path("src/mame/philips/cdicdic.cpp")
text = CPP.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    '#include "cdicdic.h"\n',
    '#include "cdicdic.h"\n#include "cdicdic_audio.h"\n',
    "audio helper include",
)

replace_once(
    '''\nnamespace {\n\nfloat attenuation_scale(uint8_t attenuation)\n{\n\tif (attenuation & 0x80)\n\t\treturn 0.0f;\n\n\treturn powf(10.0f, -(attenuation & 0x7f) / 20.0f);\n}\n\n} // anonymous namespace\n\n''',
    '\n',
    "legacy floating attenuation helper",
)

start = text.find('void cdicdic_device::decode_xa_unit(')
end_marker = 'TIMER_CALLBACK_MEMBER( cdicdic_device::audio_tick )'
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("audio implementation block: markers not found")

new_audio_block = r'''void cdicdic_device::decode_xa_unit(uint8_t param, uint8_t maximum_range, int16_t sample, int16_t &sample0, int16_t &sample1, int16_t &out_buffer)
{
	const uint8_t normalized = cdic_audio::normalize_sound_parameter(param, maximum_range);
	const cdic_hle::xa_sample decoded = cdic_hle::decode_xa_sample(normalized, sample, sample0, sample1);
	sample0 = decoded.recent;
	sample1 = decoded.older;
	out_buffer = decoded.output;
}

void cdicdic_device::decode_8bit_xa_unit(int channel, uint8_t param, const uint8_t *data, int16_t *out_buffer)
{
	int16_t *const old_samples = &m_xa_last[channel << 1];
	for (int i = 0; i < 28; i++)
	{
		const int16_t sample = int16_t(uint16_t(*data) << 8);
		decode_xa_unit(param, 8, sample, old_samples[0], old_samples[1], out_buffer[i]);
		data += 4;
	}
}

void cdicdic_device::decode_4bit_xa_unit(int channel, uint8_t param, const uint8_t *data, uint8_t shift, int16_t *out_buffer)
{
	int16_t *const old_samples = &m_xa_last[channel << 1];
	for (int i = 0; i < 28; i++)
	{
		const int16_t sample = int16_t(uint16_t((*data >> shift) & 0xf) << 12);
		decode_xa_unit(param, 12, sample, old_samples[0], old_samples[1], out_buffer[i]);
		data += 4;
	}
}

void cdicdic_device::play_xa_group(uint8_t coding, const uint8_t *data, uint16_t idx)
{
	static constexpr uint16_t HEADER_OFFSET_4BIT[8] = { 4, 5, 6, 7, 12, 13, 14, 15 };
	static constexpr uint16_t HEADER_OFFSET_8BIT[4] = { 4, 5, 6, 7 };
	static constexpr uint16_t DATA_OFFSET_4BIT[8] = { 16, 16, 17, 17, 18, 18, 19, 19 };
	static constexpr uint16_t DATA_OFFSET_8BIT[4] = { 16, 17, 18, 19 };

	const cdic_audio::coding_info format = cdic_audio::decode_coding(coding);
	const uint8_t sound_units = format.bits_per_sample == 8 ? 4 : 8;

	for (uint8_t i = 0; i < sound_units; i++)
	{
		if (format.bits_per_sample == 8)
		{
			if (format.stereo)
				decode_8bit_xa_unit(i & 1, data[HEADER_OFFSET_8BIT[i]], data + DATA_OFFSET_8BIT[i], &m_samples[i & 1][idx + (i >> 1) * 28]);
			else
				decode_8bit_xa_unit(0, data[HEADER_OFFSET_8BIT[i]], data + DATA_OFFSET_8BIT[i], &m_samples[0][idx + i * 28]);
		}
		else if (format.stereo)
		{
			decode_4bit_xa_unit(i & 1, data[HEADER_OFFSET_4BIT[i]], data + DATA_OFFSET_4BIT[i], (i & 1) ? 4 : 0, &m_samples[i & 1][idx + (i >> 1) * 28]);
		}
		else
		{
			decode_4bit_xa_unit(0, data[HEADER_OFFSET_4BIT[i]], data + DATA_OFFSET_4BIT[i], (i & 1) ? 4 : 0, &m_samples[0][idx + i * 28]);
		}
	}
}

void cdicdic_device::reset_deemphasis()
{
	m_deemphasis_enabled = false;
	m_deemphasis_rate = 0;
	std::fill_n(m_deemphasis_previous_input, 2, 0.0);
	std::fill_n(m_deemphasis_previous_output, 2, 0.0);
}

void cdicdic_device::apply_deemphasis(int16_t &left, int16_t &right, bool enabled, uint32_t sample_rate)
{
	if (!enabled || !sample_rate)
	{
		if (m_deemphasis_enabled || m_deemphasis_rate)
			reset_deemphasis();
		return;
	}

	if (!m_deemphasis_enabled || m_deemphasis_rate != sample_rate)
	{
		reset_deemphasis();
		m_deemphasis_enabled = true;
		m_deemphasis_rate = sample_rate;
	}

	left = cdic_audio::deemphasis_sample(
		left,
		m_deemphasis_previous_input[0],
		m_deemphasis_previous_output[0],
		sample_rate);
	right = cdic_audio::deemphasis_sample(
		right,
		m_deemphasis_previous_input[1],
		m_deemphasis_previous_output[1],
		sample_rate);
}

void cdicdic_device::play_cdda_sector(const uint8_t *data)
{
	constexpr uint32_t SAMPLE_RATE = 44100;
	m_dmadac[0]->set_frequency(SAMPLE_RATE);
	m_dmadac[1]->set_frequency(SAMPLE_RATE);

	const cdrom_file::toc &toc = m_cdrom->get_toc();
	const uint32_t track = m_cdrom->get_track(m_curr_lba);
	const bool emphasis =
		track < toc.numtrks &&
		bool(toc.tracks[track].control_flags & cdrom_file::CD_FLAG_CONTROL_PREEMPHASIS);
	const cdic_audio::mixer_gains gains = cdic_audio::decode_mixer_gains(
		m_atten[0], m_atten[1], m_atten[2], m_atten[3]);

	constexpr uint16_t NUM_SAMPLES = SECTOR_SIZE / 4;
	for (uint16_t i = 0; i < NUM_SAMPLES; i++)
	{
		int16_t left = int16_t((uint16_t(data[(i * 4) + 1]) << 8) | data[(i * 4) + 0]);
		int16_t right = int16_t((uint16_t(data[(i * 4) + 3]) << 8) | data[(i * 4) + 2]);
		apply_deemphasis(left, right, emphasis, SAMPLE_RATE);
		const cdic_audio::stereo_sample mixed = cdic_audio::mix_sample(left, right, gains);
		m_samples[0][i] = mixed.left;
		m_samples[1][i] = mixed.right;
	}

	m_dmadac[0]->transfer(0, 1, 1, NUM_SAMPLES, &m_samples[0][0]);
	m_dmadac[1]->transfer(0, 1, 1, NUM_SAMPLES, &m_samples[1][0]);
}

void cdicdic_device::play_audio_sector(uint8_t coding, const uint8_t *data)
{
	const cdic_audio::coding_info format = cdic_audio::decode_coding(coding);
	if (!format.valid)
	{
		LOGMASKED(LOG_SECTORS, "Invalid/reserved coding (%02x), ignoring\n", coding);
		return;
	}

	const uint32_t sample_frequency = clock2() / format.sample_rate_divisor;
	LOGMASKED(
		LOG_SECTORS,
		"Coding %02x, %u channels, %u bits, %u frequency, emphasis=%u\n",
		coding, format.stereo ? 2U : 1U, format.bits_per_sample,
		sample_frequency, format.emphasis ? 1U : 0U);

	m_dmadac[0]->set_frequency(sample_frequency);
	m_dmadac[1]->set_frequency(sample_frequency);

	const uint16_t samples_per_group = 8 >> ((format.bits_per_sample == 8 ? 1 : 0) + (format.stereo ? 1 : 0));
	uint16_t offset = 0;
	for (uint16_t i = 0; i < SECTOR_AUDIO_SIZE; i += 128, data += 128)
	{
		play_xa_group(coding, data, offset);
		offset += 28 * samples_per_group;
	}

	const cdic_audio::mixer_gains gains = cdic_audio::decode_mixer_gains(
		m_atten[0], m_atten[1], m_atten[2], m_atten[3]);
	const uint16_t total_samples = 18 * 28 * samples_per_group;
	for (uint16_t i = 0; i < total_samples; i++)
	{
		int16_t left = m_samples[0][i];
		int16_t right = m_samples[format.stereo ? 1 : 0][i];
		apply_deemphasis(left, right, format.emphasis, sample_frequency);
		const cdic_audio::stereo_sample mixed = cdic_audio::mix_sample(left, right, gains);
		m_samples[0][i] = mixed.left;
		m_samples[1][i] = mixed.right;
	}

	m_dmadac[0]->transfer(0, 1, 1, total_samples, &m_samples[0][0]);
	m_dmadac[1]->transfer(0, 1, 1, total_samples, &m_samples[1][0]);
}

'''
text = text[:start] + new_audio_block + text[end:]

replace_once(
    '''uint8_t cdicdic_device::get_sector_count_for_coding(uint8_t coding)\n{\n\treturn cdic_hle::xa_sector_count(coding);\n}\n''',
    '''uint8_t cdicdic_device::get_sector_count_for_coding(uint8_t coding)\n{\n\treturn cdic_audio::decode_coding(coding).sector_count;\n}\n''',
    "sector count helper",
)

replace_once(
    '''\tm_disc_spinup_counter = 6; // Bugfix #14462: 6 or higher is required to prevent some softlocks.\n}\n''',
    '''\tm_disc_spinup_counter = 6; // Bugfix #14462: 6 or higher is required to prevent some softlocks.\n\tif (disc_mode == DISC_CDDA || disc_mode == DISC_MODE2)\n\t\treset_deemphasis();\n}\n''',
    "disc audio filter reset",
)

replace_once(
    '''\t\t\t\tm_decoding_audio_map = true;\n\t\t\t\tstd::fill_n(m_xa_last, 4, 0);\n''',
    '''\t\t\t\tm_decoding_audio_map = true;\n\t\t\t\tstd::fill_n(m_xa_last, 4, 0);\n\t\t\t\treset_deemphasis();\n''',
    "soundmap audio filter reset",
)

replace_once(
    '''\tsave_item(NAME(m_atten));\n\tsave_item(NAME(m_xa_last));\n''',
    '''\tsave_item(NAME(m_atten));\n\tsave_item(NAME(m_xa_last));\n\tsave_item(NAME(m_deemphasis_enabled));\n\tsave_item(NAME(m_deemphasis_rate));\n\tsave_item(NAME(m_deemphasis_previous_input));\n\tsave_item(NAME(m_deemphasis_previous_output));\n''',
    "de-emphasis save state",
)

replace_once(
    '''\tstd::fill_n(m_atten, 4, 0);\n\tstd::fill_n(m_xa_last, 4, 0);\n''',
    '''\tstd::fill_n(m_atten, 4, 0);\n\tstd::fill_n(m_xa_last, 4, 0);\n\treset_deemphasis();\n''',
    "de-emphasis reset state",
)

if 'attenuation_scale(' in text:
    raise SystemExit("legacy attenuation_scale reference remains")
if 'play_raw_group(' in text:
    raise SystemExit("dead raw 16-bit audio path remains")
if 'set_volume(0x100)' in new_audio_block:
    raise SystemExit("live audio block must not override SLAVE mute state")
if ' * 0.25' in new_audio_block:
    raise SystemExit("undocumented quarter-scale remains")

CPP.write_text(text, encoding="utf-8")
print("CDIC audio live-source transformation: PASS")
