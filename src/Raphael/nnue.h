#pragma once
#include <Raphael/consts.h>
#include <Raphael/simd.h>
#include <chess/include.h>

#include <string>
#include <vector>



namespace raphael {
class Nnue {
public:
    static constexpr i32 OUTPUT_SCALE = 286;

private:
    static constexpr i32 N_INPUTS = 12 * 64;  // all features
    static constexpr i32 N_HIDDEN = 64;       // accumulator size
    static constexpr i32 QA = 255;
    static constexpr i32 QB = 64;

#ifdef USE_SIMD
    const VecI16 zeros = zero_i16();
    const VecI16 qas = full_i16(QA);
#endif

    struct NnueWeights {
        // accumulator: N_INPUTS -> N_HIDDEN
        alignas(ALIGNMENT) i16 W0[N_INPUTS * N_HIDDEN];  // column major N_HIDDEN x 768
        alignas(ALIGNMENT) i16 b0[N_HIDDEN];
        // layer1: N_HIDDEN * 2 -> 1
        alignas(ALIGNMENT) i16 W1[2 * N_HIDDEN];  // column major 1 x (2 * N_HIDDEN)
        alignas(ALIGNMENT) i16 b1;
    };
    const NnueWeights* params;  // network weights and biases

    /** Loads the embedded network
     *
     * \returns the pointer to the loaded network
     */
    static const NnueWeights* load_network();


    // nnue_state variables
    struct NnueAccumulator {
        alignas(ALIGNMENT) i16 v[2][N_HIDDEN];

        i16* operator[](chess::Color color);
        const i16* operator[](chess::Color color) const;
    };
    NnueAccumulator accumulators[MAX_DEPTH];  // accumulators[ply][black/white][index]
    struct AccumulatorState {
        bool dirty = false;
        i32 add1[2];
        i32 add2[2];
        i32 rem1[2];
        i32 rem2[2];
    };
    AccumulatorState accumulator_states[MAX_DEPTH];

    /** Refreshes the accumulator as new_acc = b1 + W1[features]
     *
     * \param new_acc accumulator to refresh
     * \param features indicies of active features
     * \param color which color accumulator to refresh
     */
    void refresh_accumulator(
        NnueAccumulator& new_acc, const std::vector<i32>& features, chess::Color color
    );

    /** Updates the accumulator as new_acc = old_acc + W1[add_features] - W1[rem_features]
     *
     * \param new_acc accumulator to write updated values to
     * \param old_acc accumulator to use as base
     * \param add1 index of first feature to activate
     * \param add2 index of second feature to activate (-1 for none)
     * \param rem1 index of first feature to deactivate
     * \param rem2 index of second feature to deactivate (-1 for none)
     * \param color which color accumulator to update
     */
    void update_accumulator(
        NnueAccumulator& new_acc,
        const NnueAccumulator& old_acc,
        i32 add1,
        i32 add2,
        i32 rem1,
        i32 rem2,
        chess::Color color
    );

public:
    Nnue();
    Nnue(const std::string& nnue_path);

    /** Evaluates the board specified by nnue_state[ply] from the given side's perspective
     *
     * \param ply which ply board to evaluate
     * \param color side to evaluate from
     * \returns the NNUE evaluation of the position in centipawns
     */
    i32 evaluate(i32 ply, chess::Color color);

    /** Sets nnue_state[ply] to match the given board
     *
     * \param board the board to set
     * \param ply which ply state to update, default 0
     */
    void set_board(const chess::Board& board, i32 ply);

    /** Updates nnue_state[ply] based on the given move and nnue_state[ply-1]
     *
     * \param board the board to make the move on, should match nnue_state[ply-1]
     * \param move the move to make
     * \param ply which ply state to update, cannot be 0
     */
    void make_move(const chess::Board& board, chess::Move move, i32 ply);
};
}  // namespace raphael
