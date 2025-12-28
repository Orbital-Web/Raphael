#include <Raphael/History.h>

#include <cstring>

using namespace Raphael;
using std::memset;
using std::min;



History::History(): _history{0} {}


void History::update(
    const chess::Move bestmove, const chess::Movelist& quietlist, const int depth, const int side
) {
    int bonus = min(HISTORY_BONUS_SCALE * depth + HISTORY_BONUS_OFFSET, HISTORY_BONUS_MAX);

    for (const auto& move : quietlist) {
        int from = (int)move.from();
        int to = (int)move.to();

        _history[side][from][to] -= _history[side][from][to] * bonus / HISTORY_MAX;
        _history[side][from][to] += (move == bestmove) ? bonus : -bonus;
    }
}


int History::get(const chess::Move move, const int side) const {
    int from = (int)move.from();
    int to = (int)move.to();
    return _history[side][from][to];
}


void History::clear() { memset(_history, 0, sizeof(_history)); }
