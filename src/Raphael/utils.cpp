#include <Raphael/consts.h>
#include <Raphael/utils.h>

#include <cmath>



namespace raphael::utils {
bool is_win(i32 eval) { return eval >= MATE_EVAL - MAX_DEPTH; }

bool is_loss(i32 eval) { return eval <= -MATE_EVAL + MAX_DEPTH; }

bool is_mate(i32 eval) { return is_win(eval) || is_loss(eval); }

i32 mate_distance(i32 eval) { return ((eval >= 0) ? 1 : -1) * (MATE_EVAL - abs(eval) + 1) / 2; }



bool stm(const chess::Board& board) { return board.sideToMove() == chess::Color::WHITE; }

bool is_quiet(const chess::Move& move, const chess::Board& board) {
    return !(
        board.isCapture(move)
        || (move.typeOf() == chess::Move::PROMOTION
            && move.promotionType() == chess::PieceType::QUEEN)
    );
}

bool is_pk(const chess::Board& board) {
    // remove pawns and see if only king remains
    const auto color = board.sideToMove();
    return (board.us(color) ^ board.pieces(chess::PieceType::PAWN, color)).count() == 1;
}

chess::Piece piece_captured(const chess::Move& move, const chess::Board& board) {
    return (move.typeOf() == chess::Move::ENPASSANT)
               ? (stm(board) ? chess::Piece::BLACKPAWN : chess::Piece::WHITEPAWN)
               : board.at(move.to());
}
}  // namespace raphael::utils
