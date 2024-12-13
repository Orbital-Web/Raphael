#pragma once
#include <chess.hpp>
#include <Raphael/consts.hpp>



namespace Raphael {
class TranspositionTable {
static constexpr uint32_t MAX_TABLE_SIZE = 134217728;   // 3GB

// TranspositionTable vars
public:
    enum Flag {INVALID=-2, LOWER, EXACT, UPPER}; 
    // storage type (size = 24 bytes)
    struct Entry {
        uint64_t key;       // 8 bytes (8 bytes)
        int depth: 30;      // 4 bytes (8 bytes)
        Flag flag: 2;           
        int eval;           // 4 bytes (8 bytes)
        chess::Move move;   // 4 bytes
    };

private:
    uint32_t size;
    std::vector<Entry> _table;



// TranspositionTable methods
public:
    // Initializes the Transposition Table (TranspositionTable<size>)
    TranspositionTable(const uint32_t size_in): size(size_in), _table(size, {.flag=INVALID, .move=chess::Move::NO_MOVE}) {
        assert((size>0 && size<=MAX_TABLE_SIZE));   // size is within (0, MAX_TABLE_SIZE]
        //assert(((size & (size-1)) == 0));           // size is a power of 2
    }


    // Retrieves the table value for a given key (assumes valid is true)
    Entry get(const uint64_t key, const int ply) const {
        // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
        Entry entry = _table[index(key)];
        if (entry.eval < -MATE_EVAL+1000) entry.eval += ply;
        else if (entry.eval > MATE_EVAL-1000) entry.eval -= ply;
        return entry;
    }


    // Sets an entry for the given key
    void set(const Entry entry, const int ply) {
        // correct mate eval when storing (https://youtu.be/XfeuxubYlT0)
        auto key = index(entry.key);
        _table[key] = entry;
        if (entry.eval < -MATE_EVAL+1000) _table[key].eval -= ply;
        else if (entry.eval > MATE_EVAL-1000) _table[key].eval += ply;
    }


    // Clears the table
    void clear() {
        std::fill(_table.begin(), _table.end(), (Entry){.flag=INVALID, .move=chess::Move::NO_MOVE});
    }


    // Whether the entry is valid or not at the current depth
    static bool valid(const Entry entry, const uint64_t key, const int depth) {
        return ((entry.flag != INVALID) && (entry.depth >= depth) && (entry.key == key));
    }


    // Whether the given eval implies a mate
    static bool isMate(const int eval) {
        int abseval = abs(eval);
        return ((abseval<=MATE_EVAL) && (abseval>MATE_EVAL-1000));
    }

private:
    // Gets index on table
    uint64_t index(const uint64_t key) const {
        return key%size;
    }
};  // TranspositionTable
}   // namespace Raphael