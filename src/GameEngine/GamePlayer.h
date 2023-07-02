#pragma once
#include <string>
#include "thc.h"



namespace cge { // chess game engine
class GamePlayer {
public:
    // Returns a valid move string
    virtual std::string get_move(const thc::ChessRules& manager, int t_remain) = 0;
};  // GamePlayer
}   // namespace cge