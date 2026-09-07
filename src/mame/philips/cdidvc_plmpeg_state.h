// license:BSD-3-Clause
// copyright-holders:Matt Dawidowicz
#ifndef MAME_PHILIPS_CDIDVC_PLMPEG_STATE_H
#define MAME_PHILIPS_CDIDVC_PLMPEG_STATE_H
#pragma once
#include <cstddef>
#include <cstdint>
struct plm_audio_t;
struct plm_video_t;
namespace cdi_dvc
{
// Versioned value-only snapshots of DVC-owned ring-buffer decoders. No host
// pointers, allocator capacity, callbacks or structure padding enter the image.
// Reads require a newly created decoder; failure leaves it safe to destroy.
std::size_t plmpeg_audio_snapshot_write(plm_audio_t const *, uint8_t *, std::size_t);
std::size_t plmpeg_video_snapshot_write(plm_video_t const *, uint8_t *, std::size_t);
bool plmpeg_audio_snapshot_read(plm_audio_t *, uint8_t const *, std::size_t);
bool plmpeg_video_snapshot_read(plm_video_t *, uint8_t const *, std::size_t);
} // namespace cdi_dvc
#endif
