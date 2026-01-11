#include <Raphael/Transposition.h>
#include <Raphael/utils.h>

#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

using namespace Raphael;



TranspositionTable::TranspositionTable(uint32_t size_mb): capacity(0), _table(nullptr) {
    resize(size_mb);
}


TranspositionTable::~TranspositionTable() { deallocate(); }


void TranspositionTable::resize(uint32_t size_mb) {
    const uint64_t newsize = (uint64_t)size_mb * 1024 * 1024 / ENTRY_SIZE;
    assert((newsize > 0 && newsize <= MAX_TABLE_SIZE));

    // re-allocate if necessary
    if (newsize > capacity || newsize <= capacity / 2) {
        deallocate();
        allocate(newsize);
    }
    size = newsize;

    clear();
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
    if (is_loss(entry.eval))
        entry.eval += ply;
    else if (is_win(entry.eval))
        entry.eval -= ply;
    return entry;
}


void TranspositionTable::prefetch(uint64_t key) const { __builtin_prefetch(&_table[index(key)]); }


void TranspositionTable::set(const Entry& entry, int ply) {
    // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
    int eval = entry.eval;
    if (is_loss(eval))
        eval -= ply;
    else if (is_win(eval))
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


void TranspositionTable::clear() {
    assert(_table != nullptr);
    assert(size > 0);

    memset(_table, 0, size * ENTRY_SIZE);
}


uint64_t TranspositionTable::index(uint64_t key) const { return key % size; }


void TranspositionTable::allocate(uint64_t newsize) {
    assert(_table == nullptr);
    assert(capacity == 0);

#ifdef _WIN32
    static constexpr size_t page_size = 4096;
#else
    static constexpr size_t page_size = 2 * 1024 * 1024;
#endif

    const size_t newsize_s = ((newsize * ENTRY_SIZE + page_size - 1) / page_size) * page_size;
    capacity = newsize_s / ENTRY_SIZE;

#ifdef _WIN32
    // TODO: windows huge page support
    _table = static_cast<EntryStorage*>(_aligned_malloc(newsize_s, page_size));
#else
    _table = static_cast<EntryStorage*>(aligned_alloc(page_size, newsize_s));
    madvise(_table, newsize_s, MADV_HUGEPAGE);
#endif
}

void TranspositionTable::deallocate() {
    if (_table) {
        assert(_table != nullptr);
        assert(capacity > 0);

#ifdef _WIN32
        _aligned_free(_table);
#else
        free(_table);
#endif

        capacity = 0;
        _table = nullptr;
    }
}
