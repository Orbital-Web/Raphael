#include <Raphael/History.h>
#include <Raphael/tunable.h>

#include <cstring>

using namespace Raphael;
using std::memset;
using std::min;



History::History(): _history{0} {}


void History::update(
    const chess::Move& bestmove, const chess::Movelist& quietlist, int depth, int side
) {
    const int bonus
        = min(HISTORY_BONUS_SCALE * depth + HISTORY_BONUS_OFFSET, (int)HISTORY_BONUS_MAX);

    for (const auto& move : quietlist) {
        const int from = move.from().index();
        const int to = move.to().index();

        _history[side][from][to] -= _history[side][from][to] * bonus / HISTORY_MAX;
        _history[side][from][to] += (move == bestmove) ? bonus : -bonus;
    }
}


int History::get(const chess::Move& move, int side) const {
    const int from = move.from().index();
    const int to = move.to().index();
    return _history[side][from][to];
}


void History::clear() { memset(_history, 0, sizeof(_history)); }
