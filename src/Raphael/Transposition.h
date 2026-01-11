#pragma once

#include <chess.hpp>
#include <vector>



namespace Raphael {
class TranspositionTable {
private:
    // table entry storage type (16 bytes)
    struct EntryStorage {
        uint64_t key;
        uint64_t val;  // 63-32: eval, 31-16: move, 15-14: flag, 13-0: depth
    };

    uint64_t size;
    uint64_t capacity;
    EntryStorage* _table;

public:
    static constexpr uint64_t MAX_TABLE_SIZE = 201326592;       // 3GB
    static constexpr uint64_t DEF_TABLE_SIZE = 4194304;         // 64MB
    static constexpr size_t ENTRY_SIZE = sizeof(EntryStorage);  // 16 bytes

    enum Flag { INVALID = 0, LOWER, EXACT, UPPER };

    // table entry interface
    struct Entry {
        uint64_t key;      // zobrist hash of move
        int depth;         // max 2^14 (16384)
        Flag flag;         // invalid, lower, exact, or upper
        chess::Move move;  // score is ignored
        int eval;          // evaluation of the move
    };


public:
    /** Initializes the Transposition Table
     *
     * \param size_mb the size of the table (in MB)
     */
    explicit TranspositionTable(uint32_t size_mb);

    /** Destructs and deallocates the table */
    ~TranspositionTable();

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;

    /** Resizes the Transposition Table
     *
     * \param size_mb the size of the table (in MB)
     */
    void resize(uint32_t size_mb);

    /** Retrieves the table value for a given key (assumes valid is true)
     *
     * \param key key to look up
     * \param ply current distance from root
     * \returns the Entry in the table
     */
    Entry get(uint64_t key, int ply) const;

    /** Prefetches a table entry
     *
     * \param key key to prefetch
     */
    void prefetch(uint64_t key) const;

    /** Sets an entry for a given key
     *
     * \param entry the entry to store
     * \param ply current distance from root
     */
    void set(const Entry& entry, int ply);

    /** Clears the table */
    void clear();

private:
    /** Computes the index on the table
     *
     * \param key the key to use
     * \returns the index of the key in the table
     */
    uint64_t index(uint64_t key) const;

    /** Allocates the table and sets capacity and _table (not size)
     *
     * \param newsize new size in number of entries
     */
    void allocate(uint64_t newsize);

    /** Deallocates the table (if allocated) and sets capacity and _table (not size) */
    void deallocate();
};
}  // namespace Raphael
