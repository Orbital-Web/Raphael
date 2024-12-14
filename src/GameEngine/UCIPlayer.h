#pragma once
#include <GameEngine/GamePlayer.h>

#include <SFML/Graphics.hpp>
#include <chess.hpp>

using std::string;



namespace cge {  // chess game engine
class UCIPlayer: public GamePlayer {
public:
    /** Initializes a UCIPlayer with a name
     *
     * \param name_in name of player
     */
    UCIPlayer(string name_in);

    /** Asks the UCI Engine to make a valid move. Should return immediately if halt becomes true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param event used to access events, e.g., mouse moves for human players
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the best move it found
     */
    chess::Move get_move(
        chess::Board board, const int t_remain, const int t_inc, sf::Event& event, bool& halt
    );

private:
    void load_uci_engine(string uci_filepath);
};
}  // namespace cge
