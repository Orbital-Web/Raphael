#pragma once
#include <chess/include.h>

#include <vector>



namespace raphael {
class TranspositionTable {
private:
    // table entry storage type (16 bytes)
    struct EntryStorage {
        u64 key;
        u64 val;  // 63-32: eval, 31-16: move, 15-14: flag, 13-0: depth
    };

    u64 size_;
    u64 capacity_;
    EntryStorage* table_;

public:
    static constexpr u64 MAX_TABLE_SIZE = 201326592;           // 3GB
    static constexpr u64 DEF_TABLE_SIZE = 4194304;             // 64MB
    static constexpr usize ENTRY_SIZE = sizeof(EntryStorage);  // 16 bytes
    static_assert(ENTRY_SIZE == 16);

    enum Flag { INVALID = 0, LOWER, EXACT, UPPER };

    // table entry interface
    struct Entry {
        u64 key;           // zobrist hash of move
        i32 depth;         // max 2^14 (16384)
        Flag flag;         // invalid, lower, exact, or upper
        chess::Move move;  // score is ignored
        i32 eval;          // evaluation of the move
    };


public:
    /** Initializes the Transposition Table
     *
     * \param size_mb the size of the table (in MB)
     */
    explicit TranspositionTable(u32 size_mb);

    /** Destructs and deallocates the table */
    ~TranspositionTable();

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;

    /** Resizes the Transposition Table
     *
     * \param size_mb the size of the table (in MB)
     */
    void resize(u32 size_mb);

    /** Retrieves the table value for a given key (assumes valid is true)
     *
     * \param key key to look up
     * \param ply current distance from root
     * \returns the Entry in the table
     */
    Entry get(u64 key, i32 ply) const;

    /** Prefetches a table entry
     *
     * \param key key to prefetch
     */
    void prefetch(u64 key) const;

    /** Sets an entry for a given key
     *
     * \param entry the entry to store
     * \param ply current distance from root
     */
    void set(const Entry& entry, i32 ply);

    /** Clears the table */
    void clear();

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
    void allocate(u64 newsize);

    /** Deallocates the table (if allocated) and sets capacity_ and table_ (not size) */
    void deallocate();
};
}  // namespace raphael
