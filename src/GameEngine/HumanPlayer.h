#pragma once
#include <chess/include.h>



namespace cge {  // chess game engine
enum MouseEvent { NONE, LMBDOWN, RMBDOWN, LMBUP, RMBUP };
struct MouseInfo {
    i32 x;
    i32 y;
    MouseEvent event;
};


class HumanPlayer {
public:
    std::string name;

private:
    chess::Board board_;
    chess::MoveList<chess::ScoredMove> movelist_;

    chess::Move move_ = chess::Move::NO_MOVE;
    chess::Square sq_from_;
    chess::Square sq_to_;


public:
    /** Initializes a HumanPlayer
     *
     * \param name_in name of player
     */
    HumanPlayer(const std::string& name_in);


    /** Sets the position to play from
     *
     * \param board current board
     */
    void set_board(const chess::Board& board);

    /** Attempts to get a move from the human, returns immediately
     *
     * \param mouse mouse movement info
     * \returns the move made by the human or NO_MOVE
     */
    chess::Move try_get_move(const MouseInfo& mouse);

private:
    /** Returns a move if the move from sq_from_ to sq_to_ is valid
     *
     * \returns either a valid move or NO_MOVE
     */
    chess::Move move_if_valid();
};
}  // namespace cge
