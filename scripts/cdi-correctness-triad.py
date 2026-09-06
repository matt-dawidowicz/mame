#!/usr/bin/env python3
"""Apply the three audited CD-i correctness fixes without whole-file replacement.

This script is deliberately fail-closed.  Every source transformation must match
exactly once (or, for scoped log-address rewrites, at least once inside the audited
function body).  It never stages, commits, resets, stashes, or cleans the worktree.
"""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def function_slice(text: str, start: str, end: str, label: str) -> tuple[str, int, int]:
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"{label}: start marker not found")
    finish = text.find(end, begin + len(start))
    if finish < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[begin:finish], begin, finish


def replace_scoped_function(text: str, start: str, end: str, transform, label: str) -> str:
    body, begin, finish = function_slice(text, start, end, label)
    changed = transform(body)
    if changed == body:
        raise RuntimeError(f"{label}: transformation made no change")
    return text[:begin] + changed + text[finish:]


def patch_mcd212() -> None:
    path = "src/mame/philips/mcd212.cpp"
    text = read(path)

    text = replace_once(
        text,
        '#include "mcd212.h"\n#include "mcd212_video.h"\n',
        '#include "mcd212.h"\n#include "mcd212_control_stream.h"\n#include "mcd212_video.h"\n',
        "MCD212 include")

    def patch_ica(body: str) -> str:
        body = replace_once(
            body,
            'uint32_t addr = mcd212_video::ica_pointer_word_offset(BIT(m_csrr[0], CSR1R_PA_BIT));',
            'uint32_t addr = mcd212_control::word_index(\n'
            '\t\tmcd212_video::ica_pointer_word_offset(BIT(m_csrr[0], CSR1R_PA_BIT)));',
            "MCD212 ICA initial address")
        body = replace_once(
            body,
            '\t\tuint32_t cmd = ica[addr++] << 16;\n\t\tcmd |= ica[addr++];',
            '\t\tauto const fetch = mcd212_control::command_words(addr);\n'
            '\t\tuint32_t const command_addr = fetch.first_word;\n'
            '\t\tuint32_t cmd = uint32_t(ica[fetch.first_word]) << 16;\n'
            '\t\tcmd |= ica[fetch.second_word];\n'
            '\t\taddr = fetch.next_word;',
            "MCD212 ICA command fetch")
        body = replace_once(
            body,
            '\t\t\t\taddr = (cmd & 0x0007ffff) / 2;',
            '\t\t\t\taddr = mcd212_control::word_address_from_byte(cmd & 0x0007ffff);',
            "MCD212 ICA reload address")
        old_log = '(addr - 2) * 2 + Path * 0x200000'
        count = body.count(old_log)
        if count == 0:
            raise RuntimeError("MCD212 ICA log address: no audited occurrences found")
        body = body.replace(old_log, 'command_addr * 2 + Path * 0x200000')
        return body

    text = replace_scoped_function(
        text,
        'template <int Path>\nvoid mcd212_device::process_ica()',
        'template <int Path>\nvoid mcd212_device::process_dca()',
        patch_ica,
        "MCD212 ICA")

    def patch_dca(body: str) -> str:
        body = replace_once(
            body,
            '\tuint32_t addr = (m_dca[Path] & 0x0007ffff) / 2;',
            '\tuint32_t addr = mcd212_control::word_address_from_byte(m_dca[Path] & 0x0007ffff);',
            "MCD212 DCA initial address")
        body = replace_once(
            body,
            '\tbool addr_changed = false;\n',
            '',
            "MCD212 DCA dead address flag")
        body = replace_once(
            body,
            '\t\tcmd = dca[addr++] << 16;\n\t\tcmd |= dca[addr++];',
            '\t\tauto const fetch = mcd212_control::command_words(addr);\n'
            '\t\tuint32_t const command_addr = fetch.first_word;\n'
            '\t\tcmd = uint32_t(dca[fetch.first_word]) << 16;\n'
            '\t\tcmd |= dca[fetch.second_word];\n'
            '\t\taddr = fetch.next_word;',
            "MCD212 DCA command fetch")
        old_log = '(addr - 2) * 2 + Path * 0x200000'
        count = body.count(old_log)
        if count == 0:
            raise RuntimeError("MCD212 DCA log address: no audited occurrences found")
        body = body.replace(old_log, 'command_addr * 2 + Path * 0x200000')
        body = replace_once(
            body,
            '\tif (!addr_changed)\n\t{\n\t\taddr += (max - count) >> 1;\n\t}\n\n\tm_dca[Path] = addr * 2;',
            '\taddr = mcd212_control::advance_word(addr, (max - count) >> 1);\n\n'
            '\tm_dca[Path] = addr * 2;',
            "MCD212 DCA command-window advance")
        return body

    text = replace_scoped_function(
        text,
        'template <int Path>\nvoid mcd212_device::process_dca()',
        'template <int Path>\nstatic inline uint8_t BYTE_TO_CLUT',
        patch_dca,
        "MCD212 DCA")

    if 'ica[addr++]' in text or 'dca[addr++]' in text:
        raise RuntimeError("MCD212: raw post-increment control-stream fetch remains")

    write(path, text)


