#include "HumanPlayer.h"
#include <string>
#include <iostream>



// Asks the human to return a valid move string
std::string cge::HumanPlayer::get_move(const thc::ChessRules& manager, int t_remain) {
    std::string movestr = "";
    std::cout << "Pick move: ";
    std::cin >> movestr;
    return movestr;
}