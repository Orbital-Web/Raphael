#pragma once
#include <Raphael/types.h>

#include <chess.hpp>



namespace raphael::utils {
/** Whether the current score implies delivering a mate
 *
 * \param eval eval to check
 * \returns whether the eval implies a win
 */
bool is_win(i32 eval);

/** Whether the current score implies getting mated
 *
 * \param eval eval to check
 * \returns whether the eval implies a loss
 */
bool is_loss(i32 eval);

/** Whether the current scores implies a mate on either side
 *
 * \param eval eval to check
 * \returns whether the eval implies a win/loss
 */
bool is_mate(i32 eval);

/** Computes the mate distance from a mate eval
 *
 * \param eval eval with mate score
 * \returns mate distance
 */
i32 mate_distance(i32 eval);



/** Returns the current side to move
 *
 * \param board current board
 * \returns the stm (white = true)
 */
bool stm(const chess::Board& board);

/** Determines if a move is quiet
 *
 * \param move move to classify
 * \param board current board
 * \return whether the move is quiet or not
 */
bool is_quiet(const chess::Move& move, const chess::Board& board);

/** Returns the Piece that was captured
 *
 * \param move either a normal capture or enpassant move (not castling)
 * \param board current board
 * \param the piece that was captured
 */
chess::Piece piece_captured(const chess::Move& move, const chess::Board& board);
}  // namespace raphael::utils
