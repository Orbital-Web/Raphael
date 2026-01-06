#include <Raphael/consts.h>
#include <Raphael/tunable.h>

#include <cmath>



void Raphael::update_lmr_table() {
    for (const bool is_quiet : {false, true}) {
        for (int depth = 0; depth <= MAX_DEPTH; depth++) {
            for (int movei = 0; movei < 256; movei++) {
                if (depth == 0 || movei == 0) {
                    LMR_TABLE[is_quiet][depth][movei] = 0;
                    continue;
                }

                constexpr int qf = 1024 * 1024;
                LMR_TABLE[is_quiet][depth][movei]
                    = (is_quiet)
                          ? LMR_QUIET_BASE + log(depth) * log(movei + 1) * qf / LMR_QUIET_DIVISOR
                          : LMR_NOISY_BASE + log(depth) * log(movei + 1) * qf / LMR_NOISY_DIVISOR;
            }
        }
    }
}

void Raphael::init_tunables() { update_lmr_table(); }
