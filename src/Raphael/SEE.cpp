#include <Raphael/SEE.h>



namespace raphael::SEE {

namespace internal {
i32 pieceval(chess::Square sq, const chess::Board& board) { return VAL[board.at(sq)]; }


chess::Square lva(chess::Bitboard attackers, const chess::Board& board) {
    for (i32 p = 0; p < 6; p++) {
        auto attacker_of_type
            = attackers & board.pieces(static_cast<chess::PieceType::underlying>(p));
        if (attacker_of_type) return attacker_of_type.pop();
    }
    return chess::Square::NO_SQ;
}
}  // namespace internal



bool see(const chess::Move& move, const chess::Board& board, i32 threshold) {
    const auto to = move.to();                        // where the exchange happens
    auto victim_sq = move.from();                     // capturer becomes next victim
    auto occ = board.occ().clear(victim_sq.index());  // remove capturer from occ
    auto color = ~board.sideToMove();
    i32 gain = -threshold;

    // add material gain
    if (move.typeOf() == chess::Move::ENPASSANT) {
        // pawn captured
        gain += internal::VAL[0];
        const auto enpsq = (board.sideToMove() == chess::Color::WHITE)
                               ? to + chess::Direction::SOUTH
                               : to + chess::Direction::NORTH;
        occ.clear(enpsq.index());
    } else if (move.typeOf() == chess::Move::PROMOTION) {
        // promotion + any capture - pawn
        const auto promo = move.promotionType();
        gain += internal::VAL[promo] + internal::pieceval(to, board) - internal::VAL[0];
    } else if (move.typeOf() != chess::Move::CASTLING)
        gain += internal::pieceval(to, board);

    if (gain < 0) return false;

    // initial capture
    if (move.typeOf() == chess::Move::PROMOTION) {
        const auto promo = move.promotionType();
        gain -= internal::VAL[promo];
    } else
        gain -= internal::pieceval(victim_sq, board);

    if (gain >= 0) return true;

    // generate list of all direct attackers
    const auto queens = board.pieces(chess::PieceType::QUEEN);
    const auto bqs = board.pieces(chess::PieceType::BISHOP) | queens;
    const auto rqs = board.pieces(chess::PieceType::ROOK) | queens;
    auto all_attackers
        = (chess::attacks::pawn(color, to) & board.pieces(chess::PieceType::PAWN, ~color));
    all_attackers
        |= (chess::attacks::pawn(~color, to) & board.pieces(chess::PieceType::PAWN, color));
    all_attackers |= (chess::attacks::knight(to) & board.pieces(chess::PieceType::KNIGHT));
    all_attackers |= (chess::attacks::bishop(to, occ) & bqs);
    all_attackers |= (chess::attacks::rook(to, occ) & rqs);
    all_attackers |= (chess::attacks::king(to) & board.pieces(chess::PieceType::KING));

    // simulate a series of captures on the same square
    while (true) {
        all_attackers &= occ;
        const auto attackers = all_attackers & board.us(color);
        if (!attackers) break;

        color = ~color;
        victim_sq = internal::lva(attackers, board);  // capturer becomes next victim
        gain = -gain - 1 - internal::pieceval(victim_sq, board);
        if (gain >= 0) {
            if (board.at<chess::PieceType>(victim_sq) == chess::PieceType::KING
                && attackers & board.us(color))
                color = ~color;
            break;
        }

        // update virtual board
        occ.clear(victim_sq.index());  // remove capturer from occ
        all_attackers |= chess::attacks::bishop(to, occ) & bqs;
        all_attackers |= chess::attacks::rook(to, occ) & rqs;
    }

    return color != board.sideToMove();
}
}  // namespace raphael::SEE
