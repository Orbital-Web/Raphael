#pragma once
#include <chess/types.h>



namespace raphael {
// should remain largely unchanged
static constexpr i32 MAX_DEPTH = 128;
static_assert(MAX_DEPTH < 256);

static constexpr i32 INF_SCORE = 32767;
static constexpr i32 MATE_SCORE = 32766;  // score for immediate mate
static constexpr i32 NONE_SCORE = -INF_SCORE;
}  // namespace raphael
