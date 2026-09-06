// license:BSD-3-Clause
// copyright-holders:Matt Jordan

#include <cstddef>

// The decoder implementation and the tests that inspect its private state must
// share one translation unit.  cdidvc.cpp is intentionally included here so
// those backend-specific assertions retain access to PL_MPEG internals without
// creating a second implementation object or exposing private state publicly.
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "../../../3rdparty/pl_mpeg/pl_mpeg.h"
#undef PL_MPEG_IMPLEMENTATION

#include "cdidvc.cpp"
