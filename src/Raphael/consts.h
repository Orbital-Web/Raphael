#pragma once
#include <cstdint>



namespace Raphael {
// should remain largely unchanged
const int MAX_DEPTH = 128;
const int MATE_EVAL = 2000000000;  // evaluation for immediate mate

// tunable
const uint32_t DEF_TABLE_SIZE = 12582912;  // default no. entries in TranspositionTable (192mb)
const int ASPIRATION_WINDOW = 50;          // size of aspiration window
const int MAX_EXTENSIONS = 16;             // max number of times we can extend the search
const int REDUCTION_FROM = 5;              // from which move to apply late move reduction from
const int N_PIECES_END = 8;                // pieces left to count as endgame
const int PV_STABLE_COUNT = 6;    // number of consecutive bestmoves to consider pv stable and halt
const int KING_DIST_WEIGHT = 20;  // how important king proximity is for the evaluation at endgame
const int KILLER_WEIGHT = 50;     // move ordering priority for killer moves
const int GOOD_CAPTURE_WEIGHT = 5000;  // move ordering priority for good captures



// Value of each piece
// https://lichess.org/@/ubdip/blog/finding-the-value-of-pieces/PByOBlNB
namespace PVAL {
const int PAWN = 100;
const int KNIGHT = 316;
const int BISHOP = 328;
const int ROOK = 493;
const int QUEEN = 983;
const int KING = 0;

// combined list
const int VALS[12] = {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    -PAWN,
    -KNIGHT,
    -BISHOP,
    -ROOK,
    -QUEEN,
    -KING,
};
}  // namespace PVAL



// Piece-Square Tables for each piece (midgame and endgame)
// https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
// Use MID[piece][square] and END[piece][square] as it contains black's PST
namespace PST {
// combined table
extern int MID[12][64];  // PNBRQKpnbrqk, A1...H8
extern int END[12][64];  // PNBRQKpnbrqk, A1...H8

// must be called for MID and END to function properly
void init_pst();
}  // namespace PST



// Past Pawn and Isolated Pawn Mask
// & with the enemy pieces to determine whether the pawn at the square is a past pawn
// & with our pieces to determine whether the pawn at the file is isolated
namespace PMASK {
const int PASSEDBONUS[7] = {  // bonus for passed pawn based on distance to promotion line
    0,
    115,
    80,
    50,
    30,
    15,
    10
};
const int ISOLATION_WEIGHT = 30;  // penalty for an isolated pawn

extern uint64_t WPASSED[64];   // passed pawn mask bitboard for white (A1...H8)
extern uint64_t BPASSED[64];   // passed pawn mask bitboard for black
extern uint64_t ISOLATED[64];  // isolated pawn mask bitboard (A1...H8)

void init_pawnmask();
}  // namespace PMASK



// mobility
namespace MOBILITY {
const int BISH_MID = 2;
const int BISH_END = 3;
const int ROOK_MID = 1;
const int ROOK_END = 2;
}  // namespace MOBILITY
}  // namespace Raphael
