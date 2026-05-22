#include <Raphael/cuckoo.h>

#include <algorithm>

using namespace raphael;
using std::swap;



Cuckoo::Cuckoo() {
    u32 count = 0;

    for (chess::Piece piece = chess::Piece::WHITEKNIGHT; piece <= chess::Piece::BLACKKING; ++piece)
    {
        // skip pawns moves as they are irreversible
        if (piece == chess::Piece::BLACKPAWN) continue;

        for (chess::Square sq0 = chess::Square::A1; sq0 <= chess::Square::H8; ++sq0) {
            for (chess::Square sq1 = sq0 + 1; sq1 <= chess::Square::H8; ++sq1) {
                if (!chess::Attacks::nonpawn_attack(piece.type(), sq0, 0).is_set(sq1)) continue;

                auto move = chess::Move::make(sq0, sq1);
                u64 key_delta = chess::Zobrist::piece(piece, sq0)
                                ^ chess::Zobrist::piece(piece, sq1) ^ chess::Zobrist::stm();
                u32 slot = h1(key_delta);

                // insert into cuckoo table
                while (true) {
                    swap(keys[slot], key_delta);
                    swap(moves[slot], move);
                    if (!move) break;
                    slot = (slot == h1(key_delta)) ? h2(key_delta) : h1(key_delta);
                }

                count++;
            }
        }
    }

    assert(count == 7336 / 2);  // 7336 reversible moves, hash of (sq0, sq1) == (sq1, sq0) so half
}
