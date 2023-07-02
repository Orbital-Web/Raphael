#pragma once
#include <string>
#include "thc.h"

namespace cge { // chess game engine
class GamePlayer {
public:
    GamePlayer();
    virtual std::string get_move() = 0;
    virtual void update(thc::ChessRules state) = 0;
};
}   // namespace cge