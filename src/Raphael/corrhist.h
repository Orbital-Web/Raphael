#pragma once
#include <Raphael/position.h>

#include <atomic>



namespace raphael {
struct CorrectionEntry {
    i16 value = 0;

    operator i32() const;

    /** Updates the corrhist entry with gravity
     *
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update(i32 bonus);
};

struct SharedCorrectionEntry {
    std::atomic<i16> value = 0;

    operator i32() const;

    /** Updates the shared corrhist entry with gravity
     *
     * \param bonus bonus to apply, negative to apply penalty
     */
    void update(i32 bonus);
};



class SharedCorrectionHistory {
private:
    SharedCorrectionEntry pawn_correction_[2][CORRHIST_SIZE];        // [stm][pawn_hash]
    SharedCorrectionEntry major_correction_[2][CORRHIST_SIZE];       // [stm][major_hash]
    SharedCorrectionEntry nonpawn_correction_[2][2][CORRHIST_SIZE];  // [stm][color][nonpawn_hash]


public:
    /** Initializes the shared correction history tables with zeros */
    SharedCorrectionHistory();

    /** Zeros out all the histories */
    void clear();


    /** Returns a reference to the pawn corrhist entry
     *
     * \param board current board
     * \returns pawn corrhist entry
     */
    const SharedCorrectionEntry& pawn_corr_entry(const chess::Board& board) const;
    SharedCorrectionEntry& pawn_corr_entry(const chess::Board& board);

    /** Returns a reference to the major corrhist entry
     *
     * \param board current board
     * \returns major corrhist entry
     */
    const SharedCorrectionEntry& major_corr_entry(const chess::Board& board) const;
    SharedCorrectionEntry& major_corr_entry(const chess::Board& board);

    /** Returns a reference to the non-pawn corrhist entry
     *
     * \param board current board
     * \param color color of non-pawns
     * \returns non-pawn corrhist entry
     */
    const SharedCorrectionEntry& nonpawn_corr_entry(
        const chess::Board& board, chess::Color color
    ) const;
    SharedCorrectionEntry& nonpawn_corr_entry(const chess::Board& board, chess::Color color);
};

class CorrectionHistory {
private:
    CorrectionEntry cont_correction_[12][64][12][64];  // [prev from][prev to][from][to]

public:
    /** Initializes the correction history tables with zeros */
    CorrectionHistory();

    /** Zeros out all the histories */
    void clear();


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



namespace corrhist {
/** Updates the correction histories
 *
 * \param position current position
 * \param thread_corrhist this thread's correction history
 * \param shared_corrhist this NUMA node's correction history
 * \param fdepth current fractional depth
 * \param score current score
 * \param static_eval current static eval
 */
void update(
    const Position<true>& position,
    CorrectionHistory* thread_corrhist,
    SharedCorrectionHistory* shared_corrhist,
    i32 fdepth,
    i32 score,
    i32 static_eval
);

/** Returns the correction term
 *
 * \param position current position
 * \param thread_corrhist this thread's correction history
 * \param shared_corrhist this NUMA node's correction history
 * \returns the correction term
 */
i32 get(
    const Position<true>& position,
    const CorrectionHistory* thread_corrhist,
    const SharedCorrectionHistory* shared_corrhist
);
}  // namespace corrhist
}  // namespace raphael
