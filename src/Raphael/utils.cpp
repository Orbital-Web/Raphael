#include <Raphael/consts.h>
#include <Raphael/utils.h>

#include <cmath>



namespace raphael::utils {
bool is_win(i32 eval) { return eval >= MATE_EVAL - MAX_DEPTH; }

bool is_loss(i32 eval) { return eval <= -MATE_EVAL + MAX_DEPTH; }

bool is_mate(i32 eval) { return is_win(eval) || is_loss(eval); }

i32 mate_distance(i32 eval) { return ((eval >= 0) ? 1 : -1) * (MATE_EVAL - abs(eval) + 1) / 2; }



bool is_quiet(chess::Move move, const chess::Board& board) {
    return !(
        board.is_capture(move)
        || (move.type() == chess::Move::PROMOTION
            && move.promotion_type() == chess::PieceType::QUEEN)
    );
}
}  // namespace raphael::utils
