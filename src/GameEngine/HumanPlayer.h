#pragma once
#include "GamePlayer.h"
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <string>



namespace cge { // chess game engine
class HumanPlayer: public cge::GamePlayer {
public:
    // Initializes a HumanPlayer with a name
    HumanPlayer(std::string name_in);

    // Asks the human to return a move using the UI. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, int t_remain, sf::Event& event, bool& halt);

private:
    // Converts x and y coordinates into a Square
    static chess::Square get_square(int x, int y);

    // Returns a move if the move from sq_from to sq_to is valid
    static chess::Move move_if_valid(chess::Square sq_from, chess::Square sq_to, chess::Movelist& movelist);
};  // HumanPlayer
}   // namespace cge