#include <Raphael/History.h>

#include <cstring>

using namespace Raphael;
using std::memset;
using std::min;



History::History(
    const int bonus_scale, const int bonus_offset, const int bonus_max, const int history_max
)
    : bonus_scale(bonus_scale),
      bonus_offset(bonus_offset),
      bonus_max(bonus_max),
      history_max(history_max),
      _history{0} {}


void History::update(
    const chess::Move bestmove, const chess::Movelist& quietlist, const int depth, const int side
) {
    int bonus = min(bonus_scale * depth + bonus_offset, bonus_max);

    for (const auto& move : quietlist) {
        int from = (int)move.from();
        int to = (int)move.to();

        _history[side][from][to] -= _history[side][from][to] * bonus / history_max;
        _history[side][from][to] += (move == bestmove) ? bonus : -bonus;
    }
}


int History::get(const chess::Move move, const int side) const {
    int from = (int)move.from();
    int to = (int)move.to();
    return _history[side][from][to];
}


void History::clear() { memset(_history, 0, sizeof(_history)); }
