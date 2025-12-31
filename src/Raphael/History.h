#pragma once
#include <chess.hpp>



namespace Raphael {
class History {  // based on https://www.chessprogramming.org/History_Heuristic
private:
    const int bonus_scale;
    const int bonus_offset;
    const int bonus_max;
    const int history_max;

    int _history[2][64][64];

public:
    /** Initializes the storage with zeros
     *
     * \param bonus_scale depth multiplier scale of bonus
     * \param bonus_offset offset to bonus
     * \param bonus_max maximum bonus
     * \param history_max maximum value of history score
     */
    History(
        const int bonus_scale, const int bonus_offset, const int bonus_max, const int history_max
    );

    /** Updates the history with gravity
     *
     * \param bestmove a quiet move to boost
     * \param quietlist quiet moves to penalize (should include `bestmove`, `bestmove` is boosted)
     * \param depth depth of that move
     * \param side side playing the move
     */
    void update(
        const chess::Move bestmove,
        const chess::Movelist& quietlist,
        const int depth,
        const int side
    );

    /** Returns the history heuristic
     *
     * \param move the move to look for
     * \param side side playing the move
     * \returns the stored history of that move
     */
    int get(const chess::Move move, const int side) const;

    /** Zeros out the history */
    void clear();
};
}  // namespace Raphael
