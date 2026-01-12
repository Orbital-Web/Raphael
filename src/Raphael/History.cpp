#include <Raphael/History.h>
#include <Raphael/tunable.h>

#include <cstring>

using namespace raphael;
using std::memset;
using std::min;



History::History(): _history{0} {}


void History::update(
    const chess::Move& bestmove, const chess::Movelist& quietlist, i32 depth, i32 side
) {
    const i32 bonus
        = min<i32>(HISTORY_BONUS_SCALE * depth + HISTORY_BONUS_OFFSET, HISTORY_BONUS_MAX);

    for (const auto& move : quietlist) {
        const i32 from = move.from().index();
        const i32 to = move.to().index();

        _history[side][from][to] -= _history[side][from][to] * bonus / HISTORY_MAX;
        _history[side][from][to] += (move == bestmove) ? bonus : -bonus;
    }
}


i32 History::get(const chess::Move& move, bool side) const {
    const i32 from = move.from().index();
    const i32 to = move.to().index();
    return _history[side][from][to];
}


void History::clear() { memset(_history, 0, sizeof(_history)); }
