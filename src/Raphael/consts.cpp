#include <Raphael/consts.h>


namespace Raphael {
namespace PMASK {
uint64_t WPASSED[64];
uint64_t BPASSED[64];
uint64_t ISOLATED[64];

void init_pawnmask() {
    uint64_t filemask = 0x0101010101010101;  // A-file
    uint64_t rankregion = 0xFFFFFFFFFF;      // ranks 1~5

    for (int sq = 0; sq < 64; sq++) {
        ISOLATED[sq] = 0;
        int file = sq % 8;
        int rank = sq / 8;

        // left file of pawn
        if (file > 0) ISOLATED[sq] |= filemask << (file - 1);
        // right file of pawn
        if (file < 7) ISOLATED[sq] |= filemask << (file + 1);

        // middle file of pawn for passed mask
        WPASSED[sq] = filemask << file;
        WPASSED[sq] |= ISOLATED[sq];
        // same for black
        BPASSED[sq] = WPASSED[sq];

        // crop ranks above of pawn for white passed
        WPASSED[sq] &= UINT64_MAX << (8 * (rank + 1));
        // crop ranks below of pawn for black passed
        BPASSED[sq] &= UINT64_MAX >> (8 * (8 - rank));

        // crop ±2 ranks of pawn for isolation
        // even if adjacent files are occupied,
        // the pawns must be close to each other
        if (rank > 1)
            ISOLATED[sq] &= rankregion << (8 * (rank - 2));
        else
            ISOLATED[sq] &= rankregion >> (8 * (2 - rank));
    }
}
}  // namespace PMASK
}  // namespace Raphael
