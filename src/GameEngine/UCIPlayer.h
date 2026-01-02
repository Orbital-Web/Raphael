#pragma once
#include <GameEngine/GamePlayer.h>

#include <chess.hpp>



namespace cge {  // chess game engine
class UCIPlayer: public GamePlayer {
public:
    /** Initializes a UCIPlayer with a name
     *
     * \param name_in name of player
     */
    UCIPlayer(std::string name_in);

    /** Asks the UCI Engine to make a valid move. Should return immediately if halt becomes true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param mouse unused
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the found MoveEval
     */
    MoveEval get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile MouseInfo& mouse,
        volatile bool& halt
    );

private:
    void load_uci_engine(std::string uci_filepath);
};
}  // namespace cge
