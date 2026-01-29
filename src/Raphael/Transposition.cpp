#include <Raphael/Transposition.h>
#include <Raphael/utils.h>

#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

using namespace raphael;



TranspositionTable::TranspositionTable(u32 size_mb): capacity_(0), table_(nullptr) {
    resize(size_mb);
}


TranspositionTable::~TranspositionTable() { deallocate(); }


void TranspositionTable::resize(u32 size_mb) {
    const u64 newsize = (u64)size_mb * 1024 * 1024 / ENTRY_SIZE;
    assert((newsize > 0 && newsize <= MAX_TABLE_SIZE));

    // re-allocate if necessary
    if (newsize > capacity_ || newsize <= capacity_ / 2) {
        deallocate();
        allocate(newsize);
    }
    size_ = newsize;

    clear();
}


TranspositionTable::Entry TranspositionTable::get(u64 key, i32 ply) const {
    // get
    const EntryStorage& entryst = table_[index(key)];

    // unpack value to get entry (63-32: score, 31-16: move, 15-14: flag, 13-0: depth)
    Entry entry = {
        .key = entryst.key,
        .depth = i32(entryst.val & 0x3FFF),
        .flag = Flag((entryst.val >> 14) & 0b11),
        .move = chess::Move(u16((entryst.val >> 16) & 0xFFFF)),
        .score = i32(u32(entryst.val >> 32)),
    };

    // correct mate score when retrieving (https://youtu.be/XfeuxubYlT0)
    if (utils::is_loss(entry.score))
        entry.score += ply;
    else if (utils::is_win(entry.score))
        entry.score -= ply;
    return entry;
}


void TranspositionTable::prefetch(u64 key) const { __builtin_prefetch(&table_[index(key)]); }


void TranspositionTable::set(const Entry& entry, i32 ply) {
    // correct mate score when storing (https://youtu.be/XfeuxubYlT0)
    i32 score = entry.score;
    if (utils::is_loss(score))
        score -= ply;
    else if (utils::is_win(score))
        score += ply;

    // pack value to get entry storage (63-32: score, 31-16: move, 15-14: flag, 13-0: depth)
    u64 val = 0;
    val |= (entry.depth & 0x3FFF);
    val |= (u64(entry.flag) << 14);
    val |= (u64(u16(entry.move)) << 16);
    val |= (u64(u32(score)) << 32);  // score may be negative

    // set
    table_[index(entry.key)] = {.key = entry.key, .val = val};
}


void TranspositionTable::clear() {
    assert(table_ != nullptr);
    assert(size_ > 0);

    memset(table_, 0, size_ * ENTRY_SIZE);
}


u64 TranspositionTable::index(u64 key) const { return key % size_; }


void TranspositionTable::allocate(u64 newsize) {
    assert(table_ == nullptr);
    assert(capacity_ == 0);

#ifdef _WIN32
    static constexpr usize page_size = 4096;
#else
    static constexpr usize page_size = 2 * 1024 * 1024;
#endif

    const usize newsize_s = ((newsize * ENTRY_SIZE + page_size - 1) / page_size) * page_size;
    capacity_ = newsize_s / ENTRY_SIZE;

#ifdef _WIN32
    // TODO: windows huge page support
    table_ = static_cast<EntryStorage*>(_aligned_malloc(newsize_s, page_size));
#else
    table_ = static_cast<EntryStorage*>(aligned_alloc(page_size, newsize_s));
    madvise(table_, newsize_s, MADV_HUGEPAGE);
#endif
}

void TranspositionTable::deallocate() {
    if (table_) {
        assert(table_ != nullptr);
        assert(capacity_ > 0);

#ifdef _WIN32
        _aligned_free(table_);
#else
        free(table_);
#endif

        capacity_ = 0;
        table_ = nullptr;
    }
}
