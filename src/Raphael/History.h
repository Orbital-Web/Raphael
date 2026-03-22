#pragma once
#include <Raphael/position.h>
#include <Raphael/tunable.h>



namespace raphael {
struct HistoryEntry {
    i16 value = 0;

    operator i32() const;

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

struct CorrectionEntry {
    i16 value = 0;

    operator i32() const;

    /** Updates the corrhist entry with gravity
     *
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update(i32 bonus);
};



class History {
private:
    HistoryEntry butterfly_hist_[64][64][2][2];  // [from][to][from attacked][to attacked]
    HistoryEntry cont_hist_[12][64][12][64];     // [prev from][prev to][from][to]
    HistoryEntry capt_hist_[64][64][13];         // [from][to][piece, 12 for non-capture queening]

    CorrectionEntry pawn_correction_[2][CORRHIST_SIZE];  // [stm][pawn_hash % CORRHIST_SIZE]

public:
    /** Initializes all the history tables with zeros */
    History();


    /** Computes the quiet history bonus score at a given depth
     *
     * \param depth current depth
     * \returns the bonus score
     */
    i32 quiet_bonus(i32 depth) const;

    /** Computes the noisy history bonus score at a given depth
     *
     * \param depth current depth
     * \returns the bonus score
     */
    i32 noisy_bonus(i32 depth) const;

    /** Computes the quiet history penalty score at a given depth
     *
     * \param depth current depth
     * \returns the penalty score (already negative)
     */
    i32 quiet_penalty(i32 depth) const;

    /** Computes the noisy history penalty score at a given depth
     *
     * \param depth current depth
     * \returns the penalty score (already negative)
     */
    i32 noisy_penalty(i32 depth) const;


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
     * \param captured the captured piece
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update_noisy(chess::Move move, chess::Piece captured, i32 bonus);


    /** Returns the continuation history score
     *
     * \param move quiet move
     * \param position current position
     * \returns continuation history score
     */
    i32 get_conthist(chess::Move move, const Position<true>& position) const;

    /** Returns the quiet history score
     *
     * \param move quiet move
     * \param position current position
     * \returns quiet history score
     */
    i32 get_quietscore(chess::Move move, const Position<true>& position) const;

    /** Returns the noisy history score
     *
     * \param move noisy move
     * \param captured the captured piece
     * \returns noisy history score
     */
    i32 get_noisyscore(chess::Move move, chess::Piece captured) const;


    /** Updates the correction histories
     *
     * \param board current board
     * \param depth current depth
     * \param score current score
     * \param static_eval current static eval
     */
    void update_corrections(const chess::Board& board, i32 depth, i32 score, i32 static_eval);

    /** Returns the corrected score
     *
     * \param board current board
     * \param score score to correct
     * \returns the corrected score
     */
    i32 correct(const chess::Board& board, i32 score) const;


    /** Zeros out all the histories */
    void clear();

private:
    /** Returns a reference to the butterfly history entry
     *
     * \param move move to get history for
     * \param threats squares attacked by the current not side to move
     * \returns butterfly history entry
     */
    const HistoryEntry& butterfly_entry(chess::Move move, chess::BitBoard threats) const;
    HistoryEntry& butterfly_entry(chess::Move move, chess::BitBoard threats);

    /** Returns a reference to the continuation history entry
     *
     * \param move current move
     * \param moving current moving piece
     * \param prev_move previous move and moving piece
     * \returns continuation history entry
     */
    const HistoryEntry& cont_entry(
        chess::Move move, chess::Piece moving, chess::PieceMove prev_move
    ) const;
    HistoryEntry& cont_entry(chess::Move move, chess::Piece moving, chess::PieceMove prev_move);

    /** Returns a reference to the capture history entry
     *
     * \param move move to get history for
     * \param captured the captured piece
     * \returns capture history entry
     */
    const HistoryEntry& capt_entry(chess::Move move, chess::Piece captured) const;
    HistoryEntry& capt_entry(chess::Move move, chess::Piece captured);

    /** Returns a reference to the pawn corrhist entry
     *
     * \param board current board
     * \returns pawn corrhist entry
     */
    const CorrectionEntry& pawn_corr_entry(const chess::Board& board) const;
    CorrectionEntry& pawn_corr_entry(const chess::Board& board);
};
}  // namespace raphael
