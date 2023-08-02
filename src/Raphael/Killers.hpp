#pragma once
#include <chess.hpp>
#include <Raphael/consts.hpp>



namespace Raphael {
class Killers {     // based on https://rustic-chess.org/search/ordering/killers.html
// class variables
private:
    std::vector<chess::Move> _killers;



// methods
public:
    // Initialize the storage with 2 killer moves per ply
    Killers(): _killers(MAX_DEPTH*2, chess::Move::NO_MOVE) {}


    // Add killer move (assumes move is not a capture/promotion)
    void put(const chess::Move move, const int ply) {
        if (move != _killers[2*ply]) {
            _killers[2*ply+1] = _killers[2*ply];    // override old
            _killers[2*ply] = move;                 // override current
        }
    }


    // Whether the move is a killer move
    bool isKiller(const chess::Move move, const int ply) const {
        return (move==_killers[2*ply] || move==_killers[2*ply+1]);
    }


    // Clears the killer storage
    void clear() {
        std::fill(_killers.begin(), _killers.end(), chess::Move::NO_MOVE);
    }
};  // Killers
}   // namespace name
