#pragma once
#include <chess/include.h>



namespace raphael {
// https://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
class Cuckoo {
public:
    u64 keys[8192] = {};
    chess::Move moves[8192] = {};

    // hash functions into cuckoo tables
    constexpr u32 h1(u64 key) { return static_cast<u32>(key & 0x1FFF); }
    constexpr u32 h2(u64 key) { return static_cast<u32>((key >> 16) & 0x1FFF); }

    /** Initializes the cuckoo tables */
    Cuckoo();
};

inline Cuckoo cuckoo;
}  // namespace raphael
