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
    static constexpr int N_BUCKETS = 16;                    // number of king buckets
    static constexpr int N_INPUTS = (N_BUCKETS * 12 * 64);  // half-kp features
    static constexpr int N_HIDDEN0 = 256;                   // accumulator size
    static constexpr int N_HIDDEN1 = 32;
    static constexpr int N_HIDDEN2 = 32;
    static constexpr int QLEVEL1 = 6;  // quantize w1 by scaling weights by 2^6 = 64
    static constexpr int QLEVEL2 = 6;  // quantize w2 by scaling weights by 2^6 = 64
    static constexpr int QLEVEL3 = 5;  // quantize w3 by scaling weights by 2^5 = 32

    // based on https://github.com/rafid-dev/rice/blob/main/src/nnue.h
    static constexpr int KING_BUCKETS[64] = {
        0,  1,  2,  3,  3,  2,  1,  0,   // A1, B1, ...
        4,  5,  6,  7,  7,  6,  5,  4,   //
        8,  9,  10, 11, 11, 10, 9,  8,   //
        8,  9,  10, 11, 11, 10, 9,  8,   //
        12, 12, 13, 13, 13, 13, 12, 12,  //
        12, 12, 13, 13, 13, 13, 12, 12,  //
        14, 14, 15, 15, 15, 15, 14, 14,  //
        14, 14, 15, 15, 15, 15, 14, 14,  // A8, B8, ...
    };


    struct NnueWeights {
        // accumulator: N_INPUTS -> N_HIDDEN0
        alignas(ALIGNMENT) int16_t W0[N_INPUTS * N_HIDDEN0];  // [IN][OUT]
        alignas(ALIGNMENT) int16_t b0[N_HIDDEN0];
        // layer1: N_HIDDEN0 * 2 -> N_HIDDEN1
        alignas(ALIGNMENT) int8_t W1[N_HIDDEN1 * (N_HIDDEN0 * 2)];  // [OUT][IN]
        alignas(ALIGNMENT) int32_t b1[N_HIDDEN1];
        // layer2: N_HIDDEN1 -> N_HIDDEN2
        alignas(ALIGNMENT) int8_t W2[N_HIDDEN2 * N_HIDDEN1];  // [OUT][IN]
        alignas(ALIGNMENT) int32_t b2[N_HIDDEN2];
        // layer3: N_HIDDEN2 -> 1
        alignas(ALIGNMENT) int8_t W3[N_HIDDEN2];
        int32_t b3;
    };
    NnueWeights params;  // network weights and biases

    /** Loads the network from file
     *
     * \param filepath filepath of nnue file
     */
    void load(std::string filepath);


    // nnue_state variables
    struct NnueAccumulator {
        alignas(ALIGNMENT) int16_t v[2][N_HIDDEN0];

        int16_t* operator[](bool side);
        const int16_t* operator[](bool side) const;
    };
    NnueAccumulator accumulators[MAX_DEPTH];  // accumulators[ply][black/white][index]

    /** Refreshes the accumulator as new_acc = b1 + W1[features]
     *
     * \param new_acc accumulator to refresh
     * \param features indicies of active features (King * 640, piece * 64, square)
     * \param side which side accumulator to refresh (true for white)
     */
    void refresh_accumulator(NnueAccumulator& new_acc, const std::vector<int>& features, bool side);

    /** Updates the accumulator as new_acc = old_acc + W1[add_features] - W1[rem_features]
     *
     * \param new_acc accumulator to write updated values to
     * \param old_acc accumulator to use as base
     * \param add_features indicies of features to activate
     * \param rem_features indicies of features to deactivate
     * \param side which side accumulator to update (true for white)
     */
    void update_accumulator(
        NnueAccumulator& new_acc,
        const NnueAccumulator& old_acc,
        const std::vector<int>& add_features,
        const std::vector<int>& rem_features,
        bool side
    );

    /** Computes output = (W*input + b) / 2^quantization_level
     *
     * \param w weight of size M x N (flattened)
     * \param b bias of size M
     * \param input input of size N, should be non-negative
     * \param output pre-allocated output of size M
     * \param in_size i.e., N
     * \param out_size i.e., M
     * \param quantization_level log2 of weight scaling during quantization
     */
    void linear(
        const int8_t* w,
        const int32_t* b,
        const int8_t* input,
        int32_t* output,
        int in_size,
        int out_size,
        int quantization_level
    );

    /** Computes output = clamp(input, 0, 127)
     *
     * \param input input of size N
     * \param output pre-allocated output of size N
     * \param size i.e., N
     */
    void crelu(const int32_t* input, int8_t* output, int size);
    void crelu(const int16_t* input, int8_t* output, int size);

public:
    Nnue(std::string filepath);

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
