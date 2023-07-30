#pragma once
#include "utils.hpp"
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
    virtual chess::Move get_move(chess::Board board, float t_remain, sf::Event& event, bool& halt) = 0;

    // Think during opponent's turn. Should return immediately if halt becomes true
    virtual void ponder(chess::Board board, bool& halt) {}

    // Resets the player
    virtual void reset() {}
};  // GamePlayer
}   // namespace cge