#pragma once
#include <Raphael/consts.h>

#include <chess.hpp>



namespace Raphael {
class TranspositionTable {
private:
    // table entry storage type (16 bytes)
    struct EntryStorage {
        uint64_t key;
        uint64_t val;  // 63-32: eval, 31-16: move, 15-14: flag, 13-0: depth
    };

    uint32_t size;
    std::vector<EntryStorage> _table;

public:
    static constexpr uint32_t MAX_TABLE_SIZE = 201326592;       // 3GB
    static constexpr uint32_t DEF_TABLE_SIZE = 4194304;         // 64MB
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
    TranspositionTable(const uint32_t size_mb);

    /** Retrieves the table value for a given key (assumes valid is true)
     *
     * \param key key to look up
     * \param ply current distance from root
     * \returns the Entry in the table
     */
    Entry get(const uint64_t key, const int ply) const;

    /** Sets an entry for a given key
     *
     * \param entry the entry to store
     * \param ply current distance from root
     */
    void set(const Entry& entry, const int ply);

    /** Clears the table */
    void clear();

    /** Whether the entry is valid or not at the current depth
     *
     * \param entry the entry to check
     * \param key key looking up
     * \param depth the current depth
     * \returns whether entry is valid
     */
    static bool valid(const Entry entry, const uint64_t key, const int depth);

    /** Whether the given eval implies a mate
     *
     * \param eval the eval score
     * \returns whether the eval implies a mate or not
     */
    static bool isMate(const int eval);

private:
    /** Computes the index on the table
     *
     * \param key the key to use
     * \returns the index of the key in the table
     */
    uint64_t index(const uint64_t key) const;
};
}  // namespace Raphael
