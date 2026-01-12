#include <Raphael/consts.h>
#include <Raphael/tunable.h>

#include <cmath>



void raphael::update_lmp_table() {
    for (const bool improving : {false, true})
        for (int depth = 0; depth <= MAX_DEPTH; depth++)
            LMP_TABLE[improving][depth] = (LMP_THRESH_BASE + depth * depth) / (2 - improving);
}

void raphael::update_lmr_table() {
    for (const bool is_quiet : {false, true}) {
        for (int depth = 0; depth <= MAX_DEPTH; depth++) {
            for (int move_searched = 0; move_searched < 256; move_searched++) {
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

void raphael::init_tunables() {
    update_lmp_table();
    update_lmr_table();
}
