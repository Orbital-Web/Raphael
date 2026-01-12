#pragma once
#include <Raphael/types.h>

#include <chess.hpp>



namespace raphael {
class History {  // based on https://www.chessprogramming.org/History_Heuristic
private:
    i32 _history[2][64][64];

public:
    /** Initializes the history with zeros */
    History();

    /** Updates the history with gravity
     *
     * \param bestmove a quiet move to boost
     * \param quietlist quiet moves to penalize (should include `bestmove`, `bestmove` is boosted)
     * \param depth depth of that move
     * \param side side playing the move
     */
    void update(const chess::Move& bestmove, const chess::Movelist& quietlist, i32 depth, i32 side);

    /** Returns the history heuristic
     *
     * \param move the move to look for
     * \param side side playing the move
     * \returns the stored history of that move
     */
    i32 get(const chess::Move& move, bool side) const;

    /** Zeros out the history */
    void clear();
};
}  // namespace raphael
