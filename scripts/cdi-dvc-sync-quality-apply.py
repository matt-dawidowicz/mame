#!/usr/bin/env python3
"""Apply the bounded CD-i DVC sync/presentation-quality patch."""
from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, got {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/mame/philips/cdidvc.cpp",
    '#include "cdidvc_fidelity.h"\n',
    '#include "cdidvc_fidelity.h"\n#include "cdidvc_presentation.h"\n',
    "presentation helper include",
)

replace_once(
    "src/mame/philips/cdidvc.h",
    "\tuint32_t m_fmv_dclk_offset = 0;\n\tuint16_t m_fma_dclk_latch = 0;\n",
    "\tuint32_t m_fmv_dclk_offset = 0;\n\tbool m_fmv_syscr_programmed = false;\n\tuint16_t m_fma_dclk_latch = 0;\n",
    "GEN_SYSCR programmed state",
)

replace_once(
    "src/mame/philips/cdidvc.cpp",
    "\tsave_item(NAME(m_fmv_dclk_offset));\n\tsave_item(NAME(m_fma_dclk_latch));\n",
    "\tsave_item(NAME(m_fmv_dclk_offset));\n\tsave_item(NAME(m_fmv_syscr_programmed));\n\tsave_item(NAME(m_fma_dclk_latch));\n",
    "save GEN_SYSCR programmed state",
)

replace_once(
    "src/mame/philips/cdidvc.cpp",
    "\tm_fmv_dclk_offset = 0;\n\tm_fma_dclk_latch = 0;\n",
    "\tm_fmv_dclk_offset = 0;\n\tm_fmv_syscr_programmed = false;\n\tm_fma_dclk_latch = 0;\n",
    "reset GEN_SYSCR programmed state",
)

replace_once(
    "src/mame/philips/cdidvc.cpp",
    "\tm_fmv_dclk_offset += desired - current;\n}\n",
    "\tm_fmv_dclk_offset += desired - current;\n\tm_fmv_syscr_programmed = true;\n}\n",
    "latch GEN_SYSCR programmed state",
)

old_sync = '''\tif (!single_step\n\t\t\t&& (m_fmv_system_control & 0x0004U)\n\t\t\t&& m_video_queue.front().timestamp_valid\n\t\t\t&& m_mpeg_have_scr[MPEG_FMV])\n\t{\n\t\tclock90 = current_mpeg_clock90(MPEG_FMV);\n\t\tcdi_dvc::presentation_selection selection { 0, 0, false };\n\t\tfor (std::size_t index = 0; index < m_video_queue.size(); ++index)\n\t\t{\n\t\t\tqueued_video_frame const &queued = m_video_queue[index];\n\t\t\tif (!queued.timestamp_valid\n\t\t\t\t\t|| !cdi_dvc::mpeg_presentation_due(queued.timestamp90, clock90))\n\t\t\t\tbreak;\n\n\t\t\tselection.selected_index = index;\n\t\t\tselection.consume_count = index + 1;\n\t\t\tselection.valid = true;\n\t\t}\n\t\tif (!selection.valid)\n\t\t{\n\t\t\t++m_scheduler_wait_vblanks;\n\t\t\treturn;\n\t\t}\n\n\t\tselected_index = selection.selected_index;\n\t\tconsume_count = selection.consume_count;\n\t\ttimestamp_driven = true;\n\t}\n\telse if (!single_step)\n\t{\n\t\t// Compatibility fallback for streams that have not established both a\n\t\t// frame timestamp and an FMV SCR clock.  Preserve queued order and do not\n\t\t// reintroduce the old single-slot overwrite behavior.\n\t\t++m_scheduler_fallback_presented;\n\t}\n'''
new_sync = '''\tif (!single_step\n\t\t\t&& (m_fmv_system_control & 0x0004U)\n\t\t\t&& m_video_queue.front().timestamp_valid)\n\t{\n\t\tcdi_dvc::video_sync_clock const sync_clock = cdi_dvc::select_video_sync_clock(\n\t\t\tm_fmv_syscr_programmed, current_fmv_dclk(),\n\t\t\tm_mpeg_have_scr[MPEG_FMV], m_mpeg_last_scr[MPEG_FMV],\n\t\t\tm_mpeg_scr_dclk_anchor[MPEG_FMV]);\n\t\tif (sync_clock.valid)\n\t\t{\n\t\t\tclock90 = sync_clock.clock90;\n\t\t\tcdi_dvc::presentation_selection selection { 0, 0, false };\n\t\t\tfor (std::size_t index = 0; index < m_video_queue.size(); ++index)\n\t\t\t{\n\t\t\t\tqueued_video_frame const &queued = m_video_queue[index];\n\t\t\t\tif (!queued.timestamp_valid\n\t\t\t\t\t\t|| !cdi_dvc::mpeg_presentation_due(queued.timestamp90, clock90))\n\t\t\t\t\tbreak;\n\n\t\t\t\tselection.selected_index = index;\n\t\t\t\tselection.consume_count = index + 1;\n\t\t\t\tselection.valid = true;\n\t\t\t}\n\t\t\tif (!selection.valid)\n\t\t\t{\n\t\t\t\t++m_scheduler_wait_vblanks;\n\t\t\t\treturn;\n\t\t\t}\n\n\t\t\tselected_index = selection.selected_index;\n\t\t\tconsume_count = selection.consume_count;\n\t\t\ttimestamp_driven = true;\n\t\t}\n\t\telse\n\t\t{\n\t\t\t++m_scheduler_fallback_presented;\n\t\t}\n\t}\n\telse if (!single_step)\n\t{\n\t\t// Compatibility fallback for streams that have not established a usable\n\t\t// timestamp clock. Preserve queued order and do not reintroduce the old\n\t\t// single-slot overwrite behavior.\n\t\t++m_scheduler_fallback_presented;\n\t}\n'''
replace_once("src/mame/philips/cdidvc.cpp", old_sync, new_sync, "synchronous video clock")

