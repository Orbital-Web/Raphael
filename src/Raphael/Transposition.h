#pragma once
#include <Raphael/consts.h>

#include <chess.hpp>

using std::vector;



namespace Raphael {
class TranspositionTable {
    static constexpr uint32_t MAX_TABLE_SIZE = 134217728;  // 3GB

public:
    enum Flag { INVALID = -2, LOWER, EXACT, UPPER };
    // storage type (size = 24 bytes)
    struct Entry {
        uint64_t key;   // 8 bytes (8 bytes)
        int depth: 30;  // 4 bytes (8 bytes)
        Flag flag: 2;
        int eval;          // 4 bytes (8 bytes)
        chess::Move move;  // 4 bytes
    };

private:
    uint32_t size;
    vector<Entry> _table;


public:
    /** Initializes the Transposition Table
     *
     * \param size_in the number of entries the table holds
     */
    TranspositionTable(const uint32_t size_in);

    /** Retrieves the table value for a given key (assumes valid is true)
     *
     * \param key key to look up
     * \param ply number of half moves made
     * \returns the Entry in the table
     */
    Entry get(const uint64_t key, const int ply) const;

    /** Sets an entry for a given key
     *
     * \param entry the entry to store
     * \param ply number of half moves made
     */
    void set(const Entry entry, const int ply);

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
