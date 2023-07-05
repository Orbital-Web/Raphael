#pragma once
#include "GamePlayer.h"
#include <string>
#include "thc.h"



namespace cge { // chess game engine
class HumanPlayer: public cge::GamePlayer {
public:
    // Initializes a HumanPlayer with a name
    HumanPlayer(std::string name_in);

    // Asks the human to return a valid move string
    std::string get_move(const thc::ChessRules& manager, int t_remain, bool& halt);
};  // HumanPlayer
}   // namespace cge