def patch_dvc() -> None:
    path = "src/mame/philips/cdidvc.cpp"
    text = read(path)

    old = '''\tm_mpeg_schedule_play_delta90[target] = cdi_dvc::mpeg_timestamp_delta(play_ts, m_mpeg_clock90);\n\tm_mpeg_schedule_decode_delta90[target] = cdi_dvc::mpeg_timestamp_delta(decode_ts, m_mpeg_clock90);\n\tm_mpeg_schedule_play_delta45[target] = cdi_dvc::mpeg_dclk_delta(play_ts, m_mpeg_clock90);\n\tm_mpeg_schedule_decode_delta45[target] = cdi_dvc::mpeg_dclk_delta(decode_ts, m_mpeg_clock90);'''
    new = '''\tuint64_t const clock90 = current_mpeg_clock90(target);\n\tauto const schedule = cdi_dvc::measure_packet_schedule(play_ts, decode_ts, clock90);\n\tm_mpeg_schedule_play_delta90[target] = schedule.play90;\n\tm_mpeg_schedule_decode_delta90[target] = schedule.decode90;\n\tm_mpeg_schedule_play_delta45[target] = schedule.play45;\n\tm_mpeg_schedule_decode_delta45[target] = schedule.decode45;'''
    text = replace_once(text, old, new, "DVC live packet clock")

    text = replace_once(
        text,
        '"%s: DVC MPEG SCHED %s scr=%llu pts=%llu dts=%llu explicit_dts=%u play90=%lld decode90=%lld play45=%d decode45=%d event=%u\\n",',
        '"%s: DVC MPEG SCHED %s clock=%llu pts=%llu dts=%llu explicit_dts=%u play90=%lld decode90=%lld play45=%d decode45=%d event=%u\\n",',
        "DVC schedule log label")
    text = replace_once(
        text,
        '\t\t\t(unsigned long long)m_mpeg_clock90,\n\t\t\t(unsigned long long)m_mpeg_packet_pts[target],',
        '\t\t\t(unsigned long long)clock90,\n\t\t\t(unsigned long long)m_mpeg_packet_pts[target],',
        "DVC schedule log clock")

    write(path, text)


