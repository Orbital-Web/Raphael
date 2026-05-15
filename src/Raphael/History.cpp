#include <Raphael/History.h>
#include <Raphael/utils.h>

#include <algorithm>
#include <cstring>

using namespace raphael;
using std::clamp;
using std::memset;
using std::min;



HistoryEntry::operator i32() const { return value; }

void HistoryEntry::update(i32 bonus) {
    assert(bonus >= -HISTORY_MAX);
    assert(bonus <= HISTORY_MAX);
    value += bonus - value * abs(bonus) / HISTORY_MAX;
}

void HistoryEntry::update_with_base(i32 bonus, i32 base) {
    assert(bonus >= -HISTORY_MAX);
    assert(bonus <= HISTORY_MAX);
    value += bonus - base * abs(bonus) / HISTORY_MAX;
}


CorrectionEntry::operator i32() const { return value; }

void CorrectionEntry::update(i32 bonus) {
    assert(bonus >= -CORRHIST_MAX);
    assert(bonus <= CORRHIST_MAX);
    value += bonus - value * abs(bonus) / CORRHIST_MAX;
}



History::History()
    : butterfly_hist_{},
      cont_hist_{},
      capt_hist_{},
      pawn_correction_{},
      major_correction_{},
      nonpawn_correction_{},
      cont_correction_{} {}


i32 History::bonus(i32 fdepth, i32 depth_mul, i32 base_bonus, i32 max_bonus) const {
    return min(depth_mul * fdepth / DEPTH_SCALE + base_bonus, max_bonus);
}


void History::update_quiet(chess::Move move, const Position<true>& position, i32 bonus) {
    const auto& board = position.board();
    const auto threats = board.threats();
    const auto moving = board.at(move.from());
    const auto prev1 = position.prev_move(1);
    const auto prev2 = position.prev_move(2);
    const auto prev4 = position.prev_move(4);

    const auto total_conthist = get_conthist(move, position);

    butterfly_entry(move, threats).update(bonus);
    if (prev1.moving != chess::Piece::NONE)
        cont_entry(move, moving, prev1).update_with_base(bonus, total_conthist);
    if (prev2.moving != chess::Piece::NONE)
        cont_entry(move, moving, prev2).update_with_base(bonus, total_conthist);
    if (prev4.moving != chess::Piece::NONE)
        cont_entry(move, moving, prev4).update_with_base(bonus, total_conthist);
}

void History::update_noisy(chess::Move move, chess::Piece captured, i32 bonus) {
    capt_entry(move, captured).update(bonus);
}


i32 History::get_conthist(chess::Move move, const Position<true>& position) const {
    const auto& board = position.board();
    const auto moving = board.at(move.from());
    const auto prev1 = position.prev_move(1);
    const auto prev2 = position.prev_move(2);
    const auto prev4 = position.prev_move(4);

    i32 score = 0;
    score += (prev1.moving != chess::Piece::NONE)
                 ? cont_entry(move, moving, prev1) * CONTHIST1_WEIGHT / 128
                 : 0;
    score += (prev2.moving != chess::Piece::NONE)
                 ? cont_entry(move, moving, prev2) * CONTHIST2_WEIGHT / 128
                 : 0;
    score += (prev4.moving != chess::Piece::NONE)
                 ? cont_entry(move, moving, prev4) * CONTHIST4_WEIGHT / 128
                 : 0;
    return score;
}

i32 History::get_quietscore(chess::Move move, const Position<true>& position) const {
    const auto& board = position.board();
    const auto threats = board.threats();

    i32 score = 0;
    score += butterfly_entry(move, threats);
    score += get_conthist(move, position);
    return score;
}

i32 History::get_noisyscore(chess::Move move, chess::Piece captured) const {
    return capt_entry(move, captured);
}


void History::update_corrections(
    const Position<true>& position, i32 fdepth, i32 score, i32 static_eval
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

    pawn_corr_entry(board).update(bonus);
    major_corr_entry(board).update(bonus);
    nonpawn_corr_entry(board, chess::Color::WHITE).update(bonus);
    nonpawn_corr_entry(board, chess::Color::BLACK).update(bonus);
    if (curr.moving != chess::Piece::NONE && prev1.moving != chess::Piece::NONE)
        cont_corr_entry(curr.move, curr.moving, prev1).update(bonus);
    if (curr.moving != chess::Piece::NONE && prev2.moving != chess::Piece::NONE)
        cont_corr_entry(curr.move, curr.moving, prev2).update(bonus);
}

