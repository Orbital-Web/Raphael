#include <Raphael/consts.h>
#include <Raphael/utils.h>

#include <cmath>



namespace raphael {
bool is_win(i32 eval) { return eval >= MATE_EVAL - MAX_DEPTH; }

bool is_loss(i32 eval) { return eval <= -MATE_EVAL + MAX_DEPTH; }

bool is_mate(i32 eval) { return is_win(eval) || is_loss(eval); }

i32 mate_distance(i32 eval) { return ((eval >= 0) ? 1 : -1) * (MATE_EVAL - abs(eval) + 1) / 2; }
};  // namespace raphael
