#pragma once
#include <chess/include.h>



namespace raphael::wdl {
namespace internal {
/** Returns the p_a term
 *
 * \param material current total material using 1, 3, 3, 5, 9
 * \returns p_a
 */
f64 wdl_a(i32 material);

/** Returns the p_b term
 *
 * \param material current total material using 1, 3, 3, 5, 9
 * \returns p_b
 */
f64 wdl_b(i32 material);
}  // namespace internal



struct WDL {
    i32 win;
    i32 draw;
    i32 loss;
};

/** Returns the current estimated win, draw, loss rate
 *
 * \param score unnormalized score
 * \param board current board
 * \returns the predicted wdl rates
 */
WDL get_wdl(i32 score, const chess::Board& board);

/** Normalizes the score so 1 pawn = 0.5 win rate
 *
 * \param score unnormalized score
 * \param board current board
 * \returns the normalized score
 */
i32 normalize_score(i32 score, const chess::Board& board);
}  // namespace raphael::wdl
