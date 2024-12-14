#include <Raphael/Transposition.h>

using namespace Raphael;
using std::fill;



TranspositionTable::TranspositionTable(const uint32_t size_in)
    : size(size_in), _table(size, {.flag = INVALID, .move = chess::Move::NO_MOVE}) {
    assert((size > 0 && size <= MAX_TABLE_SIZE));  // size is within (0, MAX_TABLE_SIZE]
    // assert(((size & (size-1)) == 0));           // size is a power of 2
}


TranspositionTable::Entry TranspositionTable::get(const uint64_t key, const int ply) const {
    // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
    Entry entry = _table[index(key)];
    if (entry.eval < -MATE_EVAL + 1000)
        entry.eval += ply;
    else if (entry.eval > MATE_EVAL - 1000)
        entry.eval -= ply;
    return entry;
}


void TranspositionTable::set(const Entry entry, const int ply) {
    // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
    auto key = index(entry.key);
    _table[key] = entry;
    if (entry.eval < -MATE_EVAL + 1000)
        _table[key].eval -= ply;
    else if (entry.eval > MATE_EVAL - 1000)
        _table[key].eval += ply;
}


void TranspositionTable::clear() {
    fill(_table.begin(), _table.end(), (Entry){.flag = INVALID, .move = chess::Move::NO_MOVE});
}


bool TranspositionTable::valid(const Entry entry, const uint64_t key, const int depth) {
    return ((entry.flag != INVALID) && (entry.depth >= depth) && (entry.key == key));
}


bool TranspositionTable::isMate(const int eval) {
    int abseval = abs(eval);
    return ((abseval <= MATE_EVAL) && (abseval > MATE_EVAL - 1000));
}


uint64_t TranspositionTable::index(const uint64_t key) const { return key % size; }
