// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstddef>

// Keep the single-header decoder implementation in a tiny, stable
// translation unit so ordinary DVC scheduling/register edits compile faster.
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"

namespace cdi_dvc
{
bool plmpeg_video_has_reference_frame(plm_video_t const *decoder)
{
	return decoder && decoder->has_reference_frame;
}
}

#include "cdidvc_plmpeg_state_impl.h"
