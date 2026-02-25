#pragma once
#include <Raphael/consts.h>
#include <Raphael/simd.h>
#include <chess/include.h>

#include <vector>



namespace raphael {
class Nnue {
public:
    static constexpr i32 OUTPUT_SCALE = 280;

private:
    static constexpr i32 N_INPUTS = 12 * 64;  // all features
    static constexpr i32 N_HIDDEN = 512;      // accumulator size
    static constexpr i32 N_OUTBUCKETS = 8;    // output buckets
    static constexpr i32 QA = 255;
    static constexpr i32 QB = 64;

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

        /** Initializes the accumulator */
        NnueAccumulator();


        /** Returns if the accumulator needs to be updated
         *
         * \returns whether the accumulator is dirty or not
         */
        bool dirty() const;


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

        /** Resets updates stored on this accumulator */
        void reset_updates();


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

        /** Refreshes the accumulator values
         *
         * \param weights start of W0
         * \param biases start of b0
         * \param board board to use for refreshing
         * \param perspective accumulator perspective
         */
        void refresh(
            const i16* weights,
            const i16* biases,
            const chess::Board& board,
            chess::Color perspective
        );
    };


    struct NnueParams {
        // accumulator: N_INPUTS -> N_HIDDEN
        alignas(ALIGNMENT) i16 W0[N_INPUTS * N_HIDDEN];  // column major N_HIDDEN x 768
        alignas(ALIGNMENT) i16 b0[N_HIDDEN];
        // layer1: N_HIDDEN * 2 -> 1
        alignas(ALIGNMENT) i16 W1[N_OUTBUCKETS * 2 * N_HIDDEN];  // row major 8 x (2 * N_HIDDEN)
        alignas(ALIGNMENT) i16 b1[N_OUTBUCKETS];
    };
    const NnueParams* params;  // network weights and biases

    /** Loads the embedded network
     *
     * \returns the pointer to the loaded network
     */
    static const NnueParams* load_network();


    // state variables
    NnueAccumulator accumulators[MAX_DEPTH][2];  // accumulators[ply][black/white][index]
    i32 idx_ = 0;


public:
    Nnue();

    /** Evaluates the board from the current side to move's perspective
     *
     * \param board current board (should match either set_board or new_board in make_move)
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
     * \param old_board the board before the move is played
     * \param new_board the board after the move is played
     * \param move the move to make
     */
    void make_move(const chess::Board& old_board, const chess::Board& new_board, chess::Move move);

    /** Updates internal states to unmake the last move */
    void unmake_move();

private:
    /** Lazily updates the accumulator stack for one perspective
     *
     * \param board current board
     * \param perspective accumulator perspective
     */
    void lazy_update(const chess::Board& board, chess::Color perspective);
};
}  // namespace raphael
