#include <Raphael/History.h>
#include <Raphael/tunable.h>
#include <Raphael/utils.h>

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



History::History(): butterfly_hist_{}, capt_hist_{} {}


i32 History::quiet_bonus(i32 depth) const {
    return min<i32>(HISTORY_BONUS_DEPTH_SCALE * depth + HISTORY_BONUS_OFFSET, HISTORY_BONUS_MAX);
}

i32 History::noisy_bonus(i32 depth) const {
    return min<i32>(CAPTHIST_BONUS_DEPTH_SCALE * depth + CAPTHIST_BONUS_OFFSET, CAPTHIST_BONUS_MAX);
}

i32 History::quiet_penalty(i32 depth) const {
    return -min<i32>(
        HISTORY_PENALTY_DEPTH_SCALE * depth + HISTORY_PENALTY_OFFSET, HISTORY_PENALTY_MAX
    );
}

i32 History::noisy_penalty(i32 depth) const {
    return -min<i32>(
        CAPTHIST_PENALTY_DEPTH_SCALE * depth + CAPTHIST_PENALTY_OFFSET, CAPTHIST_PENALTY_MAX
    );
}


void History::update_quiet(chess::Move move, chess::Color color, i32 bonus) {
    butterfly_entry(move, color).update(bonus);
}

void History::update_noisy(chess::Move move, chess::Piece captured, i32 bonus) {
    capt_entry(move, captured).update(bonus);
}


i32 History::get_quietscore(chess::Move move, chess::Color color) const {
    i32 score = 0;
    score += butterfly_entry(move, color);
    return score;
}

i32 History::get_noisyscore(chess::Move move, chess::Piece captured) const {
    return capt_entry(move, captured);
}


void History::clear() {
    memset(butterfly_hist_, 0, sizeof(butterfly_hist_));
    memset(capt_hist_, 0, sizeof(capt_hist_));
}



const HistoryEntry& History::butterfly_entry(chess::Move move, chess::Color color) const {
    return butterfly_hist_[color][move.from()][move.to()];
}
HistoryEntry& History::butterfly_entry(chess::Move move, chess::Color color) {
    return butterfly_hist_[color][move.from()][move.to()];
}

const HistoryEntry& History::capt_entry(chess::Move move, chess::Piece captured) const {
    return capt_hist_[move.from()][move.to()][captured];
}
HistoryEntry& History::capt_entry(chess::Move move, chess::Piece captured) {
    return capt_hist_[move.from()][move.to()][captured];
}
