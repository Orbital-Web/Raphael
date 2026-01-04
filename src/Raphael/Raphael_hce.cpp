#include <Raphael/Raphael_hce.h>

using namespace Raphael;
using std::array;
using std::max;
using std::min;

#define whiteturn (board.sideToMove() == chess::Color::WHITE)



const array<array<array<int, 2>, 64>, 12> RaphaelHCE::PST = [] {
    array<array<array<int, 2>, 64>, 12> pst{};

    static constexpr int PAWN_MID[64] = {
        0,  0,   0,   0,   0,   0,   0,   0,     //
        43, 144, 79,  113, 29,  152, -31, -198,  //
        65, 89,  111, 127, 171, 228, 143, 82,    //
        25, 61,  50,  91,  103, 78,  80,  32,    //
        -3, 7,   44,  67,  74,  61,  47,  7,     //
        10, 11,  38,  37,  62,  59,  90,  42,    //
        5,  21,  5,   20,  35,  91,  99,  31,    //
        0,  0,   0,   0,   0,   0,   0,   0,     //
    };
    static constexpr int PAWN_END[64] = {
        0,   0,   0,   0,  0,   0,   0,   0,    //
        183, 160, 123, 83, 123, 100, 163, 203,  //
        85,  70,  46,  1,  -11, 11,  49,  61,   //
        56,  38,  26,  1,  5,   11,  29,  34,   //
        37,  30,  12,  2,  5,   7,   14,  16,   //
        23,  23,  11,  13, 14,  11,  5,   5,    //
        29,  21,  29,  16, 24,  15,  9,   3,    //
        0,   0,   0,   0,  0,   0,   0,   0,    //
    };
    static constexpr int KNIGHT_MID[64] = {
        -78, -24, 109, 152, 287, -12, 157, -21,  //
        56,  113, 284, 256, 276, 287, 150, 156,  //
        103, 295, 262, 306, 396, 428, 309, 281,  //
        196, 219, 234, 283, 249, 307, 233, 239,  //
        174, 213, 228, 216, 242, 225, 250, 192,  //
        153, 181, 211, 214, 242, 229, 225, 166,  //
        143, 101, 168, 204, 205, 224, 153, 191,  //
        -17, 173, 139, 159, 195, 197, 173, 154,  //
    };
    static constexpr int KNIGHT_END[64] = {
        94,  126, 136, 115, 112, 128, 60,  35,   //
        131, 155, 130, 157, 139, 123, 132, 96,   //
        133, 132, 173, 171, 139, 142, 136, 106,  //
        134, 168, 196, 187, 192, 176, 175, 134,  //
        136, 157, 185, 194, 185, 189, 165, 136,  //
        125, 154, 160, 182, 173, 152, 140, 130,  //
        118, 148, 149, 153, 157, 133, 142, 100,  //
        142, 99,  134, 144, 127, 120, 91,  73,   //
    };
    static constexpr int BISHOP_MID[64] = {
        69,  98,  -96, -34, -13, 29,  11,  74,   //
        49,  92,  46,  59,  143, 132, 91,  21,   //
        70,  123, 122, 135, 153, 212, 141, 113,  //
        75,  93,  102, 147, 114, 133, 97,  96,   //
        104, 125, 88,  111, 118, 76,  88,  136,  //
        108, 130, 108, 87,  95,  114, 120, 115,  //
        140, 119, 122, 90,  102, 136, 147, 112,  //
        40,  131, 121, 111, 144, 100, 58,  85,   //
    };
    static constexpr int BISHOP_END[64] = {
        93,  95,  110, 111, 119, 109, 97,  80,   //
        107, 115, 123, 103, 105, 106, 109, 105,  //
        121, 101, 112, 101, 98,  102, 118, 113,  //
        107, 124, 120, 118, 115, 117, 113, 115,  //
        108, 105, 126, 125, 114, 112, 111, 99,   //
        104, 112, 118, 122, 120, 111, 104, 102,  //
        95,  89,  103, 106, 117, 98,  98,  84,   //
        97,  106, 86,  107, 100, 100, 111, 102,  //

    };
    static constexpr int ROOK_MID[64] = {
        238, 234, 228, 275, 284, 190, 180, 252,  //
        214, 206, 257, 279, 307, 320, 239, 269,  //
        150, 228, 214, 258, 211, 286, 356, 220,  //
        145, 156, 191, 191, 210, 230, 193, 186,  //
        112, 132, 145, 155, 176, 145, 210, 136,  //
        101, 129, 147, 147, 172, 169, 183, 143,  //
        110, 149, 147, 169, 183, 181, 195, 85,   //
        148, 146, 152, 166, 176, 171, 147, 173,  //
    };
    static constexpr int ROOK_END[64] = {
        291, 291, 300, 295, 290, 295, 296, 284,  //
        298, 299, 295, 288, 272, 269, 288, 280,  //
        302, 290, 289, 283, 285, 268, 264, 280,  //
        293, 294, 296, 284, 281, 283, 283, 289,  //
        295, 291, 295, 288, 277, 277, 265, 279,  //
        284, 287, 275, 281, 272, 263, 272, 268,  //
        280, 275, 284, 282, 269, 265, 262, 279,  //
        274, 285, 289, 282, 275, 269, 278, 246,  //
    };
    static constexpr int QUEEN_MID[64] = {
        613, 599, 613, 621, 778, 814, 757, 662,  //
        578, 541, 618, 618, 617, 745, 646, 718,  //
        601, 591, 613, 619, 688, 765, 752, 714,  //
        578, 587, 588, 590, 609, 631, 627, 627,  //
        604, 566, 596, 595, 607, 608, 633, 621,  //
        595, 628, 609, 611, 601, 626, 626, 628,  //
        555, 612, 639, 631, 631, 647, 615, 671,  //
        618, 593, 607, 640, 600, 584, 539, 549,  //
    };
    static constexpr int QUEEN_END[64] = {
        583, 643, 660, 651, 593, 578, 569, 629,  //
        594, 645, 654, 677, 682, 622, 648, 594,  //
        590, 630, 626, 670, 668, 645, 614, 615,  //
        613, 632, 653, 683, 685, 658, 687, 671,  //
        574, 641, 628, 672, 643, 655, 640, 629,  //
        573, 557, 616, 594, 623, 604, 619, 604,  //
        576, 562, 546, 567, 575, 553, 545, 518,  //
        548, 552, 564, 522, 585, 539, 578, 521,  //
    };
    static constexpr int KING_MID[64] = {
        -49, 336, 374, 89,   -138, -87,  135,  -25,   //
        283, 40,  -92, 141,  59,   -137, -120, -207,  //
        45,  -31, 5,   -6,   -28,  142,  52,   -31,   //
        6,   -83, -53, -163, -108, -100, -42,  -188,  //
        -97, 17,  -85, -165, -145, -153, -132, -184,  //
        27,  18,  -55, -107, -113, -105, -35,  -75,   //
        55,  17,  -45, -119, -86,  -40,  32,   53,    //
        -35, 71,  50,  -113, 30,   -40,  92,   64,    //
    };
    static constexpr int KING_END[64] = {
        -108, -106, -90, -41, 1,   19,  0,   -41,  // A8, B8, ...
        -55,  29,   32,  22,  35,  62,  56,  33,   //
        15,   35,   39,  31,  39,  58,  64,  25,   //
        -4,   42,   46,  55,  45,  56,  46,  17,   //
        -14,  1,    41,  47,  49,  46,  24,  4,    //
        -30,  0,    24,  38,  43,  33,  15,  -4,   //
        -45,  -11,  17,  28,  28,  14,  -10, -37,  //
        -77,  -51,  -33, -9,  -44, -18, -53, -88,  // A1, B1, ...
    };
    static const int* PST_MID[6] = {
        PAWN_MID,
        KNIGHT_MID,
        BISHOP_MID,
        ROOK_MID,
        QUEEN_MID,
        KING_MID,
    };
    static const int* PST_END[6] = {
        PAWN_END,
        KNIGHT_END,
        BISHOP_END,
        ROOK_END,
        QUEEN_END,
        KING_END,
    };

    for (int p = 0; p < 6; p++) {
        for (int sq = 0; sq < 64; sq++) {
            // flip to put sq56 -> A1 and so on
            pst[p][sq][0] = PST_MID[p][sq ^ 56];
            pst[p][sq][1] = PST_END[p][sq ^ 56];
            // flip for black
            pst[p + 6][sq][0] = -PST_MID[p][sq];
            pst[p + 6][sq][1] = -PST_END[p][sq];
        }
    }
    return pst;
}();

