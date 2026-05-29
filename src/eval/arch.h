#pragma once
#ifdef EVAL_NNUE
#include <chess/include.h>



namespace raphael::nnue {
static constexpr i32 OUTPUT_SCALE = 279;
static constexpr i32 N_INPUTS = 11 * 64;
static constexpr i32 L1_SIZE = 1280;
static constexpr i32 L2_SIZE = 16;
static constexpr i32 L3_SIZE = 32;
static constexpr i32 N_OUTBUCKETS = 8;
static constexpr i32 QA = 255;
static constexpr i32 QB = 128;
static constexpr i32 QC = 64;
static constexpr i32 N_INBUCKETS = 16;
static constexpr i32 BUCKETS[32] = {  // clang-format off
    0,  1,  2,  3,
    4,  5,  6,  7,
    8,  8,  9,  9,
    10, 10, 11, 11,
    12, 12, 13, 13,
    12, 12, 13, 13,
    14, 14, 15, 15,
    14, 14, 15, 15
};  // clang-format on
static constexpr i32 L1_SHIFT = 8;
}  // namespace raphael::nnue
#endif