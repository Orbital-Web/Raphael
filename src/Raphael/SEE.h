#pragma once

#include <chess.hpp>



namespace raphael::SEE {

namespace internal {
const int VAL[13] = {
    100,
    422,
    437,
    694,
    1313,
    10000,
    100,
    422,
    437,
    694,
    1313,
    10000,
    0,
};


/** Returns the value of a piece on a square
 *
 * \param sq the square to look at
 * \param board current board
 * \returns the value of the piece
 */
int pieceval(chess::Square sq, const chess::Board& board);

/** Returns the square of the least valuable attacker
 *
 * \param attackers attacker bitboard
 * \param board current board
 * \returns the square of the lva
 */
chess::Square lva(chess::Bitboard attackers, const chess::Board& board);
};  // namespace internal



/** Simulates exchanges on the move destination square to evaluate if it's winning or losing
 * https://github.com/rafid-dev/rice/blob/main/src/see.cpp#L5
 *
 * \param move the move to evaluate
 * \param board current board
 * \param threshold minimum evaluation to count as good
 * \returns whether the move is "good" or not
 */
bool see(const chess::Move& move, const chess::Board& board, int threshold);
}  // namespace raphael::SEE
