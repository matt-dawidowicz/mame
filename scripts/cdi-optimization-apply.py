from pathlib import Path
import re


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


def replace_once(path, old, new, label):
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, got {count}")
    write(path, text.replace(old, new, 1))


replace_once(
    "src/mame/philips/cdidvc.cpp",
    '#define PLM_NO_STDIO\n#define PL_MPEG_IMPLEMENTATION\n#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"',
    '#define PLM_NO_STDIO\n#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"',
    "production PL_MPEG implementation split",
)

production_impl = Path("src/mame/philips/cdidvc_plmpeg.cpp")
if production_impl.exists():
    raise SystemExit("production PL_MPEG implementation TU already exists")
production_impl.write_text(
    "// license:BSD-3-Clause\n"
    "// copyright-holders:Matt Jordan\n\n"
    "// Keep the single-header decoder implementation in a tiny, stable\n"
    "// translation unit so ordinary DVC scheduling/register edits compile faster.\n"
    "#define PLM_NO_STDIO\n"
    "#define PL_MPEG_IMPLEMENTATION\n"
    '#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"\n',
    encoding="utf-8",
)

dvc = read("src/mame/philips/cdidvc.cpp")
hash_start = dvc.find("\t\tuint32_t frame_hash = 2166136261U;")
hash_end_marker = "\t\t\t\tm_video_decoded_frames, frame->width, frame->height, frame->time, frame_hash);\n"
hash_end = dvc.find(hash_end_marker, hash_start)
if hash_start < 0 or hash_end < 0:
    raise SystemExit("DVC diagnostic frame hash block not found")
hash_end += len(hash_end_marker)
hash_block = dvc[hash_start:hash_end]
dvc = dvc[:hash_start] + "#if (VERBOSE & LOG_VIDEO)\n" + hash_block + "#endif\n" + dvc[hash_end:]

old_selection = """\t\tstd::vector<uint64_t> timestamps90;
\t\ttimestamps90.reserve(m_video_queue.size());
\t\tfor (queued_video_frame const &queued : m_video_queue)
\t\t{
\t\t\tif (!queued.timestamp_valid)
\t\t\t\tbreak;
\t\t\ttimestamps90.push_back(queued.timestamp90);
\t\t}

\t\tclock90 = current_mpeg_clock90(MPEG_FMV);
\t\tcdi_dvc::presentation_selection const selection =
\t\t\tcdi_dvc::select_latest_due_presentation(
\t\t\t\ttimestamps90.data(), timestamps90.size(), clock90);
"""
new_selection = """\t\tclock90 = current_mpeg_clock90(MPEG_FMV);
\t\tcdi_dvc::presentation_selection selection { 0, 0, false };
\t\tfor (std::size_t index = 0; index < m_video_queue.size(); ++index)
\t\t{
\t\t\tqueued_video_frame const &queued = m_video_queue[index];
\t\t\tif (!queued.timestamp_valid
\t\t\t\t\t|| !cdi_dvc::mpeg_presentation_due(queued.timestamp90, clock90))
\t\t\t\tbreak;

\t\t\tselection.selected_index = index;
\t\t\tselection.consume_count = index + 1;
\t\t\tselection.valid = true;
\t\t}
"""
if dvc.count(old_selection) != 1:
    raise SystemExit(f"DVC timestamp-vector block expected once, got {dvc.count(old_selection)}")
dvc = dvc.replace(old_selection, new_selection, 1)
write("src/mame/philips/cdidvc.cpp", dvc)

cdic = read("src/mame/philips/cdicdic.cpp")
old_logs = (
    "#define LOG_RAM         (1U << 9)\n"
    "#define LOG_ALL         (LOG_DECODES | LOG_SAMPLES | LOG_COMMANDS | LOG_SECTORS | LOG_IRQS | LOG_READS | LOG_WRITES | LOG_UNKNOWNS | LOG_RAM)"
)
new_logs = (
    "#define LOG_RAM         (1U << 9)\n"
    "#define LOG_TRACE       (1U << 10)\n"
    "#define LOG_ALL         (LOG_DECODES | LOG_SAMPLES | LOG_COMMANDS | LOG_SECTORS | LOG_IRQS | LOG_READS | LOG_WRITES | LOG_UNKNOWNS | LOG_RAM | LOG_TRACE)"
)
if cdic.count(old_logs) != 1:
    raise SystemExit("CDIC log-mask insertion point not found")
cdic = cdic.replace(old_logs, new_logs, 1)
cdic, trace_replacements = re.subn(
    r'logerror\(\n(?=\s*"CDIC_(?:TRACE|DBUF_TRACE))',
    "LOGMASKED(LOG_TRACE,\n",
    cdic,
)
if trace_replacements < 3:
    raise SystemExit(f"expected multiple CDIC trace replacements, got {trace_replacements}")
if re.search(r'logerror\(\n\s*"CDIC_(?:TRACE|DBUF_TRACE)', cdic):
    raise SystemExit("unconditional CDIC campaign trace remains")

