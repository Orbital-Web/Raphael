#pragma once
#include <cstdint>



namespace Raphael {
// should remain largely unchanged
const int MAX_DEPTH = 128;
const int MATE_EVAL = 2000000000;          // evaluation for immediate mate
const uint32_t DEF_TABLE_SIZE = 12582912;  // default no. entries in TranspositionTable (192mb)



// Past Pawn and Isolated Pawn Mask
// & with the enemy pieces to determine whether the pawn at the square is a past pawn
// & with our pieces to determine whether the pawn at the file is isolated
namespace PMASK {
extern uint64_t WPASSED[64];   // passed pawn mask bitboard for white (A1...H8)
extern uint64_t BPASSED[64];   // passed pawn mask bitboard for black
extern uint64_t ISOLATED[64];  // isolated pawn mask bitboard (A1...H8)

void init_pawnmask();
}  // namespace PMASK
}  // namespace Raphael
