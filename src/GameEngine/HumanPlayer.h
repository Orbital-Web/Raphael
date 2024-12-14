#pragma once
#include <GameEngine/GamePlayer.h>
#include <GameEngine/utils.h>

using std::string;



namespace cge {  // chess game engine
class HumanPlayer: public GamePlayer {
public:
    /** Initializes a HumanPlayer
     *
     * \param name_in name of player
     */
    HumanPlayer(string name_in);

    /** Asks the human to make a valid move. Should return immediately if halt becomes true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param event used to access events, e.g., mouse moves and clicks
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the move made by the human
     */
    chess::Move get_move(
        chess::Board board, const int t_remain, const int t_inc, sf::Event& event, bool& halt
    );

private:
    /** Returns a move if the move from sq_from to sq_to is valid
     *
     * \param sq_from square to move from
     * \param sq_to square to move to
     * \param movelist a list of legal moves
     * \param board current board
     */
    static chess::Move move_if_valid(
        chess::Square sq_from,
        chess::Square sq_to,
        const chess::Movelist& movelist,
        const chess::Board& board
    );
};
}  // namespace cge
