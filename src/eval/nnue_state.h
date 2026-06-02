#pragma once
#ifdef EVAL_NNUE
#include <Raphael/consts.h>
#include <eval/accumulator.h>



namespace raphael::nnue {
class NnueState {
private:
    NnueFinnyEntry finny_table_[2][2][N_INBUCKETS];  // finny_table[perspective][mirror][bucket]
    NnueAccumulator accumulators_[MAX_DEPTH];        // accumulators[ply][perspective][index]
    i32 idx_ = 0;

    const i16 (*psq_weights_)[N_PSQ][L1_SIZE];
    const i8 (*ti_weights_)[L1_SIZE];


public:
    /** Initializes the NnueState
     *
     * \param W0_psq start of psq W0 array
     * \param W0_ti start of ti W0 array
     * \param b0 start of b0 array
     */
    NnueState(
        const i16 W0_psq[N_INBUCKETS][N_PSQ][L1_SIZE],
        const i8 W0_ti[N_THREATS][L1_SIZE],
        const i16 b0[L1_SIZE]
    );

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

    /** Adds psq and ti updates for a piece addition to a square
     *
     * \param board current board (before addition)
     * \param piece piece to add
     * \param sq square to add piece to
     */
    void add_piece(const chess::Board& board, chess::Piece piece, chess::Square sq);

    /** Adds psq and ti updates for a piece removal from a square
     *
     * \param board current board (before removal)
     * \param piece piece to remove
     * \param sq square to remove piece from
     */
    void rem_piece(const chess::Board& board, chess::Piece piece, chess::Square sq);

    /** Adds psq and ti updates for a piece move
     *
     * \param board current board (before move)
     * \param from_piece piece to move
     * \param to_piece piece at destination square
     * \param from_sq source square
     * \param to_sq destination square
     */
    void move_piece(
        const chess::Board& board,
        chess::Piece from_piece,
        chess::Piece to_piece,
        chess::Square from_sq,
        chess::Square to_sq
    );

    /** Adds psq and ti updates for a piece mutation
     *
     * \param board current board (before mutation)
     * \param from_piece piece to mutate
     * \param to_piece new piece type
     * \param sq square to mutate piece on
     */
    void mutate_piece(
        const chess::Board& board, chess::Piece from_piece, chess::Piece to_piece, chess::Square sq
    );
};
}  // namespace raphael::nnue
#endif