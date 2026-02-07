#include <Raphael/Transposition.h>
#include <Raphael/utils.h>

#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

using namespace raphael;
using std::memset;



TranspositionTable::TranspositionTable(i32 size_mb): capacity_(0), table_(nullptr) {
    resize(size_mb);
}


TranspositionTable::~TranspositionTable() { deallocate(); }


void TranspositionTable::resize(i32 size_mb) {
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


bool TranspositionTable::get(ProbedEntry& ttentry, u64 key, i32 ply) const {
    // get
    const auto& entry = table_[index(key)];
    const auto packed_key = static_cast<u16>(key);

    if (packed_key == entry.key) {
        // correct mate score when retrieving (https://youtu.be/XfeuxubYlT0)
        i32 score = static_cast<i32>(entry.score);
        if (utils::is_loss(score))
            score += ply;
        else if (utils::is_win(score))
            score -= ply;
        ttentry.score = score;
        ttentry.move = static_cast<chess::Move>(entry.move);
        ttentry.depth = static_cast<i32>(entry.depth);
        ttentry.flag = entry.flag;

        return true;
    }

    return false;
}


void TranspositionTable::prefetch(u64 key) const { __builtin_prefetch(&table_[index(key)]); }


void TranspositionTable::set(u64 key, i32 score, chess::Move move, i32 depth, Flag flag, i32 ply) {
    assert(depth >= 0);
    assert(depth < 256);

    Entry entry = table_[index(key)];
    const auto packed_key = static_cast<u16>(key);

    if (move || entry.key != packed_key) entry.move = static_cast<u16>(move);

    // correct mate score when storing (https://youtu.be/XfeuxubYlT0)
    if (utils::is_loss(score))
        score -= ply;
    else if (utils::is_win(score))
        score += ply;

    assert(score >= INT16_MIN);
    assert(score <= INT16_MAX);
    assert(depth > 0);
    assert(depth < 256);

    // set
    entry.key = packed_key;
    entry.score = static_cast<i16>(score);
    entry.depth = static_cast<u8>(depth);
    entry.flag = flag;

    table_[index(key)] = entry;
}


void TranspositionTable::clear() {
    assert(table_ != nullptr);
    assert(size_ > 0);

    memset(table_, 0, size_ * ENTRY_SIZE);
}


u64 TranspositionTable::index(u64 key) const {
    // key >> 64 = 0~1, index at this fraction of the way through size_
    return static_cast<u64>((static_cast<u128>(key) * static_cast<u128>(size_)) >> 64);
}


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
