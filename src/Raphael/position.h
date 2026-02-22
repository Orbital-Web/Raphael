#pragma once
#include <Raphael/nnue.h>
#include <chess/include.h>



namespace raphael {
class Position {
private:
    chess::Board current_;
    std::vector<chess::Board> boards_;

    Nnue net_;

public:
    /** Initializes the position to startpos */
    Position();

    /** Initializes the position with a set board
     *
     * \param board board to set to
     */
    Position(const chess::Board& board);

    /** Sets the position's board
     *
     * \param board board to set to
     */
    void set_board(const chess::Board& board);


    /** Returns the current board
     *
     * \returns current board
     */
    const chess::Board board() const;


    /** Checks if the position is in repetition
     *
     * \param count number of times we must see the same move to report a repetition
     * \returns whether the position is in repetition
     */
    bool is_repetition(i32 count = 2) const;


    /** Evaluates the current board from the current side to move
     *
     * \returns the NNUE evaluation of the board in centipawns
     */
    i32 evaluate();


    /** Plays a move
     *
     * \param move move to play
     */
    void make_move(chess::Move move);

    /** Plays a nullmove */
    void make_nullmove();

    /** Unmakes the last move */
    void unmake_move();

    /** Unmakes a nullmove */
    void unmake_nullmove();
};
}  // namespace raphael
