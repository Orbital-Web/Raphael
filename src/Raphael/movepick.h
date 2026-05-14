#pragma once
#include <Raphael/History.h>



namespace raphael {
// inspired by https://github.com/Ciekce/Stormphrax/blob/main/src/movepick.h
class MoveGenerator {
public:
    enum class Stage : i32 {
        // negamax
        TT_MOVE,
        GEN_NOISY,
        GOOD_NOISY,
        GEN_QUIET,
        QUIET,
        BAD_NOISY,

        // quiescence
        QS_TT_MOVE,
        QS_GEN_NOISY,
        QS_NOISY,
        // only if in check
        QS_GEN_QUIET,
        QS_QUIET
    };

private:
    Stage stage_;
    chess::MoveList<chess::ScoredMove>* movelist_;
    const Position<true>* position_;
    const History* history_;
    chess::Move ttmove_ = chess::Move::NO_MOVE;

    bool skip_quiets_ = false;
    usize idx_ = 0;
    usize end_ = 0;
    usize bad_noisy_end_ = 0;


public:
    /** Initializes a move generator for the negamax search
     *
     * \param movelist pointer to preallocated move list to use
     * \param position pointer to current position
     * \param history pointer to history table
     * \param ttmove transposition table move
     * \returns the move generator
     */
    static MoveGenerator negamax(
        chess::MoveList<chess::ScoredMove>* movelist,
        const Position<true>* position,
        const History* history,
        chess::Move ttmove
    );

    /** Initializes a move generator for the quiescence search
     *
     * \param movelist pointer to preallocated move list to use
     * \param position pointer to current position
     * \param history pointer to history table
     * \param ttmove transposition table move
     * \returns the move generator
     */
    static MoveGenerator quiescence(
        chess::MoveList<chess::ScoredMove>* movelist,
        const Position<true>* position,
        const History* history,
        chess::Move ttmove
    );


    /** Returns the next move
     *
     * \returns the next move in the generator
     */
    chess::Move next();

    /** Signal move generator to skip quiet moves */
    void skip_quiets();

private:
    /** Initializes the move generator
     *
     * \param start_stage the starting stage of the generator
     * \param movelist pointer to preallocated move list to use
     * \param position pointer to current position
     * \param history pointer to history table
     * \param ttmove transposition table move
     */
    MoveGenerator(
        Stage start_stage,
        chess::MoveList<chess::ScoredMove>* movelist,
        const Position<true>* position,
        const History* history,
        chess::Move ttmove
    );


    /** Scores noisy moves between idx_ and end_ */
    void score_noisies();

    /** Scores quiet moves between idex_ and end_ */
    void score_quiets();


    /** Does a selection sort to return the index of the next highest scored move in the movelist
     * between idx_ and end_
     *
     * \returns index of next highest scored move in the movelist
     */
    usize select_next();
};
}  // namespace raphael
