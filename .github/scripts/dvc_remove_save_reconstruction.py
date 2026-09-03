#!/usr/bin/env python3

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / "src/mame/philips/cdidvc.cpp"
HDR = ROOT / "src/mame/philips/cdidvc.h"
SAVE_HDR = ROOT / "src/mame/philips/cdidvc_save_state.h"


def sub_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return updated


cpp = CPP.read_text(encoding="utf-8")
hdr = HDR.read_text(encoding="utf-8")

cpp = cpp.replace('#include "cdidvc_save_state.h"\n', '')
if '#include "cdidvc_save_state.h"' in cpp:
    raise RuntimeError("DVC save-state include survived removal")

cpp = cpp.replace(
    "    decoder/presentation save-state reconstruction is still incomplete; and\n",
    "    save-state support is intentionally omitted from the initial upstream series; and\n",
)

# Remove fixed replay/presentation mirrors and pre/post-save callbacks, while
# retaining the ordinary scalar save_item registrations above them.
cpp = sub_once(
    cpp,
    r"\n\tm_save_audio_replay = std::make_unique<uint8_t\[\]>\(cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY\);.*?"
    r"\n\tmachine\(\)\.save\(\)\.register_postload\(save_prepost_delegate\(FUNC\(cdi_dvc_device::save_state_postload\), this\)\);\n",
    "\n",
    "device_start save mirrors",
)

# All decoder reconstruction routines are contiguous and precede device_stop.
cpp = sub_once(
    cpp,
    r"\nvoid cdi_dvc_device::save_state_presave\(\)\n\{.*?(?=\nvoid cdi_dvc_device::device_stop\(\))",
    "\n",
    "save reconstruction routines",
)

# Remove replay-only state resets.
for snippet, label in [
    ("\tm_audio_replay_journal.clear();\n\tm_audio_replay_overflow = false;\n", "audio replay reset"),
    (
        "\tm_video_replay_journal.clear();\n"
        "\tm_video_replay_pump_events.clear();\n"
        "\tm_video_replay_overflow = false;\n"
        "\tm_video_replay_pump_overflow = false;\n",
        "video replay reset",
    ),
]:
    count = cpp.count(snippet)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    cpp = cpp.replace(snippet, "")

# Remove elementary-stream journaling from the live decoder feed paths.
cpp = sub_once(
    cpp,
    r"\n\tif \(m_audio_replay_journal\.size\(\) < cdi_dvc::SAVE_AUDIO_REPLAY_CAPACITY\)\n\t\{.*?"
    r"\n\t\}\n\n(?=\tm_audio_header_shift =)",
    "\n",
    "audio replay feed",
)
cpp = sub_once(
    cpp,
    r"\n\tif \(m_video_replay_journal\.size\(\) < cdi_dvc::SAVE_VIDEO_REPLAY_CAPACITY\)\n\t\{.*?"
    r"\n\t\}\n\n(?=\tif \(m_video_picture_header_bytes\))",
    "\n",
    "video replay feed",
)

# Remove the two replay-pump journal sites without changing decoder control flow.
cpp = sub_once(
    cpp,
    r"\n\t\tif \(end_signalled\n\t\t\t\t&& m_video_replay_pump_events\.size\(\) < cdi_dvc::SAVE_VIDEO_REPLAY_PUMP_EVENTS\)"
    r"\n\t\t\{.*?\n\t\t\}",
    "",
    "early video replay pump",
)
cpp = sub_once(
    cpp,
    r"\n\tif \(decoded_before != m_video_decoded_frames \|\| end_signalled\)\n\t\{.*?\n\t\}\n"
    r"(?=\tstd::size_t const buffered_after)",
    "\n",
    "video replay pump journal",
)

# Header: remove save-only include, method declarations, dynamic replay state,
# and <memory> once unique_ptr mirrors are gone.
hdr = hdr.replace('#include "cdidvc_save_state.h"\n\n', '')
hdr = hdr.replace('#include <memory>\n', '')
hdr = sub_once(
    hdr,
    r"\n\t// SAVE-STATE IMPLEMENTATION MODEL: dynamic queues are mirrored into.*?"
    r"\n\tbool save_state_rebuild_video_decoder\(\);\n",
    "\n",
    "save method declarations",
)
hdr = sub_once(
    hdr,
    r"\n\t// SAVE-STATE IMPLEMENTATION MODEL, NOT HARDWARE SPECIFICATION\..*?"
    r"(?=\n\t// 512 KiB MPEG/DVC RAM at E80000-EFFFFF\.)",
    "\n",
    "save state members",
)

# Production source must no longer depend on save-replay implementation state.
banned = [
    "cdidvc_save_state.h",
    "save_state_presave",
    "save_state_postload",
    "save_state_restore_failed",
    "save_state_rebuild_",
    "m_save_",
    "m_audio_replay_",
    "m_video_replay_",
    "SAVE_AUDIO_",
    "SAVE_VIDEO_",
]
for token in banned:
    if token in cpp or token in hdr:
        raise RuntimeError(f"save-reconstruction token survived: {token}")

CPP.write_text(cpp, encoding="utf-8")
HDR.write_text(hdr, encoding="utf-8")
if not SAVE_HDR.exists():
    raise RuntimeError("expected cdidvc_save_state.h to exist before removal")
SAVE_HDR.unlink()

print("DVC save-state reconstruction removed cleanly")
