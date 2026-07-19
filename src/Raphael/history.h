#pragma once
#include <Raphael/position.h>



namespace raphael {
struct HistoryEntry {
    i16 value = 0;

    [[nodiscard]] operator i32() const;

    /** Updates the entry with gravity
     *
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update(i32 bonus);

    /** Updates the entry with gravity from a base
     *
     * \param bonus bonus to apply, negative to apply penalty
     * \param base base value (in normal update, base = current val)
     */
    void update_with_base(i32 bonus, i32 base);
};



class History {
private:
    HistoryEntry butterfly_hist_[64][64][2][2];      // [from][to][from attacked][to attacked]
    HistoryEntry pawn_hist_[PAWNHIST_SIZE][12][64];  // [pawn key][from][to]
    HistoryEntry cont_hist_[12][64][12][64];         // [prev from][prev to][from][to]
    HistoryEntry capt_hist_[64][64][13][2][2];  // [from][to][piece][from attacked][to attacked]

public:
    /** Initializes all the history tables with zeros */
    History();


    /** Computes the history bonus score at a given fdepth
     *
     * \param fdepth current fractional depth
     * \param depth_mul depth multiplier
     * \param base_bonus base bonus
     * \param max_bonus max bonus
     * \returns the history bonus
     */
    [[nodiscard]] i32 bonus(i32 fdepth, i32 depth_mul, i32 base_bonus, i32 max_bonus) const;


    /** Applies a bonus to the quiet history score
     *
     * \param move quiet move
     * \param position current position
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update_quiet(chess::Move move, const Position<true>& position, i32 bonus);

    /** Applies a bonus to the noisy history score
     *
     * \param move noisy move
     * \param position current position
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update_noisy(chess::Move move, const Position<true>& position, i32 bonus);


    /** Returns the continuation history score
     *
     * \param move quiet move
     * \param position current position
     * \returns continuation history score
     */
    [[nodiscard]] i32 get_conthist(chess::Move move, const Position<true>& position) const;

    /** Returns the quiet history score
     *
     * \param move quiet move
     * \param position current position
     * \returns quiet history score
     */
    [[nodiscard]] i32 get_quietscore(chess::Move move, const Position<true>& position) const;

    /** Returns the noisy history score
     *
     * \param move noisy move
     * \param position current position
     * \returns noisy history score
     */
    [[nodiscard]] i32 get_noisyscore(chess::Move move, const Position<true>& position) const;


    /** Zeros out all the histories */
    void clear();

private:
    /** Returns a reference to the butterfly history entry
     *
     * \param move move to get history for
     * \param threats squares attacked by the current not side to move
     * \returns butterfly history entry
     */
    [[nodiscard]] const HistoryEntry& butterfly_entry(
        chess::Move move, chess::BitBoard threats
    ) const;
    [[nodiscard]] HistoryEntry& butterfly_entry(chess::Move move, chess::BitBoard threats);

    /** Returns a reference to the pawn history entry
     *
     * \param pawn_key key for the pawn
     * \param move current move
     * \param moving current moving piece
     * \returns pawn history entry
     */
    [[nodiscard]] const HistoryEntry& pawn_entry(
        u64 pawn_key, chess::Move move, chess::Piece moving
    ) const;
    [[nodiscard]] HistoryEntry& pawn_entry(u64 pawn_key, chess::Move move, chess::Piece moving);

    /** Returns a reference to the continuation history entry
     *
     * \param move current move
     * \param moving current moving piece
     * \param prev_move previous move and moving piece
     * \returns continuation history entry
     */
    [[nodiscard]] const HistoryEntry& cont_entry(
        chess::Move move, chess::Piece moving, chess::PieceMove prev_move
    ) const;
    [[nodiscard]] HistoryEntry& cont_entry(
        chess::Move move, chess::Piece moving, chess::PieceMove prev_move
    );

    /** Returns a reference to the capture history entry
     *
     * \param move move to get history for
     * \param captured the captured piece
     * \param threats squares attacked by the current not side to move
     * \returns capture history entry
     */
    [[nodiscard]] const HistoryEntry& capt_entry(
        chess::Move move, chess::Piece captured, chess::BitBoard threats
    ) const;
    [[nodiscard]] HistoryEntry& capt_entry(
        chess::Move move, chess::Piece captured, chess::BitBoard threats
    );
};
}  // namespace raphael
