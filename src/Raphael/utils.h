#pragma once
#include <Raphael/types.h>



namespace raphael {
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
};  // namespace raphael
