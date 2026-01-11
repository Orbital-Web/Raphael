#include <Raphael/consts.h>
#include <Raphael/utils.h>



bool Raphael::is_win(int eval) { return eval >= MATE_EVAL - MAX_DEPTH; }

bool Raphael::is_loss(int eval) { return eval <= -MATE_EVAL + MAX_DEPTH; }

bool Raphael::is_mate(int eval) { return is_win(eval) || is_loss(eval); }
