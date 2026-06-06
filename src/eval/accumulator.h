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

struct TIFeature {
    chess::Piece attacker;
    chess::Square attacker_sq;
    chess::Piece attacked;
    chess::Square attacked_sq;

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
     * \param weights start of W0_psq
     * \param board new board state, should match this entry's king bucket index & mirroring
     * \param perspective accumulator perspective, should match this entry's perspective
     * \param mirror whether to mirror the board, should match this entry's mirroring
     */
    void sync(
        const i16 weights[N_PSQ][L1_SIZE],
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
    enum class AccState : u8 { CLEAN = 0, DIRTY, REFRESH };

    alignas(ALIGNMENT) i16 psq_vals[2][L1_SIZE];
    alignas(ALIGNMENT) i16 ti_vals[2][L1_SIZE];

private:
    StaticVector<PSQFeature, 2> psq_adds;
    StaticVector<PSQFeature, 2> psq_subs;
    StaticVector<TIFeature, 128> ti_adds;
    StaticVector<TIFeature, 128> ti_subs;

    AccState psq_state[2];
    AccState ti_state[2];


public:
    /** Initializes the accumulator */
    NnueAccumulator() = default;

    /** Returns the stm psq accumulator state
     *
     * \param perspective which accumulator to check
     * \returns the psq state of the stm accumulator
     */
    AccState get_psq_state(chess::Color perspective) const;

    /** Sets the stm psq accumulator state
     *
     * \param perspective side to set
     * \param state new state
     */
    void set_psq_state(chess::Color perspective, AccState state);

    /** Returns the stm ti accumulator state
     *
     * \param perspective which accumulator to check
     * \returns the ti state of the stm accumulator
     */
    AccState get_ti_state(chess::Color perspective) const;

    /** Sets the stm ti accumulator state
     *
     * \param perspective side to set
     * \param state new state
     */
    void set_ti_state(chess::Color perspective, AccState state);

    /** Adds a psq feature to the accumulator
     *
     * \param piece piece to add
     * \param square square to add piece to
     */
    void add_psq(chess::Piece piece, chess::Square square);

    /** Removes a psq feature from the accumulator
     *
     * \param piece piece to remove
     * \param square square to remove piece from
     */
    void rem_psq(chess::Piece piece, chess::Square square);

    /** Adds a ti feature to the accumulator
     *
     * \param attacker attacking piece
     * \param attacked attacked piece
     * \param attacker_sq square of attacking piece
     * \param attacked_sq square of attacked piece
     */
    void add_ti(
        chess::Piece attacker,
        chess::Piece attacked,
        chess::Square attacker_sq,
        chess::Square attacked_sq
    );

    /** Removes a ti feature from the accumulator
     *
     * \param attacker attacking piece
     * \param attacked attacked piece
     * \param attacker_sq square of attacking piece
     * \param attacked_sq square of attacked piece
     */
    void rem_ti(
        chess::Piece attacker,
        chess::Piece attacked,
        chess::Square attacker_sq,
        chess::Square attacked_sq
    );

    /** Prepares this accumulator for updates */
    void prepare_updates();

    /** Updates the psq accumulator values using the stored psq updates
     *
     * \param old_acc accumulator to use as base
     * \param weights start of W0_psq
     * \param perspective accumulator perspective
     * \param mirror whether to mirror the board
     */
    void apply_psq_updates(
        const NnueAccumulator& old_acc,
        const i16 weights[N_PSQ][L1_SIZE],
        chess::Color perspective,
        bool mirror
    );

    /** Updates the ti accumulator values using the stored ti updates
     *
     * \param old_acc accumulator to use as base
     * \param weights start of W0_ti
     * \param perspective accumulator perspective
     * \param mirror whether to mirror the board
     */
    void apply_ti_updates(
        const NnueAccumulator& old_acc,
        const i8 weights[N_THREATS][L1_SIZE],
        chess::Color perspective,
        bool mirror
    );

    /** Refreshes the stm psq accumulator by copying the finny entry
     *
     * \param finny_entry finny entry to copy
     * \param perspective psq accumulator perspective
     */
    void refresh_psq(const NnueFinnyEntry& finny_entry, chess::Color perspective);

    /** Refreshes the stm ti accumulator by recomputing from the board state
     *
     * \param weights start of W0_ti
     * \param board new board state
     * \param perspective accumulator perspective
     * \param mirror whether to mirror the board, should match this entry's mirroring
     */
    void refresh_ti(
        const i8 weights[N_THREATS][L1_SIZE],
        const chess::Board& board,
        chess::Color perspective,
        bool mirror
    );


public:
    friend class NnueState;
};
}  // namespace raphael::nnue
#endif