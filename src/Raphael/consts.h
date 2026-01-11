#pragma once



namespace raphael {
// should remain largely unchanged
static constexpr int MAX_DEPTH = 128;
static_assert(MAX_DEPTH < 256);

static constexpr int MATE_EVAL = 2000000000;  // evaluation for immediate mate
}  // namespace raphael
