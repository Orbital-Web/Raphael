#include <Raphael/corrhist.h>

#include <algorithm>
#include <cstring>

using namespace raphael;
using std::abs;
using std::clamp;
using std::memory_order_relaxed;
using std::memset;



CorrectionEntry::operator i32() const { return value; }

void CorrectionEntry::update(i32 bonus) {
    assert(bonus >= -CORRHIST_MAX);
    assert(bonus <= CORRHIST_MAX);
    value += bonus - value * abs(bonus) / CORRHIST_MAX;
}


SharedCorrectionEntry::operator i32() const { return value.load(memory_order_relaxed); }

void SharedCorrectionEntry::update(i32 bonus) {
    assert(bonus >= -CORRHIST_MAX);
    assert(bonus <= CORRHIST_MAX);
    auto val = value.load(memory_order_relaxed);
    val += bonus - val * abs(bonus) / CORRHIST_MAX;
    value.store(val, memory_order_relaxed);  // we're not too concerned with lost updates
}



SharedCorrectionHistory::SharedCorrectionHistory()
    : pawn_correction_{}, major_correction_{}, nonpawn_correction_{} {}

void SharedCorrectionHistory::clear() {
    memset(pawn_correction_, 0, sizeof(pawn_correction_));
    memset(major_correction_, 0, sizeof(major_correction_));
    memset(nonpawn_correction_, 0, sizeof(nonpawn_correction_));
}

const SharedCorrectionEntry& SharedCorrectionHistory::pawn_corr_entry(
    const chess::Board& board
) const {
    return pawn_correction_[board.stm()][board.pawn_hash() % CORRHIST_SIZE];
}
SharedCorrectionEntry& SharedCorrectionHistory::pawn_corr_entry(const chess::Board& board) {
    return pawn_correction_[board.stm()][board.pawn_hash() % CORRHIST_SIZE];
}

const SharedCorrectionEntry& SharedCorrectionHistory::major_corr_entry(
    const chess::Board& board
) const {
    return major_correction_[board.stm()][board.major_hash() % CORRHIST_SIZE];
}
SharedCorrectionEntry& SharedCorrectionHistory::major_corr_entry(const chess::Board& board) {
    return major_correction_[board.stm()][board.major_hash() % CORRHIST_SIZE];
}

const SharedCorrectionEntry& SharedCorrectionHistory::nonpawn_corr_entry(
    const chess::Board& board, chess::Color color
) const {
    return nonpawn_correction_[board.stm()][color][board.nonpawn_hash(color) % CORRHIST_SIZE];
}
SharedCorrectionEntry& SharedCorrectionHistory::nonpawn_corr_entry(
    const chess::Board& board, chess::Color color
) {
    return nonpawn_correction_[board.stm()][color][board.nonpawn_hash(color) % CORRHIST_SIZE];
}


CorrectionHistory::CorrectionHistory(): cont_correction_{} {}

void CorrectionHistory::clear() { memset(cont_correction_, 0, sizeof(cont_correction_)); }

const CorrectionEntry& CorrectionHistory::cont_corr_entry(
    chess::Move move, chess::Piece moving, chess::PieceMove prev_move
) const {
    assert(move);
    assert(moving != chess::Piece::NONE);
    assert(prev_move.move);
    assert(prev_move.moving != chess::Piece::NONE);
    return cont_correction_[prev_move.moving][prev_move.move.to()][moving][move.to()];
}
CorrectionEntry& CorrectionHistory::cont_corr_entry(
    chess::Move move, chess::Piece moving, chess::PieceMove prev_move
) {
    assert(move);
    assert(moving != chess::Piece::NONE);
    assert(prev_move.move);
    assert(prev_move.moving != chess::Piece::NONE);
    return cont_correction_[prev_move.moving][prev_move.move.to()][moving][move.to()];
}



namespace raphael::corrhist {
void update(
    const Position<true>& position,
    CorrectionHistory* thread_corrhist,
    SharedCorrectionHistory* shared_corrhist,
    i32 fdepth,
    i32 score,
    i32 static_eval
) {
    const auto bonus = clamp(
        (score - static_eval) * fdepth / (DEPTH_SCALE * CORRHIST_BONUS_DEPTH_DIV),
        -CORRHIST_BONUS_MAX,
        CORRHIST_BONUS_MAX
    );

    const auto& board = position.board();
    const auto curr = position.prev_move(1);
    const auto prev1 = position.prev_move(2);
    const auto prev2 = position.prev_move(3);

    shared_corrhist->pawn_corr_entry(board).update(bonus);
    shared_corrhist->major_corr_entry(board).update(bonus);
    shared_corrhist->nonpawn_corr_entry(board, chess::Color::WHITE).update(bonus);
    shared_corrhist->nonpawn_corr_entry(board, chess::Color::BLACK).update(bonus);
    if (curr.moving != chess::Piece::NONE && prev1.moving != chess::Piece::NONE)
        thread_corrhist->cont_corr_entry(curr.move, curr.moving, prev1).update(bonus);
    if (curr.moving != chess::Piece::NONE && prev2.moving != chess::Piece::NONE)
        thread_corrhist->cont_corr_entry(curr.move, curr.moving, prev2).update(bonus);
}

i32 get(
    const Position<true>& position,
    const CorrectionHistory* thread_corrhist,
    const SharedCorrectionHistory* shared_corrhist
) {
    const auto& board = position.board();
    const auto curr = position.prev_move(1);
    const auto prev1 = position.prev_move(2);
    const auto prev2 = position.prev_move(3);

    i32 correction = 0;
    correction += shared_corrhist->pawn_corr_entry(board) * PAWN_CORRHIST_WEIGHT;
    correction += shared_corrhist->major_corr_entry(board) * MAJOR_CORRHIST_WEIGHT;
    correction += shared_corrhist->nonpawn_corr_entry(board, chess::Color::WHITE)
                  * NONPAWN_CORRHIST_WEIGHT;
    correction += shared_corrhist->nonpawn_corr_entry(board, chess::Color::BLACK)
                  * NONPAWN_CORRHIST_WEIGHT;
    correction += (curr.moving != chess::Piece::NONE && prev1.moving != chess::Piece::NONE)
                      ? thread_corrhist->cont_corr_entry(curr.move, curr.moving, prev1)
                            * CONT1_CORRHIST_WEIGHT
                      : 0;
    correction += (curr.moving != chess::Piece::NONE && prev2.moving != chess::Piece::NONE)
                      ? thread_corrhist->cont_corr_entry(curr.move, curr.moving, prev2)
                            * CONT2_CORRHIST_WEIGHT
                      : 0;
    correction /= CORRHIST_MAX;

    return correction;
}
}  // namespace raphael::corrhist
