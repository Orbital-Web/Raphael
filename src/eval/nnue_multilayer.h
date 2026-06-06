#pragma once
#ifdef EVAL_MULTILAYER
#include <eval/nnue_state.h>
#include <eval/sparse.h>



namespace raphael::nnue {
class Nnue {
public:
    enum class NnuePerm : u8 { NONE = 0, AVX2 = 1, AVX512 = 2 };
    struct NnueParams {
        // accumulator: (N_PSQ + N_THREATS) -> L1_SIZE
        alignas(ALIGNMENT) i16 W0_psq[N_INBUCKETS][N_PSQ][L1_SIZE];
        alignas(ALIGNMENT) i8 W0_ti[N_THREATS][L1_SIZE];
        alignas(ALIGNMENT) i16 b0[L1_SIZE];
        // layer1: L1_SIZE -> L2_SIZE
        alignas(ALIGNMENT) i8 W1[N_OUTBUCKETS][L1_SIZE / 4][L2_SIZE * 4];
        alignas(ALIGNMENT) i32 b1[N_OUTBUCKETS][L2_SIZE];
        // layer2: L2_SIZE -> L3_SIZE
        alignas(ALIGNMENT) i32 W2[N_OUTBUCKETS][L2_SIZE][L3_SIZE];
        alignas(ALIGNMENT) i32 b2[N_OUTBUCKETS][L3_SIZE];
        // layer3: L3_SIZE -> 1
        alignas(ALIGNMENT) i32 W3[N_OUTBUCKETS][L3_SIZE];
        alignas(ALIGNMENT) i32 b3[N_OUTBUCKETS];

        // flags
        NnuePerm permutation;
        bool sparsity_permed;
    };

private:
#ifdef MEASURE_SPARSITY
    static inline u64 ft_activations[L1_SIZE / 2] = {};  // number of times each ft neuron fired

    static inline u64 total_nnz = 0;
    static inline u64 total_calls = 0;
#endif

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
    static u64 save_ft_activations();
#endif

private:
    /** Activates the output of l0 (the accumulators)
     *
     * \param acc_psq stm psq accumulator values
     * \param acc_ti stm ti accumulator values
     * \param l0_out output buffer to write activated l0 outputs to
     * \param sp an iterator into the nonzero blocks of l0_out
     */
    void activate_l0(
        const i16 acc_psq[L1_SIZE],
        const i16 acc_ti[L1_SIZE],
        u8 l0_out[L1_SIZE / 2],
        [[maybe_unused]] SparseIterator& sp
    ) const;

    /** Does a forward pass through l1
     *
     * \param l0_out activated outputs of l0
     * \param l1_out output buffer to write activated l1 outputs to
     * \param sp an iterator into the nonzero blocks of l0_out
     * \param bucket_idx output bucket
     */
    void forward_l1(
        const u8 l0_out[L1_SIZE],
        i32 l1_out[L2_SIZE],
        [[maybe_unused]] const SparseIterator& sp,
        i32 bucket_idx
    ) const;

    /** Does a forward pass through l2 and l3
     *
     * \param l1_out activated outputs of l1
     * \param l3_out output buffer
     * \param bucket_idx output bucket
     */
    void forward_l2l3(const i32 l1_out[L2_SIZE], i64& l3_out, i32 bucket_idx) const;

#ifdef MEASURE_SPARSITY
    /** Updates the ft activation count and tracks the number of nonzero blocks
     *
     * \param l0_out activated l0 output for one perspective
     */
    static void record_ft_activations(const u8 l0_out[L1_SIZE / 2]);
#endif
};
}  // namespace raphael::nnue
#endif
