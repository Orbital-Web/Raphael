#pragma once
#include <chess.hpp>
#include <cstring>



namespace Raphael {
class History {     // based on https://www.chessprogramming.org/History_Heuristic
// History vars
private:
    int _history[2][64][64];


// History methods
public:
    // Initializes the storage with zeros
    History() {
        clear();
    }


    // Updates history (assumes move is quiet)
    void update(const chess::Move move, const int depth, const int side) {
        int from = (int)move.from();
        int to = (int)move.to();
        _history[side][from][to] += depth * depth;
    }


    // Returns the history heuristic 
    int get(const chess::Move move, const int side) const {
        int from = (int)move.from();
        int to = (int)move.to();
        return _history[side][from][to];
    }


    // Zeros out the history
    void clear() {
        std::memset(_history, 0, sizeof(_history));
    }
};  // History
}   // namespace Raphael
