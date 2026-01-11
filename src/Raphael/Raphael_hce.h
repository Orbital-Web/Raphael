#pragma once
#include <Raphael/types.h>

#include <array>
#include <chess.hpp>



namespace raphael {
// legacy, kept for future references
class RaphaelHCE {
private:
    // evaluation[midgame, endgame]
    static constexpr int KING_DIST_WEIGHT[2] = {0, 20};  // closer king bonus
    static constexpr int DRAW_DIVIDE_SCALE = 32;         // eval divide scale by for likely draw
    static constexpr int PVAL[12][2] = {
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
    static const MultiArray<int, 12, 64, 2> PST;  // piece square table for piece, square, and phase
    static constexpr int PAWN_PASSED_WEIGHT[7][2] = {
        {0,   0  }, // promotion line
        {114, 215},
        {10,  160},
        {4,   77 },
        {-12, 47 },
        {1,   20 },
        {15,  13 },
    };  // bonus for passed pawn based on its rank
    static constexpr int PAWN_ISOLATION_WEIGHT[2] = {29, 21};  // isolated pawn cost
    static constexpr int MOBILITY_BISHOP[2] = {12, 6};         // bishop see/xray square count bonus
    static constexpr int MOBILITY_ROOK[2] = {11, 3};           // rook see/xray square count bonus
    static constexpr int BISH_PAIR_WEIGHT[2] = {39, 72};       // bishop pair bonus
    static constexpr int BISH_CORNER_WEIGHT[2] = {1, 20};      // enemy king to bishop corner bonus

    struct PMasks {
        std::array<uint64_t, 64> WPASSED;   // white passed pawn mask bitboard (A1...H8)
        std::array<uint64_t, 64> BPASSED;   // black passed pawn mask bitboard
        std::array<uint64_t, 64> ISOLATED;  // isolated pawn mask bitboard (A1...H8)
    };
    static const PMasks PMASK;


public:
    /** Statically evaluates the board from the current player's perspective
     *
     * \param board board to evaluate
     * \returns the static evaluation of the board
     */
    static int evaluate(const chess::Board& board);
};
};  // namespace raphael
