#include <Raphael/consts.h>
#include <Raphael/tunable.h>

#include <cmath>



namespace raphael {
void update_lmp_table() {
    for (const bool improving : {false, true})
        for (i32 depth = 0; depth <= MAX_DEPTH; depth++)
            LMP_TABLE[improving][depth] = (LMP_THRESH_BASE + depth * depth) / (2 - improving);
}

void update_lmr_table() {
    for (const bool is_quiet : {false, true}) {
        for (i32 depth = 0; depth <= MAX_DEPTH; depth++) {
            for (i32 move_searched = 0; move_searched < 256; move_searched++) {
                if (depth == 0 || move_searched == 0) {
                    LMR_TABLE[is_quiet][depth][move_searched] = 0;
                    continue;
                }

                const double scale = 128 * 128 * log(depth) * log(move_searched);
                LMR_TABLE[is_quiet][depth][move_searched]
                    = (is_quiet) ? LMR_QUIET_BASE + scale / LMR_QUIET_DIVISOR
                                 : LMR_NOISY_BASE + scale / LMR_NOISY_DIVISOR;
            }
        }
    }
}

void update_see_table() {
    SEE_TABLE[0] = SEE_PAWN_VAL;
    SEE_TABLE[1] = SEE_KNIGHT_VAL;
    SEE_TABLE[2] = SEE_BISHOP_VAL;
    SEE_TABLE[3] = SEE_ROOK_VAL;
    SEE_TABLE[4] = SEE_QUEEN_VAL;
    SEE_TABLE[5] = 10000;
    SEE_TABLE[6] = SEE_PAWN_VAL;
    SEE_TABLE[7] = SEE_KNIGHT_VAL;
    SEE_TABLE[8] = SEE_BISHOP_VAL;
    SEE_TABLE[9] = SEE_ROOK_VAL;
    SEE_TABLE[10] = SEE_QUEEN_VAL;
    SEE_TABLE[11] = 10000;
    SEE_TABLE[12] = 0;
}

void init_tunables() {
    update_lmp_table();
    update_lmr_table();
    update_see_table();
}
}  // namespace raphael
