#include <Raphael/Transposition.h>
#include <Raphael/tunable.h>
#include <Raphael/utils.h>

#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

using namespace raphael;
using std::memset;



u32 TranspositionTable::Entry::age() const { return static_cast<u32>(age_flag >> 2); }

TranspositionTable::Flag TranspositionTable::Entry::flag() const {
    return static_cast<Flag>(age_flag & 0x3);
}

void TranspositionTable::Entry::set_age_flag(u32 age, Flag flag) {
    assert(age <= MAX_AGE);
    age_flag = static_cast<u8>(age << 2) | static_cast<u8>(flag);
}

i32 TranspositionTable::Entry::value(u32 tt_age) const {
    const i32 relative_age = (MAX_AGE + 1 + tt_age - age()) & MAX_AGE;
    return TT_VALUE_DEPTH_WEIGHT * depth - TT_VALUE_AGE_WEIGHT * relative_age;
}


TranspositionTable::TranspositionTable(i32 size_mb): capacity_(0), table_(nullptr) {
    resize(size_mb);
}

TranspositionTable::~TranspositionTable() { deallocate(); }

void TranspositionTable::resize(i32 size_mb) {
    assert(size_mb > 0 && size_mb <= MAX_TABLE_SIZE_MB);
    const usize newsize = (usize)size_mb * 1024 * 1024 / CLUSTER_SIZE;

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
    const auto& cluster = table_[index(key)];
    const auto packed_key = static_cast<u16>(key);

    for (usize i = 0; i < ENTRIES_PER_CLUSTER; i++) {
        const auto& entry = cluster.entries[i];
        if (packed_key == entry.key && entry.flag() != Flag::INVALID) {
            // correct mate score when retrieving (https://youtu.be/XfeuxubYlT0)
            i32 score = static_cast<i32>(entry.score);
            if (utils::is_loss(score))
                score += ply;
            else if (utils::is_win(score))
                score -= ply;
            ttentry.score = score;
            ttentry.static_eval = static_cast<i32>(entry.static_eval);
            ttentry.move = static_cast<chess::Move>(entry.move);
            ttentry.depth = static_cast<i32>(entry.depth);
            ttentry.flag = entry.flag();

            return true;
        }
    }
    return false;
}

void TranspositionTable::prefetch(u64 key) const { __builtin_prefetch(&table_[index(key)]); }

void TranspositionTable::set(
    u64 key, i32 score, i32 static_eval, chess::Move move, i32 depth, Flag flag, i32 ply
) {
    assert(depth >= 0);
    assert(depth < 256);
    assert(score >= INT16_MIN);
    assert(score <= INT16_MAX);
    assert(static_eval >= INT16_MIN);
    assert(static_eval <= INT16_MAX);

    auto& cluster = table_[index(key)];
    const auto packed_key = static_cast<u16>(key);

    // choose candidate to evict
    Entry* entry = nullptr;
    i32 min_value = INT32_MAX;

    for (usize i = 0; i < ENTRIES_PER_CLUSTER; i++) {
        auto& candidate = cluster.entries[i];

        // replace if empty or same key
        if (candidate.flag() == Flag::INVALID || packed_key == candidate.key) {
            entry = &candidate;
            break;
        }

        // otherwise replace worst entry
        const i32 value = candidate.value(age_);
        if (value < min_value) {
            min_value = value;
            entry = &candidate;
        }
    }
    assert(entry != nullptr);

    if (!(flag == Flag::EXACT || packed_key != entry->key || entry->age() != age_
          || depth + TT_REPLACEMENT_DEPTH_OFFSET > entry->depth))
        return;

    if (move || entry->key != packed_key) entry->move = static_cast<u16>(move);

    // correct mate score when storing (https://youtu.be/XfeuxubYlT0)
    if (utils::is_loss(score))
        score -= ply;
    else if (utils::is_win(score))
        score += ply;

    // set
    entry->key = packed_key;
    entry->score = static_cast<i16>(score);
    entry->static_eval = static_cast<i16>(static_eval);
    entry->depth = static_cast<u8>(depth);
    entry->set_age_flag(age_, flag);
}

void TranspositionTable::clear() {
    assert(table_ != nullptr);
    assert(size_ > 0);

    memset(table_, 0, size_ * CLUSTER_SIZE);

    age_ = 0;
}

void TranspositionTable::do_age() { age_ = (age_ + 1) & MAX_AGE; }

i32 TranspositionTable::hashfull() const {
    i32 filled = 0;

    for (usize i = 0; i < 1000; i++) {
        const auto& cluster = table_[i];
        for (usize j = 0; j < ENTRIES_PER_CLUSTER; j++) {
            const auto& entry = cluster.entries[j];
            if (entry.flag() != Flag::INVALID && entry.age() == age_) filled++;
        }
    }

    return filled / ENTRIES_PER_CLUSTER;
}


u64 TranspositionTable::index(u64 key) const {
    // key >> 64 = 0~1, index at this fraction of the way through size_
    return static_cast<u64>((static_cast<u128>(key) * static_cast<u128>(size_)) >> 64);
}

void TranspositionTable::allocate(usize newsize) {
    assert(table_ == nullptr);
    assert(capacity_ == 0);

#if defined(__linux__)
    static constexpr usize page_size = 2 * 1024 * 1024;
#else
    static constexpr usize page_size = 4096;
#endif

    const usize newsize_s = ((newsize * CLUSTER_SIZE + page_size - 1) / page_size) * page_size;
    capacity_ = newsize_s / CLUSTER_SIZE;

#if defined(__linux__)
    table_ = static_cast<Cluster*>(aligned_alloc(page_size, newsize_s));
    madvise(table_, newsize_s, MADV_HUGEPAGE);
#elif defined(_WIN32)
    table_ = static_cast<Cluster*>(_aligned_malloc(newsize_s, page_size));
#else
    table_ = static_cast<Cluster*>(aligned_alloc(page_size, newsize_s));
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
