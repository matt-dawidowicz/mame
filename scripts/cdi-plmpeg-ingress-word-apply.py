#!/usr/bin/env python3
from pathlib import Path
import re

cpp_path = Path("src/mame/philips/cdidvc.cpp")
cpp = cpp_path.read_text(encoding="utf-8")

# The first guarded transformation was applied through re.sub(), whose
# replacement-string processing converted the moved C++ "\\n" escapes into
# literal source newlines.  Repair only that impossible-in-normal-C++ shape:
# an ordinary quoted string split immediately before its closing quote.
pattern = re.compile(r'"([^"\n]*)\n"')
cpp, count = pattern.subn(lambda match: '"' + match.group(1) + r'\n"', cpp)
if count != 11:
    raise SystemExit(f"expected 11 generated string escape repairs, got {count}")
if pattern.search(cpp):
    raise SystemExit("generated string escape repair incomplete")

# The production algorithm must already be present; this repair must never
# silently serve as a second implementation pass.
for token in (
    "audio_decoder_feed_word",
    "video_decoder_feed_word",
    "mpeg_payload_word",
    "m_mpeg_packet_remaining[target] >= 2",
):
    if token not in cpp:
        raise SystemExit(f"missing production token: {token}")

cpp_path.write_text(cpp, encoding="utf-8")
print(f"Repaired {count} generated C++ newline escapes")