old_overlay = '''\tunsigned const src_y = unsigned(m_video_crop_y)\n\t\t+ unsigned(rel_y / int(cdi_dvc::VIDEO_PIXEL_Y_SCALE));\n\tfor (unsigned x = 0; x < window_w; ++x)\n\t{\n\t\tunsigned const src_x = unsigned(m_video_crop_x) + x;\n\t\tuint32_t const color = m_video_present_frame[size_t(src_y) * m_video_present_width + src_x];\n\t\tfor (unsigned repeat = 0; repeat < cdi_dvc::VIDEO_PIXEL_X_SCALE; ++repeat)\n\t\t{\n\t\t\tint const out_x = dst_x + int(x * cdi_dvc::VIDEO_PIXEL_X_SCALE + repeat);\n\t\t\tif (out_x < 0 || out_x >= int(pixel_count) || out_x >= int(external_count))\n\t\t\t\tcontinue;\n\t\t\tif (out_x < clip_min_x || out_x > clip_max_x || !external_video[out_x])\n\t\t\t\tcontinue;\n\n\t\t\tpixels[out_x] = color;\n#if (VERBOSE & LOG_VIDEO)\n\t\t\tuint8_t const r = uint8_t(color >> 16);\n\t\t\tuint8_t const g = uint8_t(color >> 8);\n\t\t\tuint8_t const b = uint8_t(color);\n\t\t\tm_video_overlay_hash ^= r;\n\t\t\tm_video_overlay_hash *= 16777619U;\n\t\t\tm_video_overlay_hash ^= g;\n\t\t\tm_video_overlay_hash *= 16777619U;\n\t\t\tm_video_overlay_hash ^= b;\n\t\t\tm_video_overlay_hash *= 16777619U;\n\t\t\t++m_video_overlay_pixels;\n#endif\n\t\t\t++m_video_overlay_total_pixels;\n\t\t\tif (physical_y >= visible_top && physical_y < visible_top + 64)\n\t\t\t\t++m_video_overlay_top64_pixels;\n\t\t}\n\t}\n'''
new_overlay = '''\tunsigned const src_y = unsigned(m_video_crop_y)\n\t\t+ unsigned(rel_y / int(cdi_dvc::VIDEO_PIXEL_Y_SCALE));\n\tbool const vcd_mode = cdi_dvc::vcd_pixel_clock_enabled(m_vcd_control);\n\tunsigned const output_w = cdi_dvc::video_output_width(window_w, vcd_mode);\n\tfor (unsigned output_x = 0; output_x < output_w; ++output_x)\n\t{\n\t\tunsigned const src_x = unsigned(m_video_crop_x)\n\t\t\t+ cdi_dvc::video_source_x_for_output(output_x, window_w, vcd_mode);\n\t\tuint32_t const color = m_video_present_frame[size_t(src_y) * m_video_present_width + src_x];\n\t\tint const out_x = dst_x + int(output_x);\n\t\tif (out_x < 0 || out_x >= int(pixel_count) || out_x >= int(external_count))\n\t\t\tcontinue;\n\t\tif (out_x < clip_min_x || out_x > clip_max_x || !external_video[out_x])\n\t\t\tcontinue;\n\n\t\tpixels[out_x] = color;\n#if (VERBOSE & LOG_VIDEO)\n\t\tuint8_t const r = uint8_t(color >> 16);\n\t\tuint8_t const g = uint8_t(color >> 8);\n\t\tuint8_t const b = uint8_t(color);\n\t\tm_video_overlay_hash ^= r;\n\t\tm_video_overlay_hash *= 16777619U;\n\t\tm_video_overlay_hash ^= g;\n\t\tm_video_overlay_hash *= 16777619U;\n\t\tm_video_overlay_hash ^= b;\n\t\tm_video_overlay_hash *= 16777619U;\n\t\t++m_video_overlay_pixels;\n#endif\n\t\t++m_video_overlay_total_pixels;\n\t\tif (physical_y >= visible_top && physical_y < visible_top + 64)\n\t\t\t++m_video_overlay_top64_pixels;\n\t}\n'''
replace_once("src/mame/philips/cdidvc.cpp", old_overlay, new_overlay, "VCD horizontal presentation cadence")

print("Applied bounded CD-i DVC sync/quality patch")
