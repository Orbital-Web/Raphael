#pragma once
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <string>



namespace cge { // chess game engine
class GamePlayer {
// class variables
public:
    std::string name;


// methods
public:
    // Initializes a GamePlayer with a name
    GamePlayer(std::string name_in): name(name_in) {}

    // Returns a valid move string. Should return immediately if halt becomes true
    virtual chess::Move get_move(chess::Board board, int t_remain, sf::Event& event, bool& halt) = 0;


    // macros
    #define whiteturn (board.sideToMove() == chess::Color::WHITE)   // [bool] curently white's turn
    #define movename (chess::squareToString[move.from()] + chess::squareToString[move.to()])    // [string] numeric of move
};  // GamePlayer
}   // namespace cge