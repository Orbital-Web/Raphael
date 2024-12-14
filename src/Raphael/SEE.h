#pragma once
#include <Raphael/consts.h>

#include <chess.hpp>



namespace Raphael {
// Static Exchange Evaluation
namespace SEE {
const int VAL[6] = {
    PVAL::PAWN,
    PVAL::KNIGHT,
    PVAL::BISHOP,
    PVAL::ROOK,
    PVAL::QUEEN,
    10000,
};


/** Returns the value of a piece on a square
 *
 * \param sq the square to look at
 * \param board current board
 * \returns the value of the piece
 */
int pieceval(const chess::Square sq, const chess::Board& board);

/** Returns the square of the least valuable attacker
 *
 * \param attackers attacker bitboard
 * \param board current board
 * \returns the square of the lva
 */
chess::Square lva(chess::Bitboard attackers, const chess::Board& board);

/** Returns the static exchange evaluation for a capture move
 * https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
 * http://www.talkchess.com/forum3/viewtopic.php?t=40054
 *
 * \param move the capture move
 * \param board current board
 * \returns the SEE evaluation
 */
int evaluate(const chess::Move& move, const chess::Board& board);

/** A quicker version of SEE to just compute if a capture is good or bad
 * https://github.com/rafid-dev/rice/blob/main/src/see.cpp#L5
 *
 * \param move the capture move
 * \param board current board
 * \param threshold minimum evaluation to count as good
 * \returns whether the capture is "good" or not
 */
bool goodCapture(const chess::Move& move, const chess::Board& board, const int threshold);
}  // namespace SEE
}  // namespace Raphael
