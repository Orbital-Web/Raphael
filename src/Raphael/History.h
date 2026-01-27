#pragma once
#include <chess/include.h>



namespace raphael {
struct HistoryEntry {
    i16 value = 0;

    operator i32() const;

    /** Updates the entry with gravity
     *
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update(i32 bonus);
};



class History {  // based on https://www.chessprogramming.org/History_Heuristic
private:
    HistoryEntry butterfly_hist_[2][64][64];  // [side][from][to]
    HistoryEntry capt_hist_[64][64][13];      // [from][to][piece, 12 for non-capture queening]

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
     * \param color current side to move
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update_quiet(chess::Move move, chess::Color color, i32 bonus);

    /** Applies a bonus to the noisy history score
     *
     * \param move noisy move
     * \param captured the captured piece
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update_noisy(chess::Move move, chess::Piece captured, i32 bonus);


    /** Returns the quiet history score
     *
     * \param move quiet move
     * \param color current side to move
     * \returns quiet history score
     */
    i32 get_quietscore(chess::Move move, chess::Color color) const;

    /** Returns the noisy history score
     *
     * \param move noisy move
     * \param captured the captured piece
     * \returns noisy history score
     */
    i32 get_noisyscore(chess::Move move, chess::Piece captured) const;


    /** Zeros out all the histories */
    void clear();

private:
    /** Returns a reference to the butterfly history entry
     *
     * \param move move to get history for
     * \param color current side to move
     * \returns butterfly history entry
     */
    const HistoryEntry& butterfly_entry(chess::Move move, chess::Color color) const;
    HistoryEntry& butterfly_entry(chess::Move move, chess::Color color);

    /** Returns a reference to the capture history entry
     *
     * \param move move to get history for
     * \param captured the captured piece
     * \returns capture history entry
     */
    const HistoryEntry& capt_entry(chess::Move move, chess::Piece captured) const;
    HistoryEntry& capt_entry(chess::Move move, chess::Piece captured);
};
}  // namespace raphael