const RaphaelHCE::PMasks RaphaelHCE::PMASK = [] {
    PMasks pmask{};

    static constexpr uint64_t filemask = 0x0101010101010101;  // A-file
    static constexpr uint64_t rankregion = 0xFFFFFFFFFF;      // ranks 1~5

    for (int sq = 0; sq < 64; sq++) {
        pmask.ISOLATED[sq] = 0;
        int file = sq % 8;
        int rank = sq / 8;

        // left file of pawn
        if (file > 0) pmask.ISOLATED[sq] |= filemask << (file - 1);
        // right file of pawn
        if (file < 7) pmask.ISOLATED[sq] |= filemask << (file + 1);

        // middle file of pawn for passed mask
        pmask.WPASSED[sq] = filemask << file;
        pmask.WPASSED[sq] |= pmask.ISOLATED[sq];
        // same for black
        pmask.BPASSED[sq] = pmask.WPASSED[sq];

        // crop ranks above of pawn for white passed
        pmask.WPASSED[sq] &= UINT64_MAX << (8 * (rank + 1));
        // crop ranks below of pawn for black passed
        pmask.BPASSED[sq] &= UINT64_MAX >> (8 * (8 - rank));

        // crop ±2 ranks of pawn for isolation
        // even if adjacent files are occupied,
        // the pawns must be close to each other
        if (rank > 1)
            pmask.ISOLATED[sq] &= rankregion << (8 * (rank - 2));
        else
            pmask.ISOLATED[sq] &= rankregion >> (8 * (2 - rank));
    }
    return pmask;
}();


