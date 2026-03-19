#include <Raphael/SEE.h>
#include <Raphael/tunable.h>



namespace raphael::SEE {

namespace internal {
i32 pieceval(chess::Square sq, const chess::Board& board) { return SEE_TABLE[board.at(sq)]; }


chess::Square lva(chess::BitBoard attackers, const chess::Board& board) {
    for (chess::PieceType pt = chess::PieceType::PAWN; pt <= chess::PieceType::KING; ++pt) {
        const auto attacker_of_type = attackers & board.occ(pt);
        if (attacker_of_type) return static_cast<chess::Square>(attacker_of_type.lsb());
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
        gain += SEE_TABLE[chess::PieceType::PAWN];
        const auto enpsq = to.ep_square();
        occ.unset(enpsq);
    } else if (move.type() == chess::Move::PROMOTION) {
        // promotion + any capture - pawn
        const auto promo = move.promotion_type();
        gain
            += SEE_TABLE[promo] + internal::pieceval(to, board) - SEE_TABLE[chess::PieceType::PAWN];
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

    const auto white_pinned = board.pinned(chess::Color::WHITE);
    const auto black_pinned = board.pinned(chess::Color::BLACK);
    const auto white_kingray = chess::Attacks::between(board.king_square(chess::Color::WHITE), to);
    const auto black_kingray = chess::Attacks::between(board.king_square(chess::Color::BLACK), to);
    const auto allowed = ~(white_pinned | black_pinned) | (white_pinned & white_kingray)
                         | (black_pinned & black_kingray);

    auto all_attackers
        = (chess::Attacks::pawn(to, color) & board.occ(chess::PieceType::PAWN, ~color));
    all_attackers |= (chess::Attacks::pawn(to, ~color) & board.occ(chess::PieceType::PAWN, color));
    all_attackers |= (chess::Attacks::knight(to) & board.occ(chess::PieceType::KNIGHT));
    all_attackers |= (chess::Attacks::bishop(to, occ) & bqs);
    all_attackers |= (chess::Attacks::rook(to, occ) & rqs);
    all_attackers |= (chess::Attacks::king(to) & board.occ(chess::PieceType::KING));
    all_attackers &= allowed;

    // simulate a series of captures on the same square
    while (true) {
        const auto attackers = all_attackers & board.occ(color);
        if (!attackers) break;

        color = ~color;
        victim_sq = internal::lva(attackers, board);  // capturer becomes next victim
        const auto victim = board.at(victim_sq).type();

        // remove victim and expose xrays
        occ.unset(victim_sq);
        if (victim == chess::PieceType::PAWN || victim == chess::PieceType::BISHOP
            || victim == chess::PieceType::QUEEN)
            all_attackers |= chess::Attacks::bishop(to, occ) & bqs;
        if (victim == chess::PieceType::ROOK || victim == chess::PieceType::QUEEN)
            all_attackers |= chess::Attacks::rook(to, occ) & rqs;
        all_attackers &= occ;

        gain = -gain - 1 - internal::pieceval(victim_sq, board);
        if (gain >= 0) {
            if (victim == chess::PieceType::KING && (all_attackers & board.occ(color)))
                color = ~color;
            break;
        }
    }

    return color != board.stm();
}
}  // namespace raphael::SEE
