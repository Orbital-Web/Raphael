#pragma once
#include <Raphael/consts.h>
#include <Raphael/simd.h>
#include <chess/include.h>

#include <string>
#include <vector>



namespace raphael {
class Nnue {
public:
    static constexpr i32 OUTPUT_SCALE = 283;

private:
    static constexpr i32 N_INPUTS = 12 * 64;  // all features
    static constexpr i32 N_HIDDEN = 64;       // accumulator size
    static constexpr i32 QA = 255;
    static constexpr i32 QB = 64;

#ifdef USE_SIMD
    const VecI16 zeros = zero_i16();
    const VecI16 qas = full_i16(QA);
#endif

    struct NnueFeature {
        chess::Piece piece;
        chess::Square square;

        /** Returns the feature index
         *
         * \param perspective feature perspective
         * \param mirror whether to mirror the board
         * \return the feature index
         */
        i32 index(chess::Color perspective, bool mirror) const;
    };

    class NnueAccumulator {
    public:
        alignas(ALIGNMENT) i16 values[N_HIDDEN];
        NnueFeature adds[2];
        NnueFeature subs[2];
        u8 n_adds = 0;
        u8 n_subs = 0;
        bool needs_refresh = false;
        bool dirty = false;

        /** Initializes the accumulator */
        NnueAccumulator();


        /** Returns whether the accumulator isn't dirty and doesn't need refresh
         *
         * \returns whether the accumulator is clean
         */
        bool is_clean() const;


        /** Adds a new piece to the accumulator
         *
         * \param piece piece to add
         * \param square square to add piece to
         */
        void add_piece(chess::Piece piece, chess::Square square);

        /** Removes a piece from the accumulator
         *
         * \param piece piece to remove
         * \param square square to remove piece from
         */
        void rem_piece(chess::Piece piece, chess::Square square);

        /** Moves a piece on the accumulator
         *
         * \param piece piece to move
         * \param from square to move from
         * \param to square to move to
         */
        void move_piece(chess::Piece piece, chess::Square from, chess::Square to);


        /** Updates the accumulator values
         *
         * \param old_acc accumulator to use as base
         * \param weights start of W0
         * \param perspective accumulator perspective
         * \param mirror whether to mirror the board
         */
        void update(
            const NnueAccumulator& old_acc,
            const i16* weights,
            chess::Color perspective,
            bool mirror
        );
    };


    struct NnueParams {
        // accumulator: N_INPUTS -> N_HIDDEN
        alignas(ALIGNMENT) i16 W0[N_INPUTS * N_HIDDEN];  // column major N_HIDDEN x 768
        alignas(ALIGNMENT) i16 b0[N_HIDDEN];
        // layer1: N_HIDDEN * 2 -> 1
        alignas(ALIGNMENT) i16 W1[2 * N_HIDDEN];  // column major 1 x (2 * N_HIDDEN)
        alignas(ALIGNMENT) i16 b1;
    };
    const NnueParams* params;  // network weights and biases

    /** Loads the embedded network
     *
     * \returns the pointer to the loaded network
     */
    static const NnueParams* load_network();

    NnueAccumulator accumulators[MAX_DEPTH][2];  // accumulators[ply][black/white][index]


    /** Refreshes an accumulators as acc = b0 + W0[features...]
     *
     * \param acc accumulator to refresh
     * \param board current board, which the accumulator will match
     * \param perspective accumulator perspective
     */
    void refresh_color(NnueAccumulator& acc, const chess::Board& board, chess::Color perspective);

public:
    Nnue();

    /** Evaluates the board specified by nnue_state[ply] from the given side's perspective
     *
     * \param ply which ply board to evaluate
     * \param color side to evaluate from
     * \returns the NNUE evaluation of the position in centipawns
     */
    i32 evaluate(i32 ply, chess::Color color);

    /** Sets nnue_state[ply=0] to match the given board
     *
     * \param board the board to set
     */
    void set_board(const chess::Board& board);

    /** Updates nnue_state[ply] based on the given move and nnue_state[ply-1]
     *
     * \param board the board to make the move on, should match nnue_state[ply-1]
     * \param move the move to make
     * \param ply which ply state to update, cannot be 0
     */
    void make_move(const chess::Board& board, chess::Move move, i32 ply);
};
}  // namespace raphael
