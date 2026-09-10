#!/usr/bin/env python3
"""Apply the bounded 2026-09-10 CD-i consolidation/performance batch.

This script is intentionally exact-match guarded.  It exists to make the large
source edits reproducible through GitHub Actions without silently rewriting a
newer source shape.
"""
from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, got {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/mame/philips/cdidvc.cpp",
    '''\t\tcdi_audio::stereo_sample const mixed = cdi_audio::mix_attenuated_stereo(\n\t\t\t\tattenuation,\n\t\t\t\tdouble(output.left) / 32768.0,\n\t\t\t\tdouble(output.right) / 32768.0);\n\n\t\t// Clamp only at MAME's normalized output boundary.  This prevents an\n\t\t// over-range four-path mix escaping the sound stream, but is not a claim\n\t\t// about the DSP56001 accumulator width or its final rounding circuit.\n\t\tstream.put_clamp(0, i, mixed.left);\n\t\tstream.put_clamp(1, i, mixed.right);\n''',
    '''\t\t// The queue is already signed 16-bit PCM.  Stay in that domain for the\n\t\t// firmware-derived Q22 matrix instead of normalizing to double only for\n\t\t// mix_attenuated_stereo() to quantize the exact same values back to int16.\n\t\tcdi_audio::stereo_pcm16 const mixed = cdi_audio::mix_fma_attenuated_pcm16(\n\t\t\t\tattenuation, output.left, output.right);\n\t\tconstexpr double pcm16_normalization = 1.0 / 32768.0;\n\n\t\t// Clamp only at MAME's normalized output boundary.  This prevents an\n\t\t// over-range four-path mix escaping the sound stream, but is not a claim\n\t\t// about the DSP56001 accumulator width or its final rounding circuit.\n\t\tstream.put_clamp(0, i, double(mixed.left) * pcm16_normalization);\n\t\tstream.put_clamp(1, i, double(mixed.right) * pcm16_normalization);\n''',
    "DVC int16/double/int16 audio round-trip",
)

replace_once(
    "src/mame/philips/cdicdic.cpp",
    '''\t\tfor (int i = SECTOR_HEADER; i < SECTOR_FILE2; i += 2)\n\t\t{\n\t\t\tcdic_hle::write_ram_word(&m_ram[dev_buffer], uint16_t((uint16_t(buffer[i]) << 8) | buffer[i + 1]));\n\t\t\tdev_buffer += 2;\n\t\t}\n\n\t\tfor (int i = SECTOR_FILE2; i < SECTOR_SIZE; i += 2)\n\t\t{\n\t\t\tcdic_hle::write_ram_word(&m_ram[dev_buffer], uint16_t((uint16_t(buffer[i]) << 8) | buffer[i + 1]));\n\t\t\tdev_buffer += 2;\n\t\t}\n''',
    '''\t\t// Header and payload are contiguous here; the former split loops had\n\t\t// identical bodies and no semantic boundary at SECTOR_FILE2.\n\t\tfor (int i = SECTOR_HEADER; i < SECTOR_SIZE; i += 2)\n\t\t{\n\t\t\tcdic_hle::write_ram_word(&m_ram[dev_buffer], uint16_t((uint16_t(buffer[i]) << 8) | buffer[i + 1]));\n\t\t\tdev_buffer += 2;\n\t\t}\n''',
    "CDIC duplicate contiguous sector-copy loops",
)

print("Applied CD-i bounded consolidation/performance batch")
