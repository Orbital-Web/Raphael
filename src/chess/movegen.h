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
    assert(board.stm() == color);

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

    if (!mask) return {BitBoard::FULL, checks};
    return {mask, checks};
}

template <Color::underlying color, PieceType::underlying pt>
[[nodiscard]] inline BitBoard Movegen::pin_mask(
    const Board& board, Square sq, BitBoard occ_opp, BitBoard occ_us
) {
    static_assert(pt == PieceType::BISHOP || pt == PieceType::ROOK);
    assert(board.stm() == color);

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
[[nodiscard]] inline BitBoard Movegen::seen_squares(const Board& board, BitBoard opp_empty) {
    assert(board.stm() == color);

    const auto king_sq = board.king_square(~static_cast<Color>(color));
    BitBoard map_king_atk = Attacks::king(king_sq) & opp_empty;
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
    assert(board.stm() == color);

    constexpr auto UP = relative_direction(Direction::NORTH, static_cast<Color>(color));
    constexpr auto DOWN = relative_direction(Direction::SOUTH, static_cast<Color>(color));
    constexpr auto DOWN_LEFT = relative_direction(Direction::SOUTH_WEST, static_cast<Color>(color));
    constexpr auto DOWN_RIGHT
        = relative_direction(Direction::SOUTH_EAST, static_cast<Color>(color));
    constexpr auto UP_LEFT = relative_direction(Direction::NORTH_WEST, static_cast<Color>(color));
    constexpr auto UP_RIGHT = relative_direction(Direction::NORTH_EAST, static_cast<Color>(color));

    constexpr auto RANK_B_PROMO
        = BitBoard::from_rank(Rank(Rank::R7).relative(static_cast<Color>(color)));
    constexpr auto RANK_PROMO
        = BitBoard::from_rank(Rank(Rank::R8).relative(static_cast<Color>(color)));
    constexpr auto DOUBLE_PUSH_RANK
        = BitBoard::from_rank(Rank(Rank::R3).relative(static_cast<Color>(color)));

    const auto pawns = board.occ(PieceType::PAWN, static_cast<Color>(color));
    const auto pawns_lr = pawns & ~pin_hv;
    const auto pinned_pawns_lr = pawns_lr & pin_d;
    const auto unpinned_pawns_lr = pawns_lr & ~pin_d;

    // these pawns can maybe go left or right
    auto l_pawns
        = unpinned_pawns_lr.shifted<UP_LEFT>() | (pinned_pawns_lr.shifted<UP_LEFT>() & pin_d);
    auto r_pawns
        = unpinned_pawns_lr.shifted<UP_RIGHT>() | (pinned_pawns_lr.shifted<UP_RIGHT>() & pin_d);

    // prune moves that don't capture a piece and are not on the checkmask
    l_pawns &= occ_opp & checkmask;
    r_pawns &= occ_opp & checkmask;

    // these pawns can walk forward
    const auto pawns_hv = pawns & ~pin_d;

    const auto pawns_pinned_hv = pawns_hv & pin_hv;
    const auto pawns_unpinned_hv = pawns_hv & ~pin_hv;

    // prune moves that are blocked by a piece
    const BitBoard single_push_unpinned = pawns_unpinned_hv.shifted<UP>() & ~board.occ();
    const BitBoard single_push_pinned = pawns_pinned_hv.shifted<UP>() & pin_hv & ~board.occ();

    // prune moves that are not on the checkmask
    BitBoard single_push = (single_push_unpinned | single_push_pinned) & checkmask;
    BitBoard double_push = ((single_push_unpinned & DOUBLE_PUSH_RANK).shifted<UP>() & ~board.occ()
                            | (single_push_pinned & DOUBLE_PUSH_RANK).shifted<UP>() & ~board.occ())
                           & checkmask;

    // generate promos
    if (pawns & RANK_B_PROMO) {
        auto promo_left = l_pawns & RANK_PROMO;
        auto promo_right = r_pawns & RANK_PROMO;
        auto promo_push = single_push & RANK_PROMO;

        // generate capturing promos for ALL or CAPTURE
        while (mt != MoveGenType::QUIET && promo_left) {
            const auto sq = static_cast<Square>(promo_left.poplsb());
            moves.push(
                {.move = Move::make<Move::PROMOTION>(sq + DOWN_RIGHT, sq, PieceType::QUEEN)}
            );
            moves.push({.move = Move::make<Move::PROMOTION>(sq + DOWN_RIGHT, sq, PieceType::ROOK)});
            moves.push(
                {.move = Move::make<Move::PROMOTION>(sq + DOWN_RIGHT, sq, PieceType::BISHOP)}
            );
            moves.push(
                {.move = Move::make<Move::PROMOTION>(sq + DOWN_RIGHT, sq, PieceType::KNIGHT)}
            );
        }
        while (mt != MoveGenType::QUIET && promo_right) {
            const auto sq = static_cast<Square>(promo_right.poplsb());
            moves.push({.move = Move::make<Move::PROMOTION>(sq + DOWN_LEFT, sq, PieceType::QUEEN)});
            moves.push({.move = Move::make<Move::PROMOTION>(sq + DOWN_LEFT, sq, PieceType::ROOK)});
            moves.push(
                {.move = Move::make<Move::PROMOTION>(sq + DOWN_LEFT, sq, PieceType::BISHOP)}
            );
            moves.push(
                {.move = Move::make<Move::PROMOTION>(sq + DOWN_LEFT, sq, PieceType::KNIGHT)}
            );
        }

        // FIXME: generate quiet queening for ALL or NOISY
        // FIXME: replace below with generate quiet underpromo for ALL or QUIET

        // generate quiet promos for ALL or QUIET
        while (mt != MoveGenType::CAPTURE && promo_push) {
            const auto sq = static_cast<Square>(promo_push.poplsb());
            moves.push({.move = Move::make<Move::PROMOTION>(sq + DOWN, sq, PieceType::QUEEN)});
            moves.push({.move = Move::make<Move::PROMOTION>(sq + DOWN, sq, PieceType::ROOK)});
            moves.push({.move = Move::make<Move::PROMOTION>(sq + DOWN, sq, PieceType::BISHOP)});
            moves.push({.move = Move::make<Move::PROMOTION>(sq + DOWN, sq, PieceType::KNIGHT)});
        }
    }
    single_push &= ~RANK_PROMO;
    l_pawns &= ~RANK_PROMO;
    r_pawns &= ~RANK_PROMO;

    // generate captures for ALL or CAPTURE
    while (mt != MoveGenType::QUIET && l_pawns) {
        const auto sq = static_cast<Square>(l_pawns.poplsb());
        moves.push({.move = Move::make<Move::NORMAL>(sq + DOWN_RIGHT, sq)});
    }
    while (mt != MoveGenType::QUIET && r_pawns) {
        const auto sq = static_cast<Square>(r_pawns.poplsb());
        moves.push({.move = Move::make<Move::NORMAL>(sq + DOWN_LEFT, sq)});
    }

    // generate pushes for ALL or QUIET
    while (mt != MoveGenType::CAPTURE && single_push) {
        const auto sq = static_cast<Square>(single_push.poplsb());
        moves.push({.move = Move::make<Move::NORMAL>(sq + DOWN, sq)});
    }
    while (mt != MoveGenType::CAPTURE && double_push) {
        const auto sq = static_cast<Square>(double_push.poplsb());
        moves.push({.move = Move::make<Move::NORMAL>(sq + DOWN + DOWN, sq)});
    }

    // generate enpassant for ALL or CAPTURE
    if constexpr (mt == MoveGenType::QUIET) return;

    const Square ep = board.enpassant_square();
    if (ep != Square::NONE) {
        auto moves = generate_legal_ep<color>(board, checkmask, pin_d, pawns_lr, ep);
        for (const auto& move : moves)
            if (move != Move::NO_MOVE) moves.push({.move = move});
    }
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
    if (!checkmask.is_set(eppawn_sq) && !checkmask.is_set(ep)) return moves;

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
        if (pin_d.is_set(from) && !pin_d.is_set(ep)) continue;

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
    return Attacks::knight(sq);
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_bishops(
    Square sq, BitBoard pin_d, BitBoard occ_all
) {
    if (pin_d.is_set(sq)) return Attacks::bishop(sq, occ_all) & pin_d;
    return Attacks::bishop(sq, occ_all);
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_rooks(
    Square sq, BitBoard pin_hv, BitBoard occ_all
) {
    if (pin_hv.is_set(sq)) return Attacks::rook(sq, occ_all) & pin_hv;
    return Attacks::rook(sq, occ_all);
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_queens(
    Square sq, BitBoard pin_d, BitBoard pin_hv, BitBoard occ_all
) {
    if (pin_d.is_set(sq)) return Attacks::bishop(sq, occ_all) & pin_d;
    if (pin_hv.is_set(sq)) return Attacks::rook(sq, occ_all) & pin_hv;
    return Attacks::queen(sq, occ_all);
}

[[nodiscard]] inline BitBoard Movegen::generate_legal_kings(
    Square sq, BitBoard seen, BitBoard movable_square
) {
    return Attacks::king(sq) & movable_square & ~seen;
}

template <Color::underlying color>
[[nodiscard]] inline BitBoard Movegen::generate_legal_castles(
    const Board& board, Square sq, BitBoard seen, BitBoard pin_hv
) {
    assert(board.stm() == color);

    const auto rights = board.castle_rights();
    if (!sq.is_back_rank(static_cast<Color>(color)) || !rights.has(static_cast<Color>(color)))
        return 0;

    BitBoard moves = 0;

    for (const auto side :
         {Board::CastlingRights::Side::KING_SIDE, Board::CastlingRights::Side::QUEEN_SIDE}) {
        if (!rights.has(static_cast<Color>(color), side)) continue;

        // skip if there are pieces on the castling path
        const auto is_king_side = (side == Board::CastlingRights::Side::KING_SIDE);
        if (board.occ() & board.castle_path(static_cast<Color>(color), is_king_side)) continue;

        // skip if king path is being attacked
        const auto king_to = Square::castling_king_dest(is_king_side, static_cast<Color>(color));
        if (Attacks::between(sq, king_to) & seen) continue;

        // skip if chess960 and rook is pinned on backrank
        const auto rook_from
            = BitBoard::from_square(Square(rights.get_rook_file(color, side), sq.rank()));
        if (board.chess960() && (pin_hv & board.occ(static_cast<Color>(color)) & rook_from))
            continue;

        moves |= rook_from;
    }

    return moves;
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
    assert(board.stm() == color);

    const auto king_sq = board.king_square(static_cast<Color>(color));

    auto occ_us = board.occ(static_cast<Color>(color));
    auto occ_opp = board.occ(~static_cast<Color>(color));
    auto occ_all = occ_us | occ_opp;

    auto opp_empty = ~occ_us;

    const auto [checkmask, checks] = check_mask<color>(board, king_sq);
    const auto pin_hv = pin_mask<color, PieceType::ROOK>(board, king_sq, occ_opp, occ_us);
    const auto pin_d = pin_mask<color, PieceType::BISHOP>(board, king_sq, occ_opp, occ_us);
    assert(checks <= 2);

    BitBoard movable_square;
    if constexpr (mt == MoveGenType::ALL)
        movable_square = opp_empty;
    else if constexpr (mt == MoveGenType::CAPTURE)
        movable_square = occ_opp;
    else
        movable_square = ~occ_all;

    // generate king moves
    auto seen = seen_squares<~color>(board, opp_empty);
    push_moves(movelist, BitBoard::from_square(king_sq), [&](Square sq) {
        return generate_legal_kings(sq, seen, movable_square);
    });

    if (mt != MoveGenType::CAPTURE && checks == 0) {
        BitBoard moves = generate_legal_castles<color>(board, king_sq, seen, pin_hv);
        while (moves) {
            const auto to = static_cast<Square>(moves.poplsb());
            movelist.push({.move = Move::make<Move::CASTLING>(king_sq, to)});
        }
    }

    // only king moves allowed in double check
    if (checks == 2) return;

    // moves have to be on the checkmask
    movable_square &= checkmask;

    // generate pawn moves
    generate_legal_pawns<color, mt>(movelist, board, pin_d, pin_hv, checkmask, occ_opp);

    // generate knight moves
    const auto knights_mask
        = board.occ(PieceType::KNIGHT, static_cast<Color>(color)) & ~(pin_d | pin_hv);
    push_moves(movelist, knights_mask, [&](Square sq) {
        return generate_legal_knights(sq) & movable_square;
    });

    // generate bishop moves
    const auto bishops_mask = board.occ(PieceType::BISHOP, static_cast<Color>(color)) & ~pin_hv;
    push_moves(movelist, bishops_mask, [&](Square sq) {
        return generate_legal_bishops(sq, pin_d, occ_all) & movable_square;
    });

    // generate rook moves
    const auto rooks_mask = board.occ(PieceType::ROOK, static_cast<Color>(color)) & ~pin_d;
    push_moves(movelist, rooks_mask, [&](Square sq) {
        return generate_legal_rooks(sq, pin_hv, occ_all) & movable_square;
    });

    // generate queen moves
    const auto queens_mask
        = board.occ(PieceType::QUEEN, static_cast<Color>(color)) & ~(pin_d & pin_hv);
    push_moves(movelist, queens_mask, [&](Square sq) {
        return generate_legal_queens(sq, pin_d, pin_hv, occ_all) & movable_square;
    });
}
}  // namespace chess
