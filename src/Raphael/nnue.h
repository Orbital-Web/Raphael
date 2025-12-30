#pragma once
#include <Raphael/consts.h>
#include <Raphael/simd.h>
#include <stdint.h>

#include <chess.hpp>
#include <string>
#include <vector>



namespace Raphael {
class Nnue {
private:
    static constexpr int N_INPUTS = 12 * 64;  // all features
    static constexpr int N_HIDDEN = 64;       // accumulator size
    static constexpr int QA = 255;
    static constexpr int QB = 64;
    static constexpr int OUTPUT_SCALE = 400;

#ifdef USE_SIMD
    const VecI16 zeros = zero_i16();
    const VecI16 qas = full_i16(QA);
#endif

    struct NnueWeights {
        // accumulator: N_INPUTS -> N_HIDDEN
        alignas(ALIGNMENT) int16_t W0[N_INPUTS * N_HIDDEN];  // [IN][OUT]
        alignas(ALIGNMENT) int16_t b0[N_HIDDEN];
        // layer1: N_HIDDEN * 2 -> 1
        alignas(ALIGNMENT) int16_t W1[2 * N_HIDDEN];  // [IN][OUT]
        alignas(ALIGNMENT) int16_t b1;
    };
    static NnueWeights params;  // network weights and biases
    static bool loaded;

    /** Loads the network from the linked binary */
    static void load();

    /** Loads the network from nnue_path
     *
     * \param nnue_path the path to load the model from
     */
    static void load(const char* nnue_path);


    // nnue_state variables
    struct NnueAccumulator {
        alignas(ALIGNMENT) int16_t v[2][N_HIDDEN];

        int16_t* operator[](bool side);
        const int16_t* operator[](bool side) const;
    };
    NnueAccumulator accumulators[MAX_DEPTH];  // accumulators[ply][black/white][index]
    struct AccumulatorState {
        bool dirty = false;
        int add1[2];
        int add2[2];
        int rem1[2];
        int rem2[2];
    };
    AccumulatorState accumulator_states[MAX_DEPTH];

    /** Refreshes the accumulator as new_acc = b1 + W1[features]
     *
     * \param new_acc accumulator to refresh
     * \param features indicies of active features
     * \param side which side accumulator to refresh (true for white)
     */
    void refresh_accumulator(NnueAccumulator& new_acc, const std::vector<int>& features, bool side);

    /** Updates the accumulator as new_acc = old_acc + W1[add_features] - W1[rem_features]
     *
     * \param new_acc accumulator to write updated values to
     * \param old_acc accumulator to use as base
     * \param add1 index of first feature to activate
     * \param add2 index of second feature to activate (-1 for none)
     * \param rem1 index of first feature to deactivate
     * \param rem2 index of second feature to deactivate (-1 for none)
     * \param side which side accumulator to update (true for white)
     */
    void update_accumulator(
        NnueAccumulator& new_acc,
        const NnueAccumulator& old_acc,
        int add1,
        int add2,
        int rem1,
        int rem2,
        bool side
    );

public:
    Nnue();
    Nnue(const std::string& nnue_path);

    /** Evaluates the board specified by nnue_state[ply] from the given side's perspective
     *
     * \param ply which ply board to evaluate
     * \param side side to evaluate from (true for white)
     * \returns the NNUE evaluation of the position in centipawns
     */
    int32_t evaluate(int ply, bool side);

    /** Sets nnue_state[ply=0] to reflect the given board
     *
     * \param board the board to set
     */
    void set_board(const chess::Board& board);

    /** Updates nnue_state[ply] based on the given move and nnue_state[ply-1]
     *
     * \param ply which ply state to update, cannot be 0
     * \param move the move to make
     * \param board the board to make the move on, should match nnue_state[ply-1]
     */
    void make_move(int ply, const chess::Move& move, const chess::Board& board);
};
}  // namespace Raphael
