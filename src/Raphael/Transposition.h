#pragma once
#include <chess/include.h>

#include <vector>



namespace raphael {
class TranspositionTable {
public:
    static constexpr usize MAX_TABLE_SIZE = 201326592;  // 3GB
    static constexpr usize DEF_TABLE_SIZE = 4194304;    // 64MB

    enum Flag : u8 { INVALID = 0, LOWER, EXACT, UPPER };

    // table entry interface
    struct Entry {
        u64 key;           // zobrist hash of position
        i32 score;         // score of the position
        chess::Move move;  // bestmove
        u8 depth;          // max 255
        Flag flag;         // invalid, lower, exact, or upper
    };
    static constexpr usize ENTRY_SIZE = sizeof(Entry);  // 16 bytes
    static_assert(ENTRY_SIZE == 16);

private:
    usize size_;
    usize capacity_;
    Entry* table_;


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

    /** Retrieves the table entry for a given key
     *
     * \param ttentry entry to put probed result into
     * \param key key to look up
     * \param ply current distance from root
     * \returns whether there was a tt hit or not
     */
    bool get(Entry& ttentry, u64 key, i32 ply) const;

    /** Prefetches a table entry
     *
     * \param key key to prefetch
     */
    void prefetch(u64 key) const;

    /** Sets an entry for a given key
     *
     * \param key zobrist hash of position
     * \param score score of the position
     * \param move bestmove
     * \param depth max 255
     * \param flag invalid, lower, exact, or upper
     * \param ply current distance from root
     */
    void set(u64 key, i32 score, chess::Move move, i32 depth, Flag flag, i32 ply);

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
    void allocate(usize newsize);

    /** Deallocates the table (if allocated) and sets capacity_ and table_ (not size) */
    void deallocate();
};
}  // namespace raphael
