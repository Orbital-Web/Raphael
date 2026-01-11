#include <Raphael/consts.h>
#include <Raphael/utils.h>

#include <cmath>



bool raphael::is_win(int eval) { return eval >= MATE_EVAL - MAX_DEPTH; }

bool raphael::is_loss(int eval) { return eval <= -MATE_EVAL + MAX_DEPTH; }

bool raphael::is_mate(int eval) { return is_win(eval) || is_loss(eval); }

int raphael::mate_distance(int eval) {
    return ((eval >= 0) ? 1 : -1) * (MATE_EVAL - abs(eval) + 1) / 2;
}
