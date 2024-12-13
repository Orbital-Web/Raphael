#pragma once
#include <chess.hpp>
#include <SFML/Graphics.hpp>
#include <GameEngine/utils.hpp>
#include <GameEngine/GamePlayer.hpp>



namespace cge { // chess game engine
class UCIPlayer: public GamePlayer {
// HumanPlayer methods
public:
    // Initializes a UCIPlayer with a name
    UCIPlayer(std::string name_in): GamePlayer(name_in) {
        load_uci_engine("src/Games/stockfish-windows-x86-64-avx2.exe");
    }


    // Asks the human to return a move using the UI. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, const int t_remain, const int t_inc, sf::Event& event, bool& halt) {
        return chess::Move::NO_MOVE;
    }

private:
    void load_uci_engine(std::string uci_filepath) {
    }
};  // UCIPlayer
}   // namespace cge