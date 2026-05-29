#pragma once
#ifdef EVAL_NNUE
#include <eval/arch.h>
#include <eval/simd.h>



namespace raphael::nnue {
struct PSQFeature {
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
    NnueFinnyEntry() = default;

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
    void sync(
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
    enum class PSQState : u8 { CLEAN = 0, DIRTY, REFRESH };

    alignas(ALIGNMENT) i16 values[2][L1_SIZE];

private:
    StaticVector<PSQFeature, 2> psq_adds;
    StaticVector<PSQFeature, 2> psq_subs;
    PSQState psq_state[2];


public:
    /** Initializes the accumulator */
    NnueAccumulator() = default;

    /** Returns the stm accumulator state
     *
     * \param perspective which accumulator to check
     * \returns the PSQ state of the stm accumulator
     */
    PSQState get_psq_state(chess::Color perspective) const;

    /** Sets the stm accumulator state
     *
     * \param perspective side to set
     * \param state new state
     */
    void set_psq_state(chess::Color perspective, PSQState state);

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

    /** Prepares this accumulator for updates */
    void prepare_updates();

    /** Updates the accumulator values
     *
     * \param old_acc accumulator to use as base
     * \param weights start of W0
     * \param perspective accumulator perspective
     * \param mirror whether to mirror the board
     */
    void apply_updates(
        const NnueAccumulator& old_acc,
        const i16 weights[N_INPUTS][L1_SIZE],
        chess::Color perspective,
        bool mirror
    );

    /** Refreshes the stm accumulator by copying the finny entry
     *
     * \param finny_entry finny entry to copy
     * \param perspective accumulator perspective
     */
    void refresh_from(const NnueFinnyEntry& finny_entry, chess::Color perspective);
};
}  // namespace raphael::nnue
#endif