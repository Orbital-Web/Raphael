#pragma once
#include <GameEngine/GamePlayer.h>



namespace cge {  // chess game engine
class HumanPlayer: public GamePlayer {
public:
    /** Initializes a HumanPlayer
     *
     * \param name_in name of player
     */
    HumanPlayer(const std::string& name_in);

    /** Asks the human to make a valid move. Should return immediately if halt becomes true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param mouse contains mouse movement info
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the move made by the human
     */
    MoveEval get_move(
        chess::Board board,
        const i32 t_remain,
        const i32 t_inc,
        volatile MouseInfo& mouse,
        volatile bool& halt
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
        const chess::MoveList<chess::ScoredMove>& movelist,
        const chess::Board& board
    );
};
}  // namespace cge