i32 History::get_correction(const Position<true>& position) const {
    const auto& board = position.board();
    const auto curr = position.prev_move(1);
    const auto prev1 = position.prev_move(2);
    const auto prev2 = position.prev_move(3);

    i32 correction = 0;
    correction += pawn_corr_entry(board) * PAWN_CORRHIST_WEIGHT;
    correction += major_corr_entry(board) * MAJOR_CORRHIST_WEIGHT;
    correction += nonpawn_corr_entry(board, chess::Color::WHITE) * NONPAWN_CORRHIST_WEIGHT;
    correction += nonpawn_corr_entry(board, chess::Color::BLACK) * NONPAWN_CORRHIST_WEIGHT;
    correction += (curr.moving != chess::Piece::NONE && prev1.moving != chess::Piece::NONE)
                      ? cont_corr_entry(curr.move, curr.moving, prev1) * CONT1_CORRHIST_WEIGHT
                      : 0;
    correction += (curr.moving != chess::Piece::NONE && prev2.moving != chess::Piece::NONE)
                      ? cont_corr_entry(curr.move, curr.moving, prev2) * CONT2_CORRHIST_WEIGHT
                      : 0;
    correction /= CORRHIST_MAX;

    return correction;
}


void History::clear() {
    memset(butterfly_hist_, 0, sizeof(butterfly_hist_));
    memset(cont_hist_, 0, sizeof(cont_hist_));
    memset(capt_hist_, 0, sizeof(capt_hist_));
    memset(pawn_correction_, 0, sizeof(pawn_correction_));
    memset(major_correction_, 0, sizeof(major_correction_));
    memset(nonpawn_correction_, 0, sizeof(nonpawn_correction_));
    memset(cont_correction_, 0, sizeof(cont_correction_));
}



const HistoryEntry& History::butterfly_entry(chess::Move move, chess::BitBoard threats) const {
    const auto from_attacked = threats.is_set(move.from());
    const auto to_attacked = threats.is_set(move.to());
    return butterfly_hist_[move.from()][move.to()][from_attacked][to_attacked];
}
HistoryEntry& History::butterfly_entry(chess::Move move, chess::BitBoard threats) {
    const auto from_attacked = threats.is_set(move.from());
    const auto to_attacked = threats.is_set(move.to());
    return butterfly_hist_[move.from()][move.to()][from_attacked][to_attacked];
}

const HistoryEntry& History::cont_entry(
    chess::Move move, chess::Piece moving, chess::PieceMove prev_move
) const {
    assert(move);
    assert(moving != chess::Piece::NONE);
    assert(prev_move.move);
    assert(prev_move.moving != chess::Piece::NONE);
    return cont_hist_[prev_move.moving][prev_move.move.to()][moving][move.to()];
}
HistoryEntry& History::cont_entry(
    chess::Move move, chess::Piece moving, chess::PieceMove prev_move
) {
    assert(move);
    assert(moving != chess::Piece::NONE);
    assert(prev_move.move);
    assert(prev_move.moving != chess::Piece::NONE);
    return cont_hist_[prev_move.moving][prev_move.move.to()][moving][move.to()];
}

const HistoryEntry& History::capt_entry(chess::Move move, chess::Piece captured) const {
    return capt_hist_[move.from()][move.to()][captured];
}
HistoryEntry& History::capt_entry(chess::Move move, chess::Piece captured) {
    return capt_hist_[move.from()][move.to()][captured];
}

const CorrectionEntry& History::pawn_corr_entry(const chess::Board& board) const {
    return pawn_correction_[board.stm()][board.pawn_hash() % CORRHIST_SIZE];
}
CorrectionEntry& History::pawn_corr_entry(const chess::Board& board) {
    return pawn_correction_[board.stm()][board.pawn_hash() % CORRHIST_SIZE];
}

const CorrectionEntry& History::major_corr_entry(const chess::Board& board) const {
    return major_correction_[board.stm()][board.major_hash() % CORRHIST_SIZE];
}
CorrectionEntry& History::major_corr_entry(const chess::Board& board) {
    return major_correction_[board.stm()][board.major_hash() % CORRHIST_SIZE];
}

const CorrectionEntry& History::nonpawn_corr_entry(
    const chess::Board& board, chess::Color color
) const {
    return nonpawn_correction_[board.stm()][color][board.nonpawn_hash(color) % CORRHIST_SIZE];
}
CorrectionEntry& History::nonpawn_corr_entry(const chess::Board& board, chess::Color color) {
    return nonpawn_correction_[board.stm()][color][board.nonpawn_hash(color) % CORRHIST_SIZE];
}

const CorrectionEntry& History::cont_corr_entry(
    chess::Move move, chess::Piece moving, chess::PieceMove prev_move
) const {
    assert(move);
    assert(moving != chess::Piece::NONE);
    assert(prev_move.move);
    assert(prev_move.moving != chess::Piece::NONE);
    return cont_correction_[prev_move.moving][prev_move.move.to()][moving][move.to()];
}
CorrectionEntry& History::cont_corr_entry(
    chess::Move move, chess::Piece moving, chess::PieceMove prev_move
) {
    assert(move);
    assert(moving != chess::Piece::NONE);
    assert(prev_move.move);
    assert(prev_move.moving != chess::Piece::NONE);
    return cont_correction_[prev_move.moving][prev_move.move.to()][moving][move.to()];
}
