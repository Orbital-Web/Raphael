#pragma once
#include <Raphael/consts.h>
#include <Raphael/simd.h>
#include <chess/include.h>

#include <vector>



namespace raphael {
class Nnue {
public:
    static constexpr i32 OUTPUT_SCALE = 274;

private:
    static constexpr i32 N_INPUTS = 11 * 64;
    static constexpr i32 L1_SIZE = 1024;
    static constexpr i32 L2_SIZE = 16;
    static constexpr i32 L3_SIZE = 32;
    static constexpr i32 N_OUTBUCKETS = 8;
    static constexpr i32 QA = 255;
    static constexpr i32 QB = 64;
    static constexpr i32 QC = 64;
    static constexpr i32 N_INBUCKETS = 16;
    static constexpr i32 BUCKETS[32] = {  // clang-format off
        0,  1,  2,  3,
        4,  5,  6,  7,
        8,  8,  9,  9,
        10, 10, 11, 11,
        12, 12, 13, 13,
        12, 12, 13, 13,
        14, 14, 15, 15,
        14, 14, 15, 15
    };  // clang-format on
    static constexpr i32 L1_SHIFT = 8;

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

    class NnueFinnyEntry {
    public:
        alignas(ALIGNMENT) i16 values[L1_SIZE];

    private:
        std::array<chess::BitBoard, 6> pieces_ = {};  // bitboard per piece type
        std::array<chess::BitBoard, 2> occ_ = {};     // bitboard per color


    public:
        /** Dummy constructor */
        NnueFinnyEntry();

        /** Initializes the finny entry to equal the bias value
         *
         * \param biases start of b0
         */
        void initialize(const i16 biases[L1_SIZE]);

        /** Updates the finny entry incrementally to match the new board state
         *
         * \param weights start of W0
         * \param board new board state, should match this entry's king bucket index & mirroring
         * \param perspective accumulator perspective, should match this entry's perspective
         * \param mirror whether to mirror the board, should match this entry's mirroring
         */
        void update(
            const i16 weights[N_INPUTS][L1_SIZE],
            const chess::Board& board,
            chess::Color perspective,
            bool mirror
        );

    private:
        /** Returns the occupancy BitBoard for a piecetype and color
         *
         * \param pt piecetype
         * \param color color
         * \returns occupancy bitboard
         */
        chess::BitBoard occ(chess::PieceType pt, chess::Color color) const;
    };

    class NnueAccumulator {
    public:
        alignas(ALIGNMENT) i16 values[L1_SIZE];
        NnueFeature adds[2];
        NnueFeature subs[2];
        u8 n_adds = 0;
        u8 n_subs = 0;
        bool needs_refresh = false;

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
            const i16 weights[N_INPUTS][L1_SIZE],
            chess::Color perspective,
            bool mirror
        );

        /** Refreshes the accumulator by copying the finny entry
         *
         * \param finny_entry finny entry to copy
         */
        void refresh_from(const NnueFinnyEntry& finny_entry);
    };


    struct NnueParams {
        // accumulator: N_INPUTS -> L1_SIZE
        alignas(ALIGNMENT) i16 W0[N_INBUCKETS][N_INPUTS][L1_SIZE];
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
    };
    const NnueParams* params;  // network weights and biases

    /** Loads the embedded network
     *
     * \returns the pointer to the loaded network
     */
    static const NnueParams* load_network();


    // state variables
    NnueFinnyEntry finny_table[2][2][N_INBUCKETS];  // finny_table[perspective][mirror][bucket]
    NnueAccumulator accumulators[MAX_DEPTH][2];     // accumulators[ply][perspective][index]
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
     * \param board current board (before move is played)
     * \param move the move to make
     */
    void make_move(const chess::Board& board, chess::Move move);

    /** Updates internal states to unmake the last move */
    void unmake_move();

private:
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

    /** Lazily updates the accumulator stack for one perspective
     *
     * \param board current board
     * \param perspective accumulator perspective
     */
    void lazy_update(const chess::Board& board, chess::Color perspective);

    /** Activates the output of l0 (the accumulators)
     *
     * \param acc accumulator of perspective
     * \param l0_out output buffer to write activated l0 outputs to
     */
    void activate_l0(const NnueAccumulator& acc, u8 l0_out[L1_SIZE / 2]) const;

    /** Does a forward pass through l1
     *
     * \param l0_out activated outputs of l0
     * \param l1_out output buffer to write activated l1 outputs to
     * \param bucket_idx output bucket
     */
    void forward_l1(const u8 l0_out[L1_SIZE], i32 l1_out[L2_SIZE], i32 bucket_idx) const;

    /** Does a forward pass through l2 and l3
     *
     * \param l1_out activated outputs of l1
     * \param l3_out output buffer
     * \param bucket_idx output bucket
     */
    void forward_l2l3(const i32 l1_out[L2_SIZE], i64& l3_out, i32 bucket_idx) const;
};
}  // namespace raphael
