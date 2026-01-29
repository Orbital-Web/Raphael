#include <Raphael/consts.h>
#include <Raphael/utils.h>

#include <cmath>



namespace raphael::utils {
bool is_win(i32 score) { return score >= MATE_SCORE - MAX_DEPTH; }

bool is_loss(i32 score) { return score <= -MATE_SCORE + MAX_DEPTH; }

bool is_mate(i32 score) { return is_win(score) || is_loss(score); }

i32 mate_distance(i32 score) { return ((score >= 0) ? 1 : -1) * (MATE_SCORE - abs(score) + 1) / 2; }



bool is_quiet(chess::Move move, const chess::Board& board) {
    return !(
        board.is_capture(move)
        || (move.type() == chess::Move::PROMOTION
            && move.promotion_type() == chess::PieceType::QUEEN)
    );
}
}  // namespace raphael::utils
