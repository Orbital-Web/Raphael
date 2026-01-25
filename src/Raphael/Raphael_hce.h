#pragma once
#include <chess/include.h>

#include <array>



namespace raphael {
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
    /** Statically evaluates the board from the current player's perspective
     *
     * \param board board to evaluate
     * \returns the static evaluation of the board
     */
    static i32 evaluate(const chess::Board& board);
};
}  // namespace raphael
