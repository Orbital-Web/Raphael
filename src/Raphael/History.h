#pragma once
#include <Raphael/position.h>



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
    HistoryEntry pieceto_hist_[12][64][2][2];    // [piece][to][from attacked][to attacked]
    HistoryEntry cont_hist_[12][64][12][64];     // [prev from][prev to][from][to]
    HistoryEntry capt_hist_[64][64][13];         // [from][to][piece, 12 for non-capture queening]

    CorrectionEntry pawn_correction_[2][CORRHIST_SIZE];        // [stm][pawn_hash idx]
    CorrectionEntry major_correction_[2][CORRHIST_SIZE];       // [stm][major_hash idx]
    CorrectionEntry nonpawn_correction_[2][2][CORRHIST_SIZE];  // [stm][color][nonpawn_hash idx]
    CorrectionEntry cont_correction_[12][64][12][64];          // [prev from][prev to][from][to]

public:
    /** Initializes all the history tables with zeros */
    History();


    /** Computes the quiet history bonus score at a given fdepth
     *
     * \param fdepth current fractional depth
     * \returns the bonus score
     */
    i32 quiet_bonus(i32 fdepth) const;

    /** Computes the noisy history bonus score at a given fdepth
     *
     * \param fdepth current fractional depth
     * \returns the bonus score
     */
    i32 noisy_bonus(i32 fdepth) const;

    /** Computes the quiet history penalty score at a given fdepth
     *
     * \param fdepth current fractional depth
     * \returns the penalty score (already negative)
     */
    i32 quiet_penalty(i32 fdepth) const;

    /** Computes the noisy history penalty score at a given fdepth
     *
     * \param fdepth current fractional depth
     * \returns the penalty score (already negative)
     */
    i32 noisy_penalty(i32 fdepth) const;


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
     * \param position current position
     * \param fdepth current fractional depth
     * \param score current score
     * \param static_eval current static eval
     */
    void update_corrections(const Position<true>& position, i32 fdepth, i32 score, i32 static_eval);

    /** Returns the corrected score
     *
     * \param position current position
     * \param score score to correct
     * \returns the corrected score
     */
    i32 correct(const Position<true>& position, i32 score) const;


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

    /** Returns a reference to the pieceto history entry
     *
     * \param move move to get history for
     * \param moving current moving piece
     * \param threats squares attacked by the current not side to move
     * \returns butterfly history entry
     */
    const HistoryEntry& pieceto_entry(
        chess::Move move, chess::Piece moving, chess::BitBoard threats
    ) const;
    HistoryEntry& pieceto_entry(chess::Move move, chess::Piece moving, chess::BitBoard threats);

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

    /** Returns a reference to the major corrhist entry
     *
     * \param board current board
     * \returns major corrhist entry
     */
    const CorrectionEntry& major_corr_entry(const chess::Board& board) const;
    CorrectionEntry& major_corr_entry(const chess::Board& board);

    /** Returns a reference to the non-pawn corrhist entry
     *
     * \param board current board
     * \param color color of non-pawns
     * \returns non-pawn corrhist entry
     */
    const CorrectionEntry& nonpawn_corr_entry(const chess::Board& board, chess::Color color) const;
    CorrectionEntry& nonpawn_corr_entry(const chess::Board& board, chess::Color color);

    /** Returns a reference to the continuation correction history entry
     *
     * \param move current move
     * \param moving current moving piece
     * \param prev_move previous move and moving piece
     * \returns continuation history entry
     */
    const CorrectionEntry& cont_corr_entry(
        chess::Move move, chess::Piece moving, chess::PieceMove prev_move
    ) const;
    CorrectionEntry& cont_corr_entry(
        chess::Move move, chess::Piece moving, chess::PieceMove prev_move
    );
};
}  // namespace raphael
