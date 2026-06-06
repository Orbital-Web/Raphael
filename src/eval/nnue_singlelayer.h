#pragma once
#ifdef EVAL_SINGLELAYER
#include <eval/nnue_state.h>
#include <eval/sparse.h>



namespace raphael::nnue {
class Nnue {
public:
    struct NnueParams {
        // accumulator: (N_PSQ + N_THREATS) -> L1_SIZE
        alignas(ALIGNMENT) i16 W0_psq[N_INBUCKETS][N_PSQ][L1_SIZE];
        alignas(ALIGNMENT) i8 W0_ti[N_THREATS][L1_SIZE];
        alignas(ALIGNMENT) i16 b0[L1_SIZE];
        // layer1: L1_SIZE -> 1
        alignas(ALIGNMENT) i16 W1[N_OUTBUCKETS][L1_SIZE];
        alignas(ALIGNMENT) i16 b1[N_OUTBUCKETS];
    };

private:
    const NnueParams* params;  // network weights and biases

    /** Loads the embedded network
     *
     * \returns the pointer to the loaded network
     */
    static const NnueParams* load_network();

    NnueState state_;


public:
    Nnue();

    /** Evaluates the board from the current side to move's perspective
     *
     * \param board current board (should match the top board state)
     * \returns the NNUE evaluation of the board in centipawns
     */
    i32 evaluate(const chess::Board& board);

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

#ifdef MEASURE_SPARSITY
    /** Saves the number of times each ft neuron fired to a file and returns the average number of
     * nonzero blocks
     *
     * \returns average number of nonzero blocks
     */
    static u64 save_ft_activations() { return 0; };
#endif
};
}  // namespace raphael::nnue
#endif
