#pragma once
#include <chess/types.h>

#include <new>



namespace raphael {
// should remain largely unchanged
static constexpr i32 MAX_DEPTH = 250;
static_assert(MAX_DEPTH < 256);

static constexpr i32 INF_SCORE = 32767;
static constexpr i32 MATE_SCORE = 32766;  // score for immediate mate
static constexpr i32 NONE_SCORE = -INF_SCORE;

#ifdef __cpp_lib_hardware_interference_size
constexpr usize CACHE_SIZE = std::hardware_destructive_interference_size;
#else
constexpr usize CACHE_SIZE = 64;
#endif
}  // namespace raphael
