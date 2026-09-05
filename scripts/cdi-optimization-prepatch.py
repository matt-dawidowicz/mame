from pathlib import Path

path = Path("src/mame/philips/cdidvc.cpp")
text = path.read_text(encoding="utf-8")
old = "\tm_video_rgb24.clear();\n"
count = text.count(old)
if count != 2:
    raise SystemExit(f"DVC RGB24 reset sites: expected exactly two matches, got {count}")
path.write_text(text.replace(old, ""), encoding="utf-8")
