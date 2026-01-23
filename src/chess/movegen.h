#pragma once
#include <chess/attacks.h>
#include <chess/board.h>
#include <chess/movegen_fwd.h>



// logic from https://github.com/Disservin/chess-library/blob/master/src/movegen.hpp
namespace chess {
template <Movegen::MoveGenType mt>
inline void Movegen::generate_legals(ScoredMoveList& movelist, const Board& board) {
    movelist.clear();

    if (board.stm() == Color::WHITE)
        generate_legals<Color::WHITE, mt>(movelist, board);
    else
        generate_legals<Color::BLACK, mt>(movelist, board);
}

template <Color::underlying color>
[[nodiscard]] inline bool Movegen::is_ep_valid(const Board& board, Square ep) {
    assert(board.stm() == color);

    const auto occ_us = board.occ(static_cast<Color>(color));
    const auto occ_opp = board.occ(~static_cast<Color>(color));
    const auto king_sq = board.king_square(static_cast<Color>(color));

    const auto [checkmask, _] = check_mask<color>(board, king_sq);
    const auto pin_hv = pin_mask<color, PieceType::ROOK>(board, king_sq, occ_opp, occ_us);
    const auto pin_d = pin_mask<color, PieceType::BISHOP>(board, king_sq, occ_opp, occ_us);

    const auto pawns = board.occ(PieceType::PAWN, static_cast<Color>(color));
    const auto pawns_lr = pawns & ~pin_hv;
    const auto moves = generate_legal_ep<color>(board, checkmask, pin_d, pawns_lr, ep);

    for (const auto& move : moves)
        if (move != Move::NO_MOVE) return true;
    return false;
}


template <Color::underlying color>
[[nodiscard]] inline std::pair<BitBoard, i32> Movegen::check_mask(const Board& board, Square sq) {
    const auto opp_knight = board.occ(PieceType::KNIGHT, ~static_cast<Color>(color));
    const auto opp_bishop = board.occ(PieceType::BISHOP, ~static_cast<Color>(color));
    const auto opp_rook = board.occ(PieceType::ROOK, ~static_cast<Color>(color));
    const auto opp_queen = board.occ(PieceType::QUEEN, ~static_cast<Color>(color));
    const auto opp_pawns = board.occ(PieceType::PAWN, ~static_cast<Color>(color));

    BitBoard mask = 0;
    i32 checks = 0;

    // check for knight checks
    const auto knight_attacks = Attacks::knight(sq) & opp_knight;
    mask |= knight_attacks;
    checks += bool(knight_attacks);

    // check for pawn checks
    const auto pawn_attacks = Attacks::pawn(sq, board.stm()) & opp_pawns;
    mask |= pawn_attacks;
    checks += bool(pawn_attacks);

    // check for bishop checks
    const auto bishop_attacks = Attacks::bishop(sq, board.occ()) & (opp_bishop | opp_queen);
    if (bishop_attacks) {
        mask |= Attacks::between(sq, static_cast<Square>(bishop_attacks.lsb()));
        checks++;
    }

    const auto rook_attacks = Attacks::rook(sq, board.occ()) & (opp_rook | opp_queen);
    if (rook_attacks) {
        if (rook_attacks.count() > 1) {
            checks = 2;
            return {mask, checks};
        }

        mask |= Attacks::between(sq, static_cast<Square>(rook_attacks.lsb()));
        checks++;
    }

    if (!mask) return {DEFAULT_CHECKMASK, checks};
    return {mask, checks};
}

template <Color::underlying color, PieceType::underlying pt>
[[nodiscard]] inline BitBoard Movegen::pin_mask(
    const Board& board, Square sq, BitBoard occ_opp, BitBoard occ_us
) {
    static_assert(pt == PieceType::BISHOP || pt == PieceType::ROOK);

    const auto opp_pt_queen = (board.occ(static_cast<PieceType>(pt)) | board.occ(PieceType::QUEEN))
                              & board.occ(~static_cast<Color>(color));
    auto pt_attacks = Attacks::slider<pt>(sq, occ_opp) & opp_pt_queen;

    BitBoard pin = 0;
    while (pt_attacks) {
        const auto possible_pin = Attacks::between(sq, static_cast<Square>(pt_attacks.poplsb()));
        if ((possible_pin & occ_us).count() == 1) pin |= possible_pin;
    }
    return pin;
}

template <Color::underlying color>
[[nodiscard]] inline BitBoard Movegen::seen_squares(const Board& board, BitBoard negocc_opp) {
    const auto king_sq = board.king_square(~static_cast<Color>(color));
    BitBoard map_king_atk = Attacks::king(king_sq) & negocc_opp;
    if (map_king_atk.is_empty() && !board.chess960()) return 0;

    const auto occ = board.occ() ^ BitBoard::from_square(king_sq);
    const auto queens = board.occ(PieceType::QUEEN, static_cast<Color>(color));
    const auto pawns = board.occ(PieceType::PAWN, static_cast<Color>(color));
    auto knights = board.occ(PieceType::KNIGHT, static_cast<Color>(color));
    auto bishops = board.occ(PieceType::BISHOP, static_cast<Color>(color)) | queens;
    auto rooks = board.occ(PieceType::ROOK, static_cast<Color>(color)) | queens;

    auto seen = Attacks::pawn_left<color>(pawns) | Attacks::pawn_right<color>(pawns);

    while (knights) seen |= Attacks::knight(static_cast<Square>(knights.poplsb()));
    while (bishops) seen |= Attacks::bishop(static_cast<Square>(bishops.poplsb()), occ);
    while (rooks) seen |= Attacks::rook(static_cast<Square>(rooks.poplsb()), occ);
    seen |= Attacks::king(board.king_square(static_cast<Color>(color)));

    return seen;
}


// FIXME: switch to ALL, NOISY, QUIET, make NOISY include queening and QUIET include underpromos,
// after verifying bench is equivalent
template <Color::underlying color, Movegen::MoveGenType mt>
inline void Movegen::generate_legal_pawns(
    ScoredMoveList& moves,
    const Board& board,
    BitBoard pin_d,
    BitBoard pin_hv,
    BitBoard checkmask,
    BitBoard occ_opp
) {
    // TODO:
}

template <Color::underlying color>
[[nodiscard]] inline std::array<Move, 2> Movegen::generate_legal_ep(
    const Board& board, BitBoard checkmask, BitBoard pin_d, BitBoard pawns_lr, Square ep
) {
    assert(board.stm() == color);
    assert(
        (ep.rank() == Rank::R3 && color == Color::BLACK)
        || (ep.rank() == Rank::R6 && color == Color::WHITE)
    );

    std::array<Move, 2> moves = {Move::NO_MOVE, Move::NO_MOVE};
    i32 i = 0;

    const auto eppawn_sq = ep.ep_square();  // the pawn that double pushed

    // cannot enpassant if doing so doesn't resolve check
    if ((checkmask & (BitBoard::from_square(eppawn_sq) | BitBoard::from_square(ep))).is_empty())
        return moves;

    const Square king_sq = board.king_square(static_cast<Color>(color));
    const BitBoard king_mask
        = BitBoard::from_square(king_sq) & BitBoard::from_rank(eppawn_sq.rank());
    const BitBoard opp_qr = (board.occ(PieceType::ROOK) | board.occ(PieceType::QUEEN))
                            & board.occ(~static_cast<Color>(color));

    auto ep_attackers = Attacks::pawn(ep, ~static_cast<Color>(color)) & pawns_lr;
    while (ep_attackers) {
        const auto from = static_cast<Square>(ep_attackers.poplsb());
        const auto to = ep;

        // ep square must be in pinmask if the pawn is pinned
        if ((BitBoard::from_square(from) & pin_d) && !(pin_d & BitBoard::from_square(ep))) continue;

        const auto connecting_pawns
            = BitBoard::from_square(eppawn_sq) | BitBoard::from_square(from);
        const auto is_possible_pin = king_mask && opp_qr;

        // see if removing the two pawns puts us in check, i.e., K <- P p <- r/q
        if (is_possible_pin
            && !(Attacks::rook(king_sq, board.occ() ^ connecting_pawns) & opp_qr).is_empty())
            break;

        moves[i++] = Move::make<Move::ENPASSANT>(from, to);
    }

    return moves;
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_knights(Square sq) {
    // TODO:
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_bishops(
    Square sq, BitBoard pin_d, BitBoard occ_all
) {
    // TODO:
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_rooks(
    Square sq, BitBoard pin_hv, BitBoard occ_all
) {
    // TODO:
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_queens(
    Square sq, BitBoard pin_d, BitBoard pin_hv, BitBoard occ_all
) {
    // TODO:
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_kings(
    Square sq, BitBoard seen, BitBoard movable_square
) {
    // TODO:
}

template <Color::underlying color>
[[nodiscard]] inline BitBoard Movegen::generate_legal_castles(
    const Board& board, Square sq, BitBoard seen, BitBoard pin_hv
) {
    // TODO:
}


inline void Movegen::push_moves(
    ScoredMoveList& movelist, BitBoard occ, std::function<BitBoard(Square)> generator
) {
    while (occ) {
        const Square from = static_cast<Square>(occ.poplsb());
        BitBoard moves = generator(from);
        while (moves) {
            const Square to = static_cast<Square>(moves.poplsb());
            movelist.push({.move = Move::make<Move::NORMAL>(from, to)});
        }
    }
}

template <Color::underlying color, Movegen::MoveGenType mt>
inline void Movegen::generate_legals(ScoredMoveList& movelist, const Board& board) {
    // TODO:
}
}  // namespace chess
