#pragma once



namespace raphael {
/** Whether the current score implies delivering a mate
 *
 * \param eval eval to check
 * \returns whether the eval implies a win
 */
bool is_win(int eval);

/** Whether the current score implies getting mated
 *
 * \param eval eval to check
 * \returns whether the eval implies a loss
 */
bool is_loss(int eval);

/** Whether the current scores implies a mate on either side
 *
 * \param eval eval to check
 * \returns whether the eval implies a win/loss
 */
bool is_mate(int eval);
};  // namespace raphael
