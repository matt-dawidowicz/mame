#!/usr/bin/env python3
from pathlib import Path

path = Path("src/mame/philips/cdicdic.cpp")
text = path.read_text()
old = """\t\t\twhile (m_scc->dma_channel1_active())
\t\t\t{
\t\t\t\tuint8_t *const ram_word = &m_ram[device_index << 1];
\t\t\t\tuint16_t operand = cdic_hle::read_ram_word(ram_word);

\t\t\t\tif (!m_scc->dma_channel1_transfer(operand))
\t\t\t\t\tbreak;

\t\t\t\tcdic_hle::write_ram_word(ram_word, operand);
\t\t\t\t++device_index;
\t\t\t}
"""
new = """\t\t\twhile (m_scc->dma_channel1_active())
\t\t\t{
\t\t\t\t// DMACTL exposes a 14-bit CDIC byte address.  The HLE owns exactly
\t\t\t\t// 0x4000 bytes of SRAM, so an incrementing transfer that reaches the
\t\t\t\t// word after 0x3ffe must not form or dereference an out-of-allocation
\t\t\t\t// pointer.  Report a device-side bus error through the SCC controller
\t\t\t\t// as the conservative emulator safety policy.  Physical CDIC behavior
\t\t\t\t// (wrap, clip, or error) remains unmeasured and is not asserted here.
\t\t\t\tif (device_index >= 0x2000)
\t\t\t\t{
\t\t\t\t\tm_scc->dma_channel_device_bus_error(0);
\t\t\t\t\tbreak;
\t\t\t\t}

\t\t\t\tuint8_t *const ram_word = &m_ram[device_index << 1];
\t\t\t\tuint16_t operand = cdic_hle::read_ram_word(ram_word);

\t\t\t\tif (!m_scc->dma_channel1_transfer(operand))
\t\t\t\t\tbreak;

\t\t\t\tcdic_hle::write_ram_word(ram_word, operand);
\t\t\t\t++device_index;
\t\t\t}
"""
count = text.count(old)
if count != 1:
    raise SystemExit(f"expected one CDIC DMA loop, found {count}")
path.write_text(text.replace(old, new))
