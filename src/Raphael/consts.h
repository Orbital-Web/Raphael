#pragma once
#include <Raphael/types.h>



namespace raphael {
// should remain largely unchanged
static constexpr i32 MAX_DEPTH = 128;
static_assert(MAX_DEPTH < 256);

static constexpr i32 MATE_EVAL = 2000000000;  // evaluation for immediate mate
}  // namespace raphael
