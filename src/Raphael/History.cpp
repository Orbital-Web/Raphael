#include <Raphael/History.h>

#include <cstring>

using namespace Raphael;
using std::memset;
using std::min;



History::History(): _history{0} {}


void History::update(const chess::Move move, const int depth, const int side) {
    int from = (int)move.from();
    int to = (int)move.to();
    int bonus = min(HISTORY_BONUS_SCALE * depth + HISTORY_BONUS_OFFSET, HISTORY_BONUS_MAX);
    _history[side][from][to] += bonus - (_history[side][from][to] * bonus) / HISTORY_MAX;
}


int History::get(const chess::Move move, const int side) const {
    int from = (int)move.from();
    int to = (int)move.to();
    return _history[side][from][to];
}


void History::clear() { memset(_history, 0, sizeof(_history)); }
