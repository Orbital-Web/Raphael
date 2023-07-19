#pragma once
#include "../GameEngine/chess.hpp"
#include <vector>



namespace Raphael {
class TranspositionTable {
// class variables
public:
    static constexpr unsigned int MAX_TABLE_SIZE = 134217728;   // 1.125~1.5GB depending on system
    // storage type (size = 9 or 12 bytes depending on system)
    struct Entry {
        unsigned int depth: 6;  // [0, 64)
        int flag: 2;            // -1: beta, 0: exact, 1: alpha, -2 (invalid)
        int score;
        chess::Move move;
    };

private:
    unsigned int size;
    std::vector<Entry> _table;



// methods
public:
    // Initialize the Transposition Table (TranspositionTable<size>)
    TranspositionTable(unsigned int size_in): size(size_in), _table(size, {.flag = -2}) {
        assert((size>0 && size<=MAX_TABLE_SIZE));
    }


    // Retrieves the table value for a given key
    Entry get(uint64_t key) const {
        return _table[index(key)];
    }


    // Sets an entry for the given key
    void set(uint64_t key, Entry entry) {
        _table[index(key)] = entry;
    }


    // Clears the table
    void clear() {
        std::fill(_table.begin(), _table.end(), (Entry){.flag = -2});
    }

private:
    // Get index on table
    uint64_t index(uint64_t key) const {
        return key%size;
    }
};
}