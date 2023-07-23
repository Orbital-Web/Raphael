#pragma once
#include "../GameEngine/chess.hpp"
#include <vector>



namespace Raphael {
class TranspositionTable {
// class variables
public:
    static constexpr unsigned int MAX_TABLE_SIZE = 134217728;   // 2.5GB

    // storage type (size = 20 bytes)
    enum Flag {INVALID=-2, LOWER, EXACT, UPPER}; 
    struct Entry {
        uint64_t key;           // 8 bytes
        unsigned int depth: 30; // 4 bytes
        Flag flag: 2;
        int eval;               // 4 bytes
        chess::Move move;       // 4 bytes
    };

private:
    unsigned int size;
    std::vector<Entry> _table;



// methods
public:
    // Initialize the Transposition Table (TranspositionTable<size>)
    TranspositionTable(unsigned int size_in): size(size_in), _table(size, {.flag = INVALID}) {
        assert((size>0 && size<=MAX_TABLE_SIZE));   // size is within (0, MAX_TABLE_SIZE]
        assert(((size & (size-1)) == 0));           // size is a power of 2
    }


    // Retrieves the table value for a given key (assumes valid is true)
    Entry get(uint64_t key, int ply) const {
        // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
        Entry entry = _table[index(key)];
        if (entry.eval < -MATE_EVAL+1000) entry.eval += ply;
        else if (entry.eval > MATE_EVAL-1000) entry.eval -= ply;
        return entry;
    }


    // Sets an entry for the given key
    void set(uint64_t key, Entry entry, int ply) {
        // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
        if (entry.eval < -MATE_EVAL+1000) entry.eval -= ply;
        else if (entry.eval > MATE_EVAL-1000) entry.eval += ply;
        _table[index(key)] = entry;
    }


    // Clears the table
    void clear() {
        std::fill(_table.begin(), _table.end(), (Entry){.flag = INVALID});
    }


    // Whether the entry is valid or not at the current depth
    static bool valid(Entry& entry, uint64_t key, int depth) {
        return ((entry.flag != INVALID) && (entry.depth >= depth) && (entry.key == key));
    }


    // whether the given eval implies a mate
    static bool isMate(int eval) {
        int abseval = abs(eval);
        return ((abseval<=MATE_EVAL) && (abseval>MATE_EVAL-1000));
    }

private:
    // Get index on table
    uint64_t index(uint64_t key) const {
        // same as key%size since size = 2**n
        return key&size;
    }
};
}