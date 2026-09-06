// license:BSD-3-Clause
// copyright-holders:Matt Jordan

// Keep the single-header decoder implementation in a tiny, stable
// translation unit so ordinary DVC test edits compile faster.
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"
