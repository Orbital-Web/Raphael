namespace Raphael {
struct v2_0_params {
    v2_0_params(): PST() { init_pst(); }

    // logic
    static constexpr int ASPIRATION_WINDOW = 50;  // size of aspiration window
    static constexpr int MAX_EXTENSIONS = 16;     // max number of times we can extend the search
    static constexpr int REDUCTION_FROM = 5;      // from which move to apply late move reduction
    static constexpr int PV_STABLE_COUNT = 6;     // consecutive bestmoves to consider pv as stable
    static constexpr int MIN_SKIP_EVAL = 200;     // minimum eval to halt early if pv is stable

    // move ordering
    static constexpr int KILLER_WEIGHT = 50;          // move ordering priority for killer moves
    static constexpr int GOOD_CAPTURE_WEIGHT = 5000;  // move ordering priority for good captures

    // evaluation[midgame, endgame]
    static constexpr int KING_DIST_WEIGHT[2] = {0, 20};  // closer king bonus
    static constexpr int DRAW_DIVIDE_SCALE = 32;         // eval divide scale by for likely draw
    static constexpr int PVAL[12][2] = {
        // WHITE
        {100,   100 }, // PAWN
        {418,   246 }, // KNIGHT
        {449,   274 }, // BISHOP
        {554,   437 }, // ROOK
        {1191,  727 }, // QUEEN
        {0,     0   }, // KING
        // BLACK
        {-100,  -100}, // PAWN
        {-418,  -246}, // KNIGHT
        {-449,  -274}, // BISHOP
        {-554,  -437}, // ROOK
        {-1191, -727}, // QUEEN
        {0,     0   }, // KING
    };  // value of each piece
    int PST[12][64][2];  // piece square table for piece, square, and phase
    static constexpr int PAWN_PASSED_WEIGHT[7][2] = {
        {0,   0  }, // promotion line
        {114, 215},
        {10,  160},
        {4,   77 },
        {-12, 47 },
        {1,   20 },
        {15,  13 },
    };  // bonus for passed pawn based on its rank
    static constexpr int PAWN_ISOLATION_WEIGHT[2] = {29, 21};  // isolated pawn cost
    static constexpr int MOBILITY_BISHOP[2] = {12, 6};         // bishop see/xray square count bonus
    static constexpr int MOBILITY_ROOK[2] = {11, 3};           // rook see/xray square count bonus
    static constexpr int BISH_PAIR_WEIGHT[2] = {39, 72};       // bishop pair bonus
    static constexpr int BISH_CORNER_WEIGHT[2] = {1, 20};  // enemy king dist to bishop corner bonus

    void init_pst() {
        constexpr int PAWN_MID[64] = {
            0,  0,   0,   0,   0,   0,   0,   0,     //
            43, 144, 79,  113, 29,  152, -31, -198,  //
            65, 89,  111, 127, 171, 228, 143, 82,    //
            25, 61,  50,  91,  103, 78,  80,  32,    //
            -3, 7,   44,  67,  74,  61,  47,  7,     //
            10, 11,  38,  37,  62,  59,  90,  42,    //
            5,  21,  5,   20,  35,  91,  99,  31,    //
            0,  0,   0,   0,   0,   0,   0,   0,     //
        };
        constexpr int PAWN_END[64] = {
            0,   0,   0,   0,  0,   0,   0,   0,    //
            183, 160, 123, 83, 123, 100, 163, 203,  //
            85,  70,  46,  1,  -11, 11,  49,  61,   //
            56,  38,  26,  1,  5,   11,  29,  34,   //
            37,  30,  12,  2,  5,   7,   14,  16,   //
            23,  23,  11,  13, 14,  11,  5,   5,    //
            29,  21,  29,  16, 24,  15,  9,   3,    //
            0,   0,   0,   0,  0,   0,   0,   0,    //
        };
        constexpr int KNIGHT_MID[64] = {
            -78, -24, 109, 152, 287, -12, 157, -21,  //
            56,  113, 284, 256, 276, 287, 150, 156,  //
            103, 295, 262, 306, 396, 428, 309, 281,  //
            196, 219, 234, 283, 249, 307, 233, 239,  //
            174, 213, 228, 216, 242, 225, 250, 192,  //
            153, 181, 211, 214, 242, 229, 225, 166,  //
            143, 101, 168, 204, 205, 224, 153, 191,  //
            -17, 173, 139, 159, 195, 197, 173, 154,  //
        };
        constexpr int KNIGHT_END[64] = {
            94,  126, 136, 115, 112, 128, 60,  35,   //
            131, 155, 130, 157, 139, 123, 132, 96,   //
            133, 132, 173, 171, 139, 142, 136, 106,  //
            134, 168, 196, 187, 192, 176, 175, 134,  //
            136, 157, 185, 194, 185, 189, 165, 136,  //
            125, 154, 160, 182, 173, 152, 140, 130,  //
            118, 148, 149, 153, 157, 133, 142, 100,  //
            142, 99,  134, 144, 127, 120, 91,  73,   //
        };
        constexpr int BISHOP_MID[64] = {
            69,  98,  -96, -34, -13, 29,  11,  74,   //
            49,  92,  46,  59,  143, 132, 91,  21,   //
            70,  123, 122, 135, 153, 212, 141, 113,  //
            75,  93,  102, 147, 114, 133, 97,  96,   //
            104, 125, 88,  111, 118, 76,  88,  136,  //
            108, 130, 108, 87,  95,  114, 120, 115,  //
            140, 119, 122, 90,  102, 136, 147, 112,  //
            40,  131, 121, 111, 144, 100, 58,  85,   //
        };
        constexpr int BISHOP_END[64] = {
            93,  95,  110, 111, 119, 109, 97,  80,   //
            107, 115, 123, 103, 105, 106, 109, 105,  //
            121, 101, 112, 101, 98,  102, 118, 113,  //
            107, 124, 120, 118, 115, 117, 113, 115,  //
            108, 105, 126, 125, 114, 112, 111, 99,   //
            104, 112, 118, 122, 120, 111, 104, 102,  //
            95,  89,  103, 106, 117, 98,  98,  84,   //
            97,  106, 86,  107, 100, 100, 111, 102,  //

        };
        constexpr int ROOK_MID[64] = {
            238, 234, 228, 275, 284, 190, 180, 252,  //
            214, 206, 257, 279, 307, 320, 239, 269,  //
            150, 228, 214, 258, 211, 286, 356, 220,  //
            145, 156, 191, 191, 210, 230, 193, 186,  //
            112, 132, 145, 155, 176, 145, 210, 136,  //
            101, 129, 147, 147, 172, 169, 183, 143,  //
            110, 149, 147, 169, 183, 181, 195, 85,   //
            148, 146, 152, 166, 176, 171, 147, 173,  //
        };
        constexpr int ROOK_END[64] = {
            291, 291, 300, 295, 290, 295, 296, 284,  //
            298, 299, 295, 288, 272, 269, 288, 280,  //
            302, 290, 289, 283, 285, 268, 264, 280,  //
            293, 294, 296, 284, 281, 283, 283, 289,  //
            295, 291, 295, 288, 277, 277, 265, 279,  //
            284, 287, 275, 281, 272, 263, 272, 268,  //
            280, 275, 284, 282, 269, 265, 262, 279,  //
            274, 285, 289, 282, 275, 269, 278, 246,  //
        };
        constexpr int QUEEN_MID[64] = {
            613, 599, 613, 621, 778, 814, 757, 662,  //
            578, 541, 618, 618, 617, 745, 646, 718,  //
            601, 591, 613, 619, 688, 765, 752, 714,  //
            578, 587, 588, 590, 609, 631, 627, 627,  //
            604, 566, 596, 595, 607, 608, 633, 621,  //
            595, 628, 609, 611, 601, 626, 626, 628,  //
            555, 612, 639, 631, 631, 647, 615, 671,  //
            618, 593, 607, 640, 600, 584, 539, 549,  //
        };
        constexpr int QUEEN_END[64] = {
            583, 643, 660, 651, 593, 578, 569, 629,  //
            594, 645, 654, 677, 682, 622, 648, 594,  //
            590, 630, 626, 670, 668, 645, 614, 615,  //
            613, 632, 653, 683, 685, 658, 687, 671,  //
            574, 641, 628, 672, 643, 655, 640, 629,  //
            573, 557, 616, 594, 623, 604, 619, 604,  //
            576, 562, 546, 567, 575, 553, 545, 518,  //
            548, 552, 564, 522, 585, 539, 578, 521,  //
        };
        constexpr int KING_MID[64] = {
            -49, 336, 374, 89,   -138, -87,  135,  -25,   //
            283, 40,  -92, 141,  59,   -137, -120, -207,  //
            45,  -31, 5,   -6,   -28,  142,  52,   -31,   //
            6,   -83, -53, -163, -108, -100, -42,  -188,  //
            -97, 17,  -85, -165, -145, -153, -132, -184,  //
            27,  18,  -55, -107, -113, -105, -35,  -75,   //
            55,  17,  -45, -119, -86,  -40,  32,   53,    //
            -35, 71,  50,  -113, 30,   -40,  92,   64,    //
        };
        constexpr int KING_END[64] = {
            -108, -106, -90, -41, 1,   19,  0,   -41,  // A8, B8, ...
            -55,  29,   32,  22,  35,  62,  56,  33,   //
            15,   35,   39,  31,  39,  58,  64,  25,   //
            -4,   42,   46,  55,  45,  56,  46,  17,   //
            -14,  1,    41,  47,  49,  46,  24,  4,    //
            -30,  0,    24,  38,  43,  33,  15,  -4,   //
            -45,  -11,  17,  28,  28,  14,  -10, -37,  //
            -77,  -51,  -33, -9,  -44, -18, -53, -88,  // A1, B1, ...
        };
        const int* PST_MID[6] = {
            PAWN_MID,
            KNIGHT_MID,
            BISHOP_MID,
            ROOK_MID,
            QUEEN_MID,
            KING_MID,
        };
        const int* PST_END[6] = {
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
                PST[p][sq][0] = PST_MID[p][sq ^ 56];
                PST[p][sq][1] = PST_END[p][sq ^ 56];
                // flip for black
                PST[p + 6][sq][0] = -PST_MID[p][sq];
                PST[p + 6][sq][1] = -PST_END[p][sq];
            }
        }
    }
};
}  // namespace Raphael
