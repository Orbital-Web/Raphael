#pragma once
#include <chess.hpp>
#include <SFML/Graphics.hpp>



namespace cge { // chess game engine
class GamePlayer {
// GamePlayer vars
public:
    std::string name;


// GamePlayer methods
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