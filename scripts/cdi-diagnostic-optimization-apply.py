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
replace_once(old_audio, new_audio, "DVC audio output diagnostic hash")

old_decode_hash_decl = '''\t\tuint32_t hash = 2166136261U;
\t\tsize_t const values = size_t(samples->count) * 2;
'''
new_decode_hash_decl = '''#if (VERBOSE & LOG_AUDIO)
\t\tuint32_t hash = 2166136261U;
#endif
\t\tsize_t const values = size_t(samples->count) * 2;
'''
replace_once(old_decode_hash_decl, new_decode_hash_decl, "DVC decoded PCM hash declaration")

old_decode_hash_update = '''\t\t\tint16_t const pcm = cdi_dvc::quantize_plm_audio_sample(float(filtered));
\t\t\thash = cdi_dvc::hash_pcm16_sample(hash, pcm);
\t\t\tm_audio_pcm_queue.push_back(pcm);
'''
new_decode_hash_update = '''\t\t\tint16_t const pcm = cdi_dvc::quantize_plm_audio_sample(float(filtered));
#if (VERBOSE & LOG_AUDIO)
\t\t\thash = cdi_dvc::hash_pcm16_sample(hash, pcm);
#endif
\t\t\tm_audio_pcm_queue.push_back(pcm);
'''
replace_once(old_decode_hash_update, new_decode_hash_update, "DVC decoded PCM hash update")

old_decode_hash_log = '''\t\tLOGMASKED(LOG_AUDIO, "%s: DVC AUDIO decoded frames=%u frame_samples=%u total_samples=%u event=%u status=%02x fnv=%08x\\n",
\t\t\t\tmachine().describe_context(), m_audio_decoded_frames, samples->count,
\t\t\t\tm_audio_decoded_samples, m_audio_decode_events,
\t\t\t\tm_audio_backend_status, hash);
'''
new_decode_hash_log = '''#if (VERBOSE & LOG_AUDIO)
\t\tLOGMASKED(LOG_AUDIO, "%s: DVC AUDIO decoded frames=%u frame_samples=%u total_samples=%u event=%u status=%02x fnv=%08x\\n",
\t\t\t\tmachine().describe_context(), m_audio_decoded_frames, samples->count,
\t\t\t\tm_audio_decoded_samples, m_audio_decode_events,
\t\t\t\tm_audio_backend_status, hash);
#endif
'''
replace_once(old_decode_hash_log, new_decode_hash_log, "DVC decoded PCM hash log")

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
\t\t\t++m_video_overlay_pixels;
#endif
'''
replace_once(old_video, new_video, "DVC overlay diagnostic hash")

old_complete = '''\tif (!m_video_overlay_complete && physical_y == dst_y + int(geometry.output_height) - 1 && m_video_overlay_pixels)
\t{
\t\tm_video_overlay_complete = true;
\t\tLOGMASKED(LOG_VIDEO, "%s: DVC VIDEO overlay complete frame=%u crop=%u,%u window=%ux%u dst=%d,%d pixels=%u fnv=%08x\\n",
\t\t\t\tmachine().describe_context(), m_video_present_generation,
\t\t\t\tm_video_crop_x, m_video_crop_y, window_w, window_h,
\t\t\t\tdst_x, dst_y, m_video_overlay_pixels, m_video_overlay_hash);
\t}
'''
new_complete = '''#if (VERBOSE & LOG_VIDEO)
\tif (!m_video_overlay_complete && physical_y == dst_y + int(geometry.output_height) - 1 && m_video_overlay_pixels)
\t{
\t\tm_video_overlay_complete = true;
\t\tLOGMASKED(LOG_VIDEO, "%s: DVC VIDEO overlay complete frame=%u crop=%u,%u window=%ux%u dst=%d,%d pixels=%u fnv=%08x\\n",
\t\t\t\tmachine().describe_context(), m_video_present_generation,
\t\t\t\tm_video_crop_x, m_video_crop_y, window_w, window_h,
\t\t\t\tdst_x, dst_y, m_video_overlay_pixels, m_video_overlay_hash);
\t}
#endif
'''
replace_once(old_complete, new_complete, "DVC overlay completion diagnostic")

path.write_text(text, encoding="utf-8")
