#include <Raphael/SEE.h>
#include <Raphael/tunable.h>



namespace raphael::SEE {

namespace internal {
i32 pieceval(chess::Square sq, const chess::Board& board) { return SEE_TABLE[board.at(sq)]; }


chess::Square lva(chess::BitBoard attackers, const chess::Board& board) {
    for (i32 p = 0; p < 6; p++) {
        auto attacker_of_type = attackers & board.occ(chess::PieceType(p));
        if (attacker_of_type) return static_cast<chess::Square>(attacker_of_type.poplsb());
    }
    return chess::Square::NONE;
}
}  // namespace internal



bool see(chess::Move move, const chess::Board& board, i32 threshold) {
    const auto to = move.to();                // where the exchange happens
    auto victim_sq = move.from();             // capturer becomes next victim
    auto occ = board.occ().unset(victim_sq);  // remove capturer from occ
    auto color = ~board.stm();
    i32 gain = -threshold;

    // add material gain
    if (move.type() == chess::Move::ENPASSANT) {
        // pawn captured
        gain += SEE_TABLE[0];
        const auto enpsq = to.ep_square();
        occ.unset(enpsq);
    } else if (move.type() == chess::Move::PROMOTION) {
        // promotion + any capture - pawn
        const auto promo = move.promotion_type();
        gain += SEE_TABLE[promo] + internal::pieceval(to, board) - SEE_TABLE[0];
    } else if (move.type() != chess::Move::CASTLING)
        gain += internal::pieceval(to, board);

    if (gain < 0) return false;

    // initial capture
    if (move.type() == chess::Move::PROMOTION) {
        const auto promo = move.promotion_type();
        gain -= SEE_TABLE[promo];
    } else
        gain -= internal::pieceval(victim_sq, board);

    if (gain >= 0) return true;

    // generate list of all direct attackers
    const auto queens = board.occ(chess::PieceType::QUEEN);
    const auto bqs = board.occ(chess::PieceType::BISHOP) | queens;
    const auto rqs = board.occ(chess::PieceType::ROOK) | queens;
    auto all_attackers
        = (chess::Attacks::pawn(to, color) & board.occ(chess::PieceType::PAWN, ~color));
    all_attackers |= (chess::Attacks::pawn(to, ~color) & board.occ(chess::PieceType::PAWN, color));
    all_attackers |= (chess::Attacks::knight(to) & board.occ(chess::PieceType::KNIGHT));
    all_attackers |= (chess::Attacks::bishop(to, occ) & bqs);
    all_attackers |= (chess::Attacks::rook(to, occ) & rqs);
    all_attackers |= (chess::Attacks::king(to) & board.occ(chess::PieceType::KING));

    // simulate a series of captures on the same square
    while (true) {
        all_attackers &= occ;
        const auto attackers = all_attackers & board.occ(color);
        if (!attackers) break;

        color = ~color;
        victim_sq = internal::lva(attackers, board);  // capturer becomes next victim
        gain = -gain - 1 - internal::pieceval(victim_sq, board);
        if (gain >= 0) {
            if (board.at(victim_sq).type() == chess::PieceType::KING
                && attackers & board.occ(color))
                color = ~color;
            break;
        }

        // update virtual board
        occ.unset(victim_sq);  // remove capturer from occ
        all_attackers |= chess::Attacks::bishop(to, occ) & bqs;
        all_attackers |= chess::Attacks::rook(to, occ) & rqs;
    }

    return color != board.stm();
}
}  // namespace raphael::SEE
