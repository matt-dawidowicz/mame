#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Matt Jordan
"""Generate all large CD-i DVC certification fixtures outside C++ source."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
commands = [
    [sys.executable, str(root/'scripts/cdi_generate_av_reference.py'), '--root', str(root)],
    [sys.executable, str(root/'scripts/cdi_generate_motion_reference.py'), '--root', str(root)],
    [sys.executable, str(root/'scripts/cdi_generate_motion_reference.py'), '--root', str(root), '--full-size'],
]
for command in commands:
    subprocess.run(command, check=True)
print(root/'tests/emu/philips/fixtures')
