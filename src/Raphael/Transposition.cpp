#include <Raphael/Transposition.h>
#include <Raphael/utils.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

using namespace raphael;
using std::fill;



TranspositionTable::TranspositionTable(u32 size_mb): capacity_(0), table_(nullptr) {
    resize(size_mb);
}


TranspositionTable::~TranspositionTable() { deallocate(); }


void TranspositionTable::resize(u32 size_mb) {
    const usize newsize = (usize)size_mb * 1024 * 1024 / ENTRY_SIZE;
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
    Entry entry = table_[index(key)];

    // correct mate score when retrieving (https://youtu.be/XfeuxubYlT0)
    if (utils::is_loss(entry.score))
        entry.score += ply;
    else if (utils::is_win(entry.score))
        entry.score -= ply;
    return entry;
}


void TranspositionTable::prefetch(u64 key) const { __builtin_prefetch(&table_[index(key)]); }


void TranspositionTable::set(u64 key, i32 score, chess::Move move, i32 depth, Flag flag, i32 ply) {
    assert(depth >= 0);
    assert(depth < 256);

    Entry entry = table_[index(key)];

    if (move != chess::Move::NO_MOVE || entry.key != key) entry.move = move;

    // correct mate score when storing (https://youtu.be/XfeuxubYlT0)
    if (utils::is_loss(score))
        score -= ply;
    else if (utils::is_win(score))
        score += ply;

    // set
    entry.key = key;
    entry.score = score;
    entry.depth = static_cast<u8>(depth);
    entry.flag = flag;

    table_[index(key)] = entry;
}


void TranspositionTable::clear() {
    assert(table_ != nullptr);
    assert(size_ > 0);

    fill(table_, table_ + size_, Entry{});
}


u64 TranspositionTable::index(u64 key) const { return key % size_; }


void TranspositionTable::allocate(usize newsize) {
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
    table_ = static_cast<Entry*>(_aligned_malloc(newsize_s, page_size));
#else
    table_ = static_cast<Entry*>(aligned_alloc(page_size, newsize_s));
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
