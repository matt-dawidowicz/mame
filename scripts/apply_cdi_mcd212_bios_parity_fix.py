#!/usr/bin/env python3
from pathlib import Path

path = Path("src/mame/philips/mcd212.cpp")
text = path.read_text(encoding="utf-8")

old_tick = """TIMER_CALLBACK_MEMBER(mcd212_device::ica_tick)\n{\n\tm_csrr[0] &= ~CSR1R_DA;\n"""
new_tick = """TIMER_CALLBACK_MEMBER(mcd212_device::ica_tick)\n{\n\t// PA is a field-timing status bit, not rendering state.  Advance it at the\n\t// field boundary so firmware sees parity changes even when blanking rows are\n\t// outside the screen's visible clip rectangle.\n\tm_csrr[0] ^= CSR1R_PA;\n\tm_csrr[0] &= ~CSR1R_DA;\n"""

old_render = """\n\t\t// Toggle frame parity at the end of the visible frame (even in non-interlaced mode).\n\t\tif (scanline == (m_total_height - 1))\n\t\t{\n\t\t\tm_csrr[0] ^= CSR1R_PA;\n\t\t}\n"""

if text.count(old_tick) != 1:
    raise SystemExit(f"expected exactly one ICA tick anchor, found {text.count(old_tick)}")
if text.count(old_render) != 1:
    raise SystemExit(f"expected exactly one renderer parity block, found {text.count(old_render)}")

text = text.replace(old_tick, new_tick, 1)
text = text.replace(old_render, "", 1)
path.write_text(text, encoding="utf-8")

print(f"patched {path}")
print("MCD212 PA now advances in ica_tick instead of screen_update")
