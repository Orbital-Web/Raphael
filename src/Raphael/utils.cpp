#include <Raphael/consts.h>
#include <Raphael/utils.h>



bool raphael::is_win(int eval) { return eval >= MATE_EVAL - MAX_DEPTH; }

bool raphael::is_loss(int eval) { return eval <= -MATE_EVAL + MAX_DEPTH; }

bool raphael::is_mate(int eval) { return is_win(eval) || is_loss(eval); }