old_xa = """\tint16_t sampleL = 0, sampleR = 0, outL = 0, outR = 0;
\t// Green Book nominal curve.  Board-family quantization and the documented
\t// ADPCM high-attenuation anomaly remain outside this compatibility model.
\tconst double scaleLL = cdi_audio::nominal_attenuation_gain(m_atten[0]);
\tconst double scaleLR = cdi_audio::nominal_attenuation_gain(m_atten[1]);
\tconst double scaleRR = cdi_audio::nominal_attenuation_gain(m_atten[2]);
\tconst double scaleRL = cdi_audio::nominal_attenuation_gain(m_atten[3]);
\tfor (uint16_t i = 0; i < 18 * 28 * num_samples; i++)
\t{
\t\tsampleL = m_samples[0][i];
\t\tsampleR = m_samples[coding_info.channels - 1][i];
\t\tdouble const filteredL = cdi_audio::apply_50_15_deemphasis(
\t\t\tm_deemphasis[0], sampleL, sample_frequency, coding_info.emphasis);
\t\tdouble const filteredR = cdi_audio::apply_50_15_deemphasis(
\t\t\tm_deemphasis[1], sampleR, sample_frequency, coding_info.emphasis);

\t\toutL = (filteredL * scaleLL + filteredR * scaleRL) * 0.25;
\t\toutR = (filteredL * scaleLR + filteredR * scaleRR) * 0.25;
\t\tm_dmadac[0]->transfer(0, 1, 1, 1, &outL);
\t\tm_dmadac[1]->transfer(0, 1, 1, 1, &outR);
\t}
"""
new_xa = """\tint16_t sampleL = 0, sampleR = 0;
\t// Green Book nominal curve.  Board-family quantization and the documented
\t// ADPCM high-attenuation anomaly remain outside this compatibility model.
\tconst double scaleLL = cdi_audio::nominal_attenuation_gain(m_atten[0]);
\tconst double scaleLR = cdi_audio::nominal_attenuation_gain(m_atten[1]);
\tconst double scaleRR = cdi_audio::nominal_attenuation_gain(m_atten[2]);
\tconst double scaleRL = cdi_audio::nominal_attenuation_gain(m_atten[3]);
\tconst uint16_t total_samples = 18 * 28 * num_samples;
\tfor (uint16_t i = 0; i < total_samples; i++)
\t{
\t\tsampleL = m_samples[0][i];
\t\tsampleR = m_samples[coding_info.channels - 1][i];
\t\tdouble const filteredL = cdi_audio::apply_50_15_deemphasis(
\t\t\tm_deemphasis[0], sampleL, sample_frequency, coding_info.emphasis);
\t\tdouble const filteredR = cdi_audio::apply_50_15_deemphasis(
\t\t\tm_deemphasis[1], sampleR, sample_frequency, coding_info.emphasis);

\t\tm_samples[0][i] = int16_t((filteredL * scaleLL + filteredR * scaleRL) * 0.25);
\t\tm_samples[1][i] = int16_t((filteredL * scaleLR + filteredR * scaleRR) * 0.25);
\t}

\tm_dmadac[0]->transfer(0, 1, 1, total_samples, m_samples[0].get());
\tm_dmadac[1]->transfer(0, 1, 1, total_samples, m_samples[1].get());
"""
if cdic.count(old_xa) != 1:
    raise SystemExit(f"CDIC XA transfer loop expected once, got {cdic.count(old_xa)}")
cdic = cdic.replace(old_xa, new_xa, 1)
write("src/mame/philips/cdicdic.cpp", cdic)

replace_once(
    "tests/emu/philips/cdidvc.cpp",
    '#define PLM_NO_STDIO\n#define PL_MPEG_IMPLEMENTATION\n#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"',
    '#define PLM_NO_STDIO\n#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"',
    "test PL_MPEG implementation split",
)

test_impl = Path("tests/emu/philips/cdidvc_plmpeg.cpp")
if test_impl.exists():
    raise SystemExit("test PL_MPEG implementation TU already exists")
test_impl.write_text(
    "// license:BSD-3-Clause\n"
    "// copyright-holders:Matt Jordan\n\n"
    "// Keep the single-header decoder implementation in a tiny, stable\n"
    "// translation unit so ordinary DVC test edits compile faster.\n"
    "#define PLM_NO_STDIO\n"
    "#define PL_MPEG_IMPLEMENTATION\n"
    '#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"\n',
    encoding="utf-8",
)

replace_once(
    "scripts/src/tests.lua",
    '\t\tMAME_DIR .. "tests/emu/philips/cdidvc.cpp",\n',
    '\t\tMAME_DIR .. "tests/emu/philips/cdidvc.cpp",\n'
    '\t\tMAME_DIR .. "tests/emu/philips/cdidvc_plmpeg.cpp",\n',
    "mametests PL_MPEG implementation TU",
)

production = sum(
    read(path).count("#define PL_MPEG_IMPLEMENTATION")
    for path in ["src/mame/philips/cdidvc.cpp", "src/mame/philips/cdidvc_plmpeg.cpp"]
)
tests = sum(
    read(path).count("#define PL_MPEG_IMPLEMENTATION")
    for path in ["tests/emu/philips/cdidvc.cpp", "tests/emu/philips/cdidvc_plmpeg.cpp"]
)
if production != 1 or tests != 1:
    raise SystemExit(f"PL_MPEG ownership invalid: production={production}, tests={tests}")
