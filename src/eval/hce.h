#pragma once
#ifdef EVAL_HCE
#include <chess/include.h>



namespace raphael::hce {
// legacy, kept for future references
class RaphaelHCE {
private:
    // evaluation[midgame, endgame]
    static constexpr i32 KING_DIST_WEIGHT[2] = {0, 20};  // closer king bonus
    static constexpr i32 DRAW_DIVIDE_SCALE = 32;         // eval divide scale by for likely draw
    static constexpr i32 PVAL[12][2] = {
        // WHITE
        {100,   100 }, // PAWN
        {418,   246 }, // KNIGHT
        {449,   274 }, // BISHOP
        {554,   437 }, // ROOK
        {1191,  727 }, // QUEEN
        {0,     0   }, // KING
        // BLACK
        {-100,  -100}, // PAWN
        {-418,  -246}, // KNIGHT
        {-449,  -274}, // BISHOP
        {-554,  -437}, // ROOK
        {-1191, -727}, // QUEEN
        {0,     0   }, // KING
    };  // value of each piece
    static const MultiArray<i32, 12, 64, 2> PST;  // piece square table for piece, square, and phase
    static constexpr i32 PAWN_PASSED_WEIGHT[7][2] = {
        {0,   0  }, // promotion line
        {114, 215},
        {10,  160},
        {4,   77 },
        {-12, 47 },
        {1,   20 },
        {15,  13 },
    };  // bonus for passed pawn based on its rank
    static constexpr i32 PAWN_ISOLATION_WEIGHT[2] = {29, 21};  // isolated pawn cost
    static constexpr i32 MOBILITY_BISHOP[2] = {12, 6};         // bishop see/xray square count bonus
    static constexpr i32 MOBILITY_ROOK[2] = {11, 3};           // rook see/xray square count bonus
    static constexpr i32 BISH_PAIR_WEIGHT[2] = {39, 72};       // bishop pair bonus
    static constexpr i32 BISH_CORNER_WEIGHT[2] = {1, 20};      // enemy king to bishop corner bonus

    struct PMasks {
        std::array<chess::BitBoard, 64> WPASSED;   // white passed pawn mask bitboard (A1...H8)
        std::array<chess::BitBoard, 64> BPASSED;   // black passed pawn mask bitboard
        std::array<chess::BitBoard, 64> ISOLATED;  // isolated pawn mask bitboard (A1...H8)
    };
    static const PMasks PMASK;


public:
    /** Evaluates the board from the current side to move's perspective
     *
     * \param board current board (should match either set_board or new_board in make_move)
     * \returns the HCE evaluation of the board in centipawns
     */
    i32 evaluate(const chess::Board& board);

    /** Sets internal states to match the given board
     *
     * \param board the board to set
     */
    void set_board([[maybe_unused]] const chess::Board& board) {}

    /** Updates internal states based on the given move
     *
     * \param board current board (before move is played)
     * \param move the move to make
     */
    void make_move([[maybe_unused]] const chess::Board& board, [[maybe_unused]] chess::Move move) {}

    /** Updates internal states to unmake the last move */
    void unmake_move() {}
};
}  // namespace raphael::hce
#endif
