#pragma once
#include <chess/bitboard.h>
#include <chess/move.h>

#include <functional>



namespace chess {
class Board;


class Movegen {
public:
    enum class MoveGenType : u8 { ALL, CAPTURE, QUIET };  // FIXME: switch to ALL, NOISY, QUIET

private:
    static constexpr BitBoard DEFAULT_CHECKMASK = BitBoard(0xFFFFFFFFFFFFFFFFull);


public:
    /** Clears and populates movelist with a list of legal moves
     * Note that the ScoredMoves in movelist will only have their move field set
     *
     * \tparam mt either ALL, CAPTURE only, or QUIET only FIXME: switch to ALL, NOISY, QUIET
     * \param movelist movelist to populate
     * \param board current board
     */
    template <MoveGenType mt = MoveGenType::ALL>
    static void generate_legals(ScoredMoveList& movelist, const Board& board);

    /** A slow function to check if an enpassant square is valid, taking into account of pins and
     * other stuff
     *
     * \tparam color the side to move
     * \param board current board
     * \param ep the enpassant square (behind the double push)
     * \returns whether the enpassant square is valid or not
     */
    template <Color::underlying color>
    [[nodiscard]] static bool is_ep_valid(const Board& board, Square ep);

private:
    template <Color::underlying color>
    [[nodiscard]] static std::pair<BitBoard, i32> check_mask(const Board& board, Square sq);

    template <Color::underlying color, PieceType::underlying pt>
    [[nodiscard]] static BitBoard pin_mask(
        const Board& board, Square sq, BitBoard occ_opp, BitBoard occ_us
    );

    template <Color::underlying color>
    [[nodiscard]] static BitBoard seen_squares(const Board& board, BitBoard negocc_opp);


    template <Color::underlying color, MoveGenType mt>
    static void generate_legal_pawns(
        ScoredMoveList& moves,
        const Board& board,
        BitBoard pin_d,
        BitBoard pin_hv,
        BitBoard checkmask,
        BitBoard occ_opp
    );

    template <Color::underlying color>
    [[nodiscard]] static std::array<Move, 2> generate_legal_ep(
        const Board& board, BitBoard checkmask, BitBoard pin_d, BitBoard pawns_lr, Square ep
    );

    [[nodiscard]] static BitBoard generate_legal_knights(Square sq);

    [[nodiscard]] static BitBoard generate_legal_bishops(
        Square sq, BitBoard pin_d, BitBoard occ_all
    );

    [[nodiscard]] static BitBoard generate_legal_rooks(
        Square sq, BitBoard pin_hv, BitBoard occ_all
    );

    [[nodiscard]] static BitBoard generate_legal_queens(
        Square sq, BitBoard pin_d, BitBoard pin_hv, BitBoard occ_all
    );

    [[nodiscard]] static BitBoard generate_legal_kings(
        Square sq, BitBoard seen, BitBoard movable_square
    );

    template <Color::underlying color>
    [[nodiscard]] static BitBoard generate_legal_castles(
        const Board& board, Square sq, BitBoard seen, BitBoard pin_hv
    );


    void push_moves(
        ScoredMoveList& movelist, BitBoard occ, std::function<BitBoard(Square)> generator
    );

    template <Color::underlying color, MoveGenType mt>
    void generate_legals(ScoredMoveList& movelist, const Board& board);
};
}  // namespace chess
