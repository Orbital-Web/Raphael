#include <Raphael/History.h>

#include <cstring>

using namespace Raphael;
using std::memset;



History::History(): _history{0} {}


void History::update(const chess::Move move, const int depth, const int side) {
    int from = (int)move.from();
    int to = (int)move.to();
    _history[side][from][to] += depth * depth;
}


int History::get(const chess::Move move, const int side) const {
    int from = (int)move.from();
    int to = (int)move.to();
    return _history[side][from][to];
}


void History::clear() { std::memset(_history, 0, sizeof(_history)); }
