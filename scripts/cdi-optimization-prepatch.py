from pathlib import Path

path = Path("src/mame/philips/cdidvc.cpp")
text = path.read_text(encoding="utf-8")
old = "\tm_video_rgb24.clear();\n"
count = text.count(old)
if count != 2:
    raise SystemExit(f"DVC RGB24 reset sites: expected exactly two matches, got {count}")
path.write_text(text.replace(old, ""), encoding="utf-8")

# The branch already carries a deliberately inert staging TU so normal builds
# remain valid while the one-shot optimization batch waits for a runner.  The
# main patch script creates the final implementation-owning TU, so remove only
# that exact staging form here.  Refuse to delete anything unexpected.
production_impl = Path("src/mame/philips/cdidvc_plmpeg.cpp")
staged_impl = (
    "// license:BSD-3-Clause\n"
    "// copyright-holders:Matt Jordan\n\n"
    "// Staging translation unit for the DVC PL_MPEG implementation split.\n"
    "// The implementation remains owned by cdidvc.cpp until the source-side\n"
    "// definition is removed in the accompanying optimization commit.\n"
    "#define PLM_NO_STDIO\n"
    '#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"\n'
)
if production_impl.exists():
    current = production_impl.read_text(encoding="utf-8")
    if current != staged_impl:
        raise SystemExit("unexpected existing production PL_MPEG staging TU")
    production_impl.unlink()
