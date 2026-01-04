#include <Raphael/Transposition.h>

#include <cstring>

using namespace Raphael;



TranspositionTable::TranspositionTable(uint32_t size_mb) { resize(size_mb); }


void TranspositionTable::resize(uint32_t size_mb) {
    size = (uint64_t)size_mb * 1024 * 1024 / ENTRY_SIZE;
    assert((size > 0 && size <= MAX_TABLE_SIZE));  // size is within (0, MAX_TABLE_SIZE]

    _table.clear();
    _table.resize(size);
}


TranspositionTable::Entry TranspositionTable::get(uint64_t key, int ply) const {
    // get
    const EntryStorage& entryst = _table[index(key)];

    // unpack value to get entry (63-32: eval, 31-16: move, 15-14: flag, 13-0: depth)
    Entry entry = {
        .key = entryst.key,
        .depth = int(entryst.val & 0x3FFF),
        .flag = Flag((entryst.val >> 14) & 0b11),
        .move = chess::Move(uint16_t((entryst.val >> 16) & 0xFFFF)),
        .eval = int32_t(uint32_t(entryst.val >> 32)),
    };

    // correct mate eval when retrieving (https://youtu.be/XfeuxubYlT0)
    if (entry.eval < -MATE_EVAL + 1000)
        entry.eval += ply;
    else if (entry.eval > MATE_EVAL - 1000)
        entry.eval -= ply;
    return entry;
}


void TranspositionTable::set(const Entry& entry, int ply) {
    // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
    int eval = entry.eval;
    if (eval < -MATE_EVAL + 1000)
        eval -= ply;
    else if (eval > MATE_EVAL - 1000)
        eval += ply;

    // pack value to get entry storage (63-32: eval, 31-16: move, 15-14: flag, 13-0: depth)
    uint64_t val = 0;
    val |= (entry.depth & 0x3FFF);
    val |= (uint64_t(entry.flag) << 14);
    val |= (uint64_t(entry.move.move()) << 16);
    val |= (uint64_t(uint32_t(eval)) << 32);  // eval may be negative

    // set
    _table[index(entry.key)] = {.key = entry.key, .val = val};
}


void TranspositionTable::clear() { memset(_table.data(), 0, size * ENTRY_SIZE); }


bool TranspositionTable::valid(const Entry& entry, uint64_t key, int depth) {
    return ((entry.flag != INVALID) && (entry.depth >= depth) && (entry.key == key));
}


bool TranspositionTable::is_mate(int eval) {
    int abseval = abs(eval);
    return ((abseval <= MATE_EVAL) && (abseval > MATE_EVAL - 1000));
}


uint64_t TranspositionTable::index(uint64_t key) const { return key % size; }
