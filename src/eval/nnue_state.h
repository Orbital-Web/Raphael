#pragma once
#ifdef EVAL_NNUE
#include <Raphael/consts.h>
#include <eval/accumulator.h>



namespace raphael::nnue {
class NnueState {
private:
    NnueFinnyEntry finny_table_[2][2][N_INBUCKETS];  // finny_table[perspective][mirror][bucket]
    NnueAccumulator accumulators_[MAX_DEPTH];        // accumulators[ply].values[perspective][index]
    i32 idx_ = 0;

    const i16 (*weights_)[N_INPUTS][L1_SIZE];


public:
    /** Initializes the NnueState
     *
     * \param W0 start of W0 array
     * \param b0 start of b0 array
     */
    NnueState(const i16 W0[N_INBUCKETS][N_INPUTS][L1_SIZE], const i16 b0[L1_SIZE]);

    /** Lazily updates the accumulator stacks and returns the top accumulator
     *
     * \param current board (should match the top board state)
     * \returns the updated top accumulator
     */
    const NnueAccumulator& get_top_accumulator(const chess::Board& board);

    /** Sets internal states to match the given board
     *
     * \param board the board to set
     */
    void set_board(const chess::Board& board);

    /** Updates internal states based on the given move
     *
     * \param board current board (before move is played)
     * \param move the move to make
     */
    void make_move(const chess::Board& board, chess::Move move);

    /** Updates internal states to unmake the last move */
    void unmake_move();

private:
    /** Lazily updates the accumulator stack for one perspective
     *
     * \param board current board
     * \param perspective accumulator perspective
     */
    void lazy_update(const chess::Board& board, chess::Color perspective);

    /** Returns whether the features need horizontal mirroring
     *
     * \param king_sq king square for this perspective
     * \returns whether features should be horizontally mirrored
     */
    static bool needs_mirroring(chess::Square king_sq);

    /** Returns the king bucket index
     *
     * \param king_sq king square for this perspective
     * \param perspective perspective
     * \returns input bucket index
     */
    static i32 king_bucket(chess::Square king_sq, chess::Color perspective);
};
}  // namespace raphael::nnue
#endif