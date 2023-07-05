#pragma once
#include <string>
#include "thc.h"



namespace cge { // chess game engine
class GamePlayer {
// class variables
public:
    std::string name;


// methods
public:
    // Initializes a GamePlayer with a name
    GamePlayer(std::string name_in): name(name_in) {}

    // Returns a valid move string
    virtual std::string get_move(const thc::ChessRules& manager, int t_remain, bool& halt) = 0;
};  // GamePlayer
}   // namespace cge