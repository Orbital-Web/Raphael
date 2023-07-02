#pragma once
#include <string>
#include "thc.h"



namespace cge { // chess game engine
class GamePlayer {
public:
    // Returns a valid move and sets `ready=true`
    virtual thc::Move get_move(bool& ready, const thc::ChessRules& manager, int t_remain) = 0;
};  // GamePlayer
}   // namespace cge