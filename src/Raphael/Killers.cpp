#include <Raphael/Killers.h>

using namespace Raphael;
using std::fill;



Killers::Killers(): _killers{chess::Move::NO_MOVE} {}


void Killers::put(const chess::Move move, const int ply) {
    if (move != _killers[2 * ply]) {
        _killers[2 * ply + 1] = _killers[2 * ply];  // override old
        _killers[2 * ply] = move;                   // override current
    }
}


bool Killers::is_killer(const chess::Move move, const int ply) const {
    return (move == _killers[2 * ply] || move == _killers[2 * ply + 1]);
}


void Killers::clear_ply(const int ply) {
    _killers[2 * ply] = chess::Move::NO_MOVE;
    _killers[2 * ply + 1] = chess::Move::NO_MOVE;
}


void Killers::clear() { fill(_killers, _killers + 2 * MAX_DEPTH, chess::Move::NO_MOVE); }
