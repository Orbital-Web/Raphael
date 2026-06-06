#pragma once
#ifdef EVAL_NNUE
#include <Raphael/consts.h>
#include <eval/accumulator.h>
#include <eval/geometry.h>



namespace raphael::nnue {
class NnueState {
private:
    NnueFinnyEntry finny_table_[2][2][N_INBUCKETS];  // finny_table[perspective][mirror][bucket]
    NnueAccumulator accumulators_[MAX_DEPTH];        // accumulators[ply][perspective][index]
    i32 idx_ = 0;

    const i16 (*psq_weights_)[N_PSQ][L1_SIZE];
    const i8 (*ti_weights_)[L1_SIZE];


public:
    /** Initializes the NnueState
     *
     * \param W0_psq start of psq W0 array
     * \param W0_ti start of ti W0 array
     * \param b0 start of b0 array
     */
    NnueState(
        const i16 W0_psq[N_INBUCKETS][N_PSQ][L1_SIZE],
        const i8 W0_ti[N_THREATS][L1_SIZE],
        const i16 b0[L1_SIZE]
    );

    /** Lazily updates the accumulator stacks and returns the top accumulator
     *
     * \param current board (should match the top board state)
     * \returns the updated top accumulator
     */
    const NnueAccumulator& get_top_accumulator(const chess::Board& board);

    /** Sets internal states to match the given board
     *
     * \param board the board to set
     */
    void set_board(const chess::Board& board);

    /** Updates internal states to make a move. Call before add_piece, move_king etc. */
    void prepare_make_move();

    /** Updates internal states to unmake the last move */
    void unmake_move();

    /** Adds psq and ti updates for a piece addition to a square
     *
     * \param mailbox board state right before the addition
     * \param piece piece to add
     * \param sq square to add piece to
     */
    void add_piece(
        const std::array<chess::Piece, 64>& mailbox, chess::Piece piece, chess::Square sq
    );

    /** Adds psq and ti updates for a piece removal from a square
     *
     * \param mailbox board state right before the removal
     * \param piece piece to remove
     * \param sq square to remove piece from
     */
    void rem_piece(
        const std::array<chess::Piece, 64>& mailbox, chess::Piece piece, chess::Square sq
    );

    /** Adds psq and ti updates for a piece move
     *
     * \param mailbox board state right before the movement
     * \param from_piece piece to move
     * \param to_piece piece at destination square
     * \param from_sq source square
     * \param to_sq destination square
     */
    void move_piece(
        const std::array<chess::Piece, 64>& mailbox,
        chess::Piece from_piece,
        chess::Piece to_piece,
        chess::Square from_sq,
        chess::Square to_sq
    );

    /** Adds psq and ti updates for a piece mutation
     *
     * \param mailbox board state right before the mutation
     * \param old_piece piece before mutation
     * \param new_piece piece after mutation
     * \param sq square to mutate piece on
     */
    void mutate_piece(
        const std::array<chess::Piece, 64>& mailbox,
        chess::Piece old_piece,
        chess::Piece new_piece,
        chess::Square sq
    );

    /** Sets psq and ti refresh if the king crosses the horizontal or bucket boundary
     *
     * \param color stm of moving king
     * \param from_sq source square of king
     * \param to_sq destination square of king, accounting for castling
     */
    void move_king(chess::Color color, chess::Square from_sq, chess::Square to_sq);

private:
    /** Lazily updates the accumulator stack for one perspective
     *
     * \param board current board
     * \param perspective accumulator perspective
     */
    void lazy_update(const chess::Board& board, chess::Color perspective);

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

    /** Adds ti updates for a piece additional/removal from a square
     *
     * \tparam add whether we are adding or removing a piece
     * \param mailbox board state right before the addition/removal
     * \param piece piece to add/remove
     * \param sq square to add/remove piece from
     */
    template <bool add>
    void update_threats_on_change(
        const std::array<chess::Piece, 64>& mailbox, chess::Piece piece, chess::Square sq
    );

    /** Adds ti updates for adding/removing a piece on a square
     *
     * \tparam add whether we are adding or removing a piece
     * \tparam whether we are pushing outgoing or incoming threats to this square
     * \param indices square indices for each ray bit
     * \param rays piece types in ray space
     * \param br closest pieces attacking/attacked by the piece on the square
     * \param piece piece to add/remove
     * \param sq square to add/remove piece from
     */
    template <bool add, bool outgoing>
    void push_focus_threats(
        geometry::Vector indices,
        geometry::Vector rays,
        geometry::BitRays br,
        chess::Piece piece,
        chess::Square sq
    );

    template <bool add>
    void push_discovered_threats(
        geometry::Vector indices,
        geometry::Vector rays,
        geometry::BitRays sliders,
        geometry::BitRays victims
    );
};
}  // namespace raphael::nnue
#endif