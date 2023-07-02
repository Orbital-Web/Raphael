#pragma once
#include "GamePlayer.h"
#include <string>
#include "thc.h"



namespace cge { // chess game engine
class HumanPlayer: public cge::GamePlayer {
public:
    HumanPlayer();

    // Asks the human to return a valid move and sets `ready=true`
    thc::Move get_move(bool& ready, const thc::ChessRules& manager, int t_remain);
};  // HumanPlayer
}   // namespace cge