def patch_cdic() -> None:
    path = "src/mame/philips/cdicdic.cpp"
    text = read(path)

    text = replace_once(
        text,
        '#include "cdicdic.h"\n#include "cdiaudio.h"\n',
        '#include "cdicdic.h"\n#include "cdiaudio.h"\n#include "cdicdic_memory.h"\n',
        "CDIC memory helper include")

    old_dma = '''\t\t\tuint32_t device_index = (m_dma_control & 0x3fff) >> 1;\n\t\t\tuint16_t *ram = (uint16_t *)m_ram.get();\n\n\t\t\t// SCC68070 channel 1 owns the memory-side DMA cycle.\n\t\t\t// CDIC supplies or consumes only the device-side operand.\n\t\t\twhile (m_scc->dma_channel1_active())\n\t\t\t{\n\t\t\t\tuint16_t operand = ram[device_index];\n\n\t\t\t\tif (!m_scc->dma_channel1_transfer(operand))\n\t\t\t\t\tbreak;\n\n\t\t\t\tram[device_index++] = operand;\n\t\t\t}'''
    new_dma = '''\t\t\tuint32_t device_index = (m_dma_control & 0x3fff) >> 1;\n\n\t\t\t// SCC68070 channel 1 owns the memory-side DMA cycle.\n\t\t\t// CDIC supplies or consumes only the device-side operand.\n\t\t\twhile (m_scc->dma_channel1_active())\n\t\t\t{\n\t\t\t\tuint8_t *const ram_word = &m_ram[device_index << 1];\n\t\t\t\tuint16_t operand = cdic_hle::read_ram_word(ram_word);\n\n\t\t\t\tif (!m_scc->dma_channel1_transfer(operand))\n\t\t\t\t\tbreak;\n\n\t\t\t\tcdic_hle::write_ram_word(ram_word, operand);\n\t\t\t\t++device_index;\n\t\t\t}'''
    text = replace_once(text, old_dma, new_dma, "CDIC DMA RAM access")

    old_sector = '''\tuint16_t *dev_buffer = reinterpret_cast<uint16_t *>(&m_ram[completion.byte_offset]);\n\n\tif (!cdic_hle::stores_sector_payload_in_ram(cdic_hle::disc_operation(m_disc_mode)))\n\t{\n\t\t// Mono-I captures show that CD-DA PCM bypasses CDIC RAM.  Only its\n\t\t// subcode is written at byte offset $924 in the alternating buffers.\n\t\tdev_buffer += cdic_hle::CDIC_SUBCODE_BYTE_OFFSET / 2;\n\t}\n\telse\n\t{\n\t\tfor (int i = SECTOR_HEADER; i < SECTOR_FILE2; i += 2)\n\t\t\t*dev_buffer++ = ((uint16_t)buffer[i] << 8) | buffer[i + 1];\n\n\t\tfor (int i = SECTOR_FILE2; i < SECTOR_SIZE; i += 2)\n\t\t\t*dev_buffer++ = ((uint16_t)buffer[i] << 8) | buffer[i + 1];\n\t}\n\n\tfor (int i = SUBCODE_Q_CONTROL; i <= SUBCODE_Q_CRC1; i++)\n\t\t*dev_buffer++ = subcode_buffer[i];'''
    new_sector = '''\tuint32_t dev_buffer = completion.byte_offset;\n\n\tif (!cdic_hle::stores_sector_payload_in_ram(cdic_hle::disc_operation(m_disc_mode)))\n\t{\n\t\t// Mono-I captures show that CD-DA PCM bypasses CDIC RAM.  Only its\n\t\t// subcode is written at byte offset $924 in the alternating buffers.\n\t\tdev_buffer += cdic_hle::CDIC_SUBCODE_BYTE_OFFSET;\n\t}\n\telse\n\t{\n\t\tfor (int i = SECTOR_HEADER; i < SECTOR_FILE2; i += 2)\n\t\t{\n\t\t\tcdic_hle::write_ram_word(&m_ram[dev_buffer], uint16_t((uint16_t(buffer[i]) << 8) | buffer[i + 1]));\n\t\t\tdev_buffer += 2;\n\t\t}\n\n\t\tfor (int i = SECTOR_FILE2; i < SECTOR_SIZE; i += 2)\n\t\t{\n\t\t\tcdic_hle::write_ram_word(&m_ram[dev_buffer], uint16_t((uint16_t(buffer[i]) << 8) | buffer[i + 1]));\n\t\t\tdev_buffer += 2;\n\t\t}\n\t}\n\n\tfor (int i = SUBCODE_Q_CONTROL; i <= SUBCODE_Q_CRC1; i++)\n\t{\n\t\tcdic_hle::write_ram_word(&m_ram[dev_buffer], subcode_buffer[i]);\n\t\tdev_buffer += 2;\n\t}'''
    text = replace_once(text, old_sector, new_sector, "CDIC sector RAM writes")

    old_ram_w = '''\tCOMBINE_DATA((uint16_t *)&m_ram[offset << 1]);'''
    new_ram_w = '''\tcdic_hle::combine_ram_word(&m_ram[offset << 1], data, mem_mask);'''
    text = replace_once(text, old_ram_w, new_ram_w, "CDIC mapped RAM write")

    old_ram_r = '''\tconst uint16_t data = ((uint16_t)m_ram[(offset << 1) + 1] << 8) | m_ram[offset << 1];'''
    new_ram_r = '''\tconst uint16_t data = cdic_hle::read_ram_word(&m_ram[offset << 1]);'''
    text = replace_once(text, old_ram_r, new_ram_r, "CDIC mapped RAM read")

    forbidden = (
        '(uint16_t *)m_ram.get()',
        '(uint16_t *)&m_ram[',
        'reinterpret_cast<uint16_t *>(&m_ram[',
    )
    for token in forbidden:
        if token in text:
            raise RuntimeError(f"CDIC: host-native RAM access remains: {token}")

    write(path, text)


def main() -> None:
    patch_mcd212()
    patch_dvc()
    patch_cdic()
    print("Applied MCD212 control-stream, DVC live-clock, and CDIC RAM portability fixes.")


if __name__ == "__main__":
    main()
