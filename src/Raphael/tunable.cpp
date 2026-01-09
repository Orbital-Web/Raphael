#include <Raphael/consts.h>
#include <Raphael/tunable.h>

#include <cmath>



void Raphael::update_lmr_table() {
    for (const bool is_quiet : {false, true}) {
        for (int depth = 0; depth <= MAX_DEPTH; depth++) {
            for (int move_searched = 0; move_searched < 256; move_searched++) {
                if (depth == 0 || move_searched == 0) {
                    LMR_TABLE[is_quiet][depth][move_searched] = 0;
                    continue;
                }

                const double scale = 1024 * 1024 * log(depth) * log(move_searched);
                LMR_TABLE[is_quiet][depth][move_searched]
                    = (is_quiet) ? LMR_QUIET_BASE + scale / LMR_QUIET_DIVISOR
                                 : LMR_NOISY_BASE + scale / LMR_NOISY_DIVISOR;
            }
        }
    }
}

void Raphael::init_tunables() { update_lmr_table(); }
