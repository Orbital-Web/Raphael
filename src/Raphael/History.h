#pragma once
#include <chess.hpp>



namespace Raphael {
class History {  // based on https://www.chessprogramming.org/History_Heuristic
private:
    int _history[2][64][64];

public:
    /** Initializes the storage with zeros */
    History();

    /** Updates the history (assumes move is quiet)
     *
     * \param move a quiet move
     * \param depth depth of that move
     * \param side side playing the move
     */
    void update(const chess::Move move, const int depth, const int side);

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
