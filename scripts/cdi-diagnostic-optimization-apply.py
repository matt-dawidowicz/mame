from pathlib import Path

path = Path("src/mame/philips/cdidvc.cpp")
text = path.read_text(encoding="utf-8")

def replace_once(old, new, label):
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, got {count}")
    text = text.replace(old, new, 1)

old_audio = '''\t\t\tm_audio_output_hash = cdi_dvc::hash_pcm16_sample(
\t\t\t\t\tm_audio_output_hash, output.left);
\t\t\tm_audio_output_hash = cdi_dvc::hash_pcm16_sample(
\t\t\t\t\tm_audio_output_hash, output.right);
\t\t\t++m_audio_output_frames;
\t\t\tif (output.left != 0 || output.right != 0)
\t\t\t\t++m_audio_output_nonzero;
'''
new_audio = '''#if (VERBOSE & LOG_AUDIO)
\t\t\tm_audio_output_hash = cdi_dvc::hash_pcm16_sample(
\t\t\t\t\tm_audio_output_hash, output.left);
\t\t\tm_audio_output_hash = cdi_dvc::hash_pcm16_sample(
\t\t\t\t\tm_audio_output_hash, output.right);
#endif
\t\t\t++m_audio_output_frames;
#if (VERBOSE & LOG_AUDIO)
\t\t\tif (output.left != 0 || output.right != 0)
\t\t\t\t++m_audio_output_nonzero;
#endif
'''
replace_once(old_audio, new_audio, "DVC audio diagnostic hash")

old_video = '''\t\t\tpixels[out_x] = color;
\t\t\tuint8_t const r = uint8_t(color >> 16);
\t\t\tuint8_t const g = uint8_t(color >> 8);
\t\t\tuint8_t const b = uint8_t(color);
\t\t\tm_video_overlay_hash ^= r;
\t\t\tm_video_overlay_hash *= 16777619U;
\t\t\tm_video_overlay_hash ^= g;
\t\t\tm_video_overlay_hash *= 16777619U;
\t\t\tm_video_overlay_hash ^= b;
\t\t\tm_video_overlay_hash *= 16777619U;
\t\t\t++m_video_overlay_pixels;
'''
new_video = '''\t\t\tpixels[out_x] = color;
#if (VERBOSE & LOG_VIDEO)
\t\t\tuint8_t const r = uint8_t(color >> 16);
\t\t\tuint8_t const g = uint8_t(color >> 8);
\t\t\tuint8_t const b = uint8_t(color);
\t\t\tm_video_overlay_hash ^= r;
\t\t\tm_video_overlay_hash *= 16777619U;
\t\t\tm_video_overlay_hash ^= g;
\t\t\tm_video_overlay_hash *= 16777619U;
\t\t\tm_video_overlay_hash ^= b;
\t\t\tm_video_overlay_hash *= 16777619U;
#endif
\t\t\t++m_video_overlay_pixels;
'''
replace_once(old_video, new_video, "DVC overlay diagnostic hash")

path.write_text(text, encoding="utf-8")
