#pragma once
#include <chess/include.h>

#include <vector>



namespace raphael {
class TranspositionTable {
public:
    static constexpr i32 MAX_TABLE_SIZE_MB = 65536;  // 64GB
    static constexpr i32 DEF_TABLE_SIZE_MB = 64;
    static constexpr usize ENTRIES_PER_CLUSTER = 3;

    enum Flag : u8 { INVALID = 0, LOWER, EXACT, UPPER };

    struct Entry {
        u16 key;          // zobrist hash of position
        i16 score;        // score of the position
        i16 static_eval;  // static eval of the position
        u16 move;         // bestmove
        u8 depth;         // max 255
        u8 age_pv_flag;   // 5 bits age, 1 bit ttpv, 2 bits flag

        u32 age() const;
        bool pv() const;
        Flag flag() const;

        /** Sets the age and flag of the entry
         *
         * \param age age to set
         * \param pv whether this entry is on the PV
         * \param flag flag to set
         */
        void set_age_pv_flag(u32 age, bool pv, Flag flag);
    };
    static_assert(sizeof(Entry) == 10);

    struct alignas(32) Cluster {
        Entry entries[ENTRIES_PER_CLUSTER];
        u16 pad;
    };
    static constexpr usize CLUSTER_SIZE = sizeof(Cluster);
    static_assert(CLUSTER_SIZE == 32);

    struct ProbedEntry {
        i32 score;
        i32 static_eval;
        i32 depth;
        chess::Move move;
        bool was_PV;
        Flag flag;
    };

private:
    usize size_;
    usize capacity_;
    Cluster* table_;

    u32 age_ = 0;
    static constexpr u32 AGE_BITS = 5;
    static constexpr u32 MAX_AGE = (1 << AGE_BITS) - 1;


public:
    /** Initializes the Transposition Table
     *
     * \param size_mb the size of the table (in MB)
     */
    explicit TranspositionTable(i32 size_mb);

    /** Destructs and deallocates the table */
    ~TranspositionTable();

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;

    /** Resizes the Transposition Table
     *
     * \param size_mb the size of the table (in MB)
     */
    void resize(i32 size_mb);

    /** Retrieves the table entry for a given key
     *
     * \param ttentry entry to put probed result into
     * \param key key to look up
     * \param ply current distance from root
     * \returns whether there was a tt hit or not
     */
    bool get(ProbedEntry& ttentry, u64 key, i32 ply) const;

    /** Prefetches a table entry
     *
     * \param key key to prefetch
     */
    void prefetch(u64 key) const;

    /** Sets an entry for a given key
     *
     * \param key zobrist hash of position
     * \param score score of the position
     * \param static_eval static eval of the position
     * \param move bestmove
     * \param depth max 255
     * \param flag invalid, lower, exact, or upper
     * \param pv whether the entry is on the PV
     * \param ply current distance from root
     */
    void set(
        u64 key,
        i32 score,
        i32 static_eval,
        chess::Move move,
        i32 depth,
        bool pv,
        Flag flag,
        i32 ply
    );

    /** Clears the table */
    void clear();

    /** Increments the tt age */
    void do_age();

    /** Returns how full the table is */
    i32 hashfull() const;

private:
    /** Computes the index on the table
     *
     * \param key the key to use
     * \returns the index of the key in the table
     */
    u64 index(u64 key) const;

    /** Allocates the table and sets capacity_ and table_ (not size)
     *
     * \param newsize new size in number of entries
     */
    void allocate(usize newsize);

    /** Deallocates the table (if allocated) and sets capacity_ and table_ (not size) */
    void deallocate();
};
}  // namespace raphael
