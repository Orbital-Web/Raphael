#pragma once
#include <chess/include.h>



namespace raphael::utils {
/** Whether the current score implies delivering a mate
 *
 * \param score score to check
 * \returns whether the score implies a win
 */
bool is_win(i32 score);

/** Whether the current score implies getting mated
 *
 * \param score score to check
 * \returns whether the score implies a loss
 */
bool is_loss(i32 score);

/** Whether the current scores implies a mate on either side
 *
 * \param score score to check
 * \returns whether the score implies a win/loss
 */
bool is_mate(i32 score);

/** Computes the mate distance from a mate score
 *
 * \param score score with mate score
 * \returns mate distance
 */
i32 mate_distance(i32 score);
}  // namespace raphael::utils