int RaphaelHCE::evaluate(const chess::Board& board) {
    int eval_mid = 0, eval_end = 0;
    int phase = 0;
    auto pieces = board.occ();

    // draw evaluation
    int wbish_on_w = 0, wbish_on_b = 0;  // number of white bishop on light and dark tiles
    int bbish_on_w = 0, bbish_on_b = 0;  // number of black bishop on light and dark tiles
    int wbish = 0, bbish = 0;
    int wknight = 0, bknight = 0;
    bool minor_only = true;

    // mobility
    int wkr = 0, bkr = 0;          // king rank
    int wkf = 0, bkf = 0;          // king file
    int bishmob = 0, rookmob = 0;  // number of squares bishop and rooks see (white - black)
    // xray bitboards
    auto wbishx = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE);
    auto bbishx = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK);
    auto wrookx = wbishx & ~board.pieces(chess::PieceType::ROOK, chess::Color::WHITE);
    auto brookx = bbishx & ~board.pieces(chess::PieceType::ROOK, chess::Color::BLACK);
    auto wpawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    auto bpawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    // loop through all pieces
    while (pieces) {
        auto sqi = pieces.pop();
        chess::Square sq = sqi;
        int piece = (int)board.at(sq);

        // add material value
        eval_mid += PVAL[piece][0];
        eval_end += PVAL[piece][1];
        // add positional value
        eval_mid += PST[piece][sqi][0];
        eval_end += PST[piece][sqi][1];

        switch (piece) {
            // pawn structure
            case 0:
                minor_only = false;
                // passed (+ for white)
                if ((PMASK.WPASSED[sqi] & bpawns) == 0) {
                    eval_mid += PAWN_PASSED_WEIGHT[7 - (sqi / 8)][0];
                    eval_end += PAWN_PASSED_WEIGHT[7 - (sqi / 8)][1];
                }
                // isolated (- for white)
                if ((PMASK.ISOLATED[sqi] & wpawns) == 0) {
                    eval_mid -= PAWN_ISOLATION_WEIGHT[0];
                    eval_end -= PAWN_ISOLATION_WEIGHT[1];
                }
                break;
            case 6:
                minor_only = false;
                // passed (- for white)
                if ((PMASK.BPASSED[sqi] & wpawns) == 0) {
                    eval_mid -= PAWN_PASSED_WEIGHT[sqi / 8][0];
                    eval_end -= PAWN_PASSED_WEIGHT[sqi / 8][1];
                }
                // isolated (+ for white)
                if ((PMASK.ISOLATED[sqi] & bpawns) == 0) {
                    eval_mid += PAWN_ISOLATION_WEIGHT[0];
                    eval_end += PAWN_ISOLATION_WEIGHT[1];
                }
                break;

            // knight count
            case 1:
                phase++;
                wknight++;
                break;
            case 7:
                phase++;
                bknight++;
                break;

            // bishop mobility (xrays queens)
            case 2:
                phase++;
                wbish++;
                wbish_on_w += sq.is_light();
                wbish_on_b += sq.is_dark();
                bishmob += chess::attacks::bishop(sq, wbishx).count();
                break;
            case 8:
                phase++;
                bbish++;
                bbish_on_w += sq.is_light();
                bbish_on_b += sq.is_dark();
                bishmob -= chess::attacks::bishop(sq, bbishx).count();
                break;

            // rook mobility (xrays rooks and queens)
            case 3:
                phase += 2;
                minor_only = false;
                rookmob += chess::attacks::rook(sq, wrookx).count();
                break;
            case 9:
                phase += 2;
                minor_only = false;
                rookmob -= chess::attacks::rook(sq, brookx).count();
                break;

            // queen count
            case 4:
                phase += 4;
                minor_only = false;
                break;
            case 10:
                phase += 4;
                minor_only = false;
                break;

            // king proximity
            case 5:
                wkr = (int)sq.rank();
                wkf = (int)sq.file();
                break;
            case 11:
                bkr = (int)sq.rank();
                bkf = (int)sq.file();
                break;
        }
    }

    // mobility
    eval_mid += bishmob * MOBILITY_BISHOP[0];
    eval_end += bishmob * MOBILITY_BISHOP[1];
    eval_mid += rookmob * MOBILITY_ROOK[0];
    eval_end += rookmob * MOBILITY_ROOK[1];

    // bishop pair
    bool wbish_pair = wbish_on_w && wbish_on_b;
    bool bbish_pair = bbish_on_w && bbish_on_b;
    if (wbish_pair) {
        eval_mid += BISH_PAIR_WEIGHT[0];
        eval_end += BISH_PAIR_WEIGHT[1];
    }
    if (bbish_pair) {
        eval_mid -= BISH_PAIR_WEIGHT[0];
        eval_end -= BISH_PAIR_WEIGHT[1];
    }

    // convert perspective
    if (!whiteturn) {
        eval_mid *= -1;
        eval_end *= -1;
    }

    // King proximity bonus (if winning)
    int king_dist = abs(wkr - bkr) + abs(wkf - bkf);
    if (eval_mid >= 0) eval_mid += KING_DIST_WEIGHT[0] * (14 - king_dist);
    if (eval_end >= 0) eval_end += KING_DIST_WEIGHT[1] * (14 - king_dist);

    // Bishop corner (if winning)
    int ourbish_on_w = (whiteturn) ? wbish_on_w : bbish_on_w;
    int ourbish_on_b = (whiteturn) ? wbish_on_b : bbish_on_b;
    int ekr = (whiteturn) ? bkr : wkr;
    int ekf = (whiteturn) ? bkf : wkf;
    int wtile_dist = min(ekf + (7 - ekr), (7 - ekf) + ekr);  // to A8 and H1
    int btile_dist = min(ekf + ekr, (7 - ekf) + (7 - ekr));  // to A1 and H8
    if (eval_mid >= 0) {
        if (ourbish_on_w) eval_mid += BISH_CORNER_WEIGHT[0] * (7 - wtile_dist);
        if (ourbish_on_b) eval_mid += BISH_CORNER_WEIGHT[0] * (7 - btile_dist);
    }
    if (eval_end >= 0) {
        if (ourbish_on_w) eval_end += BISH_CORNER_WEIGHT[1] * (7 - wtile_dist);
        if (ourbish_on_b) eval_end += BISH_CORNER_WEIGHT[1] * (7 - btile_dist);
    }

    // apply phase
    int eg_weight = 256 * max(0, 24 - phase) / 24;
    int eval = ((256 - eg_weight) * eval_mid + eg_weight * eval_end) / 256;

    // draw division
    int wminor = wbish + wknight;
    int bminor = bbish + bknight;
    if (minor_only && wminor <= 2 && bminor <= 2) {
        if ((wminor == 1 && bminor == 1) ||                                      // 1 vs 1
            ((wbish + bbish == 3) && (wminor + bminor == 3)) ||                  // 2B vs B
            ((wknight == 2 && bminor <= 1) || (bknight == 2 && wminor <= 1)) ||  // 2N vs 0:1
            (!wbish_pair && wminor == 2 && bminor == 1) ||  // 2 vs 1, not bishop pair
            (!bbish_pair && bminor == 2 && wminor == 1))
            return eval / DRAW_DIVIDE_SCALE;
    }
    return eval;
}
