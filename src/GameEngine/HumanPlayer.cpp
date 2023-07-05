#include "HumanPlayer.h"
#include <string>
#include <iostream>



// Initializes a HumanPlayer with a name
cge::HumanPlayer::HumanPlayer(std::string name_in): GamePlayer(name_in) {}


// Asks the human to return a valid move string
std::string cge::HumanPlayer::get_move(const thc::ChessRules& manager, int t_remain, bool& halt) {
    std::string movestr = "";
    //std::cout << "Pick move: ";
    //std::cin >> movestr;
    while (!halt) {

    }
    return movestr;
}