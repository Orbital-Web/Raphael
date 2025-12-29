#pragma once
#include <Raphael/consts.h>

#include <chess.hpp>



namespace Raphael {
class Killers {  // based on https://rustic-chess.org/search/ordering/killers.html
private:
    chess::Move _killers[2 * MAX_DEPTH];

public:
    /** Initializes the storage with 2 killer moves per ply */
    Killers();


    // Adds killer move (assumes move is quiet)
    /** Adds a killer move (assumes move is quiet)
     *
     * \param move a quiet move
     * \param ply current distance from root
     */
    void put(const chess::Move move, const int ply);


    /** Returns whether the move is a killer move
     *
     * \param move the move to check
     * \param ply current distance from root
     * \returns whether move is a killer move
     */
    bool is_killer(const chess::Move move, const int ply) const;

    /** Clears the killer storage */
    void clear();
};
}  // namespace Raphael
