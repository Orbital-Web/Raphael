#pragma once
#include <chess/bitboard.h>
#ifdef CHESS_USE_PEXT
    #include <immintrin.h>
#endif



namespace chess {
class Attacks {
private:
#ifdef CHESS_USE_PEXT
    struct AttackEntry {
        BitBoard mask;      // mask
        BitBoard* attacks;  // BR_ATTACKS + offset
    };

    static inline BitBoard BISHOP_ATTACKS[5248] = {};
    static inline BitBoard ROOK_ATTACKS[102400] = {};
#else
    struct AttackEntry {
        u64 magic;          // black magic
        BitBoard negmask;   // negated mask
        BitBoard* attacks;  // BR_ATTACKS + offset
    };

    struct Magic {
        u64 magic;
        u32 offset;
    };

    static inline BitBoard BR_ATTACKS[88507] = {};

    // black magics found by Volker Annuss https://talkchess.com/forum/viewtopic.php?t=64790
    static constexpr Magic BISHOP_MAGICS[64] = {
        {0x107ac08050500bffULL, 66157},
        {0x7fffdfdfd823fffdULL, 71730},
        {0x0400c00fe8000200ULL, 37781},
        {0x103f802004000000ULL, 21015},
        {0xc03fe00100000000ULL, 47590},
        {0x24c00bffff400000ULL, 835  },
        {0x0808101f40007f04ULL, 23592},
        {0x100808201ec00080ULL, 30599},
        {0xffa2feffbfefb7ffULL, 68776},
        {0x083e3ee040080801ULL, 19959},
        {0x040180bff7e80080ULL, 21783},
        {0x0440007fe0031000ULL, 64836},
        {0x2010007ffc000000ULL, 23417},
        {0x1079ffe000ff8000ULL, 66724},
        {0x7f83ffdfc03fff80ULL, 74542},
        {0x080614080fa00040ULL, 67266},
        {0x7ffe7fff817fcff9ULL, 26575},
        {0x7ffebfffa01027fdULL, 67543},
        {0x20018000c00f3c01ULL, 24409},
        {0x407e0001000ffb8aULL, 30779},
        {0x201fe000fff80010ULL, 17384},
        {0xffdfefffde39ffefULL, 18778},
        {0x7ffff800203fbfffULL, 65109},
        {0x7ff7fbfff8203fffULL, 20184},
        {0x000000fe04004070ULL, 38240},
        {0x7fff7f9fffc0eff9ULL, 16459},
        {0x7ffeff7f7f01f7fdULL, 17432},
        {0x3f6efbbf9efbffffULL, 81040},
        {0x0410008f01003ffdULL, 84946},
        {0x20002038001c8010ULL, 18276},
        {0x087ff038000fc001ULL, 8512 },
        {0x00080c0c00083007ULL, 78544},
        {0x00000080fc82c040ULL, 19974},
        {0x000000407e416020ULL, 23850},
        {0x00600203f8008020ULL, 11056},
        {0xd003fefe04404080ULL, 68019},
        {0x100020801800304aULL, 85965},
        {0x7fbffe700bffe800ULL, 80524},
        {0x107ff00fe4000f90ULL, 38221},
        {0x7f8fffcff1d007f8ULL, 64647},
        {0x0000004100f88080ULL, 61320},
        {0x00000020807c4040ULL, 67281},
        {0x00000041018700c0ULL, 79076},
        {0x0010000080fc4080ULL, 17115},
        {0x1000003c80180030ULL, 50718},
        {0x2006001cf00c0018ULL, 24659},
        {0xffffffbfeff80fdcULL, 38291},
        {0x000000101003f812ULL, 30605},
        {0x0800001f40808200ULL, 37759},
        {0x084000101f3fd208ULL, 4639 },
        {0x080000000f808081ULL, 21759},
        {0x0004000008003f80ULL, 67799},
        {0x08000001001fe040ULL, 22841},
        {0x085f7d8000200a00ULL, 66689},
        {0xfffffeffbfeff81dULL, 62548},
        {0xffbfffefefdff70fULL, 66597},
        {0x100000101ec10082ULL, 86749},
        {0x7fbaffffefe0c02fULL, 69558},
        {0x7f83fffffff07f7fULL, 61589},
        {0xfff1fffffff7ffc1ULL, 62533},
        {0x0878040000ffe01fULL, 64387},
        {0x005d00000120200aULL, 26581},
        {0x0840800080200fdaULL, 76355},
        {0x100000c05f582008ULL, 11140}
    };

    static constexpr Magic ROOK_MAGICS[64] = {
        {0x80280013ff84ffffULL, 10890},
        {0x5ffbfefdfef67fffULL, 56054},
        {0xffeffaffeffdffffULL, 67495},
        {0x003000900300008aULL, 72797},
        {0x0030018003500030ULL, 17179},
        {0x0020012120a00020ULL, 63978},
        {0x0030006000c00030ULL, 56650},
        {0xffa8008dff09fff8ULL, 15929},
        {0x7fbff7fbfbeafffcULL, 55905},
        {0x0000140081050002ULL, 26301},
        {0x0000180043800048ULL, 78100},
        {0x7fffe800021fffb8ULL, 86245},
        {0xffffcffe7fcfffafULL, 75228},
        {0x00001800c0180060ULL, 31661},
        {0xffffe7ff8fbfffe8ULL, 38053},
        {0x0000180030620018ULL, 37433},
        {0x00300018010c0003ULL, 74747},
        {0x0003000c0085ffffULL, 53847},
        {0xfffdfff7fbfefff7ULL, 70952},
        {0x7fc1ffdffc001fffULL, 49447},
        {0xfffeffdffdffdfffULL, 62629},
        {0x7c108007befff81fULL, 58996},
        {0x20408007bfe00810ULL, 36009},
        {0x0400800558604100ULL, 21230},
        {0x0040200010080008ULL, 51882},
        {0x0010020008040004ULL, 11841},
        {0xfffdfefff7fbfff7ULL, 25794},
        {0xfebf7dfff8fefff9ULL, 49689},
        {0xc00000ffe001ffe0ULL, 63400},
        {0x2008208007004007ULL, 33958},
        {0xbffbfafffb683f7fULL, 21991},
        {0x0807f67ffa102040ULL, 45618},
        {0x200008e800300030ULL, 70134},
        {0x0000008780180018ULL, 75944},
        {0x0000010300180018ULL, 68392},
        {0x4000008180180018ULL, 66472},
        {0x008080310005fffaULL, 23236},
        {0x4000188100060006ULL, 19067},
        {0xffffff7fffbfbfffULL, 0    },
        {0x0000802000200040ULL, 43566},
        {0x20000202ec002800ULL, 29810},
        {0xfffff9ff7cfff3ffULL, 65558},
        {0x000000404b801800ULL, 77684},
        {0x2000002fe03fd000ULL, 73350},
        {0xffffff6ffe7fcffdULL, 61765},
        {0xbff7efffbfc00fffULL, 49282},
        {0x000000100800a804ULL, 78840},
        {0xfffbffefa7ffa7feULL, 82904},
        {0x0000052800140028ULL, 24594},
        {0x00000085008a0014ULL, 9513 },
        {0x8000002b00408028ULL, 29012},
        {0x4000002040790028ULL, 27684},
        {0x7800002010288028ULL, 27901},
        {0x0000001800e08018ULL, 61477},
        {0x1890000810580050ULL, 25719},
        {0x2003d80000500028ULL, 50020},
        {0xfffff37eefefdfbeULL, 41547},
        {0x40000280090013c1ULL, 4750 },
        {0xbf7ffeffbffaf71fULL, 6014 },
        {0xfffdffff777b7d6eULL, 41529},
        {0xeeffffeff0080bfeULL, 84192},
        {0xafe0000fff780402ULL, 33433},
        {0xee73fffbffbb77feULL, 8555 },
        {0x0002000308482882ULL, 1009 }
    };
#endif

    // clang-format off
    // pre-calculated lookup table for pawn attacks
    static constexpr BitBoard PAWN_ATTACKS[2][64] = {
        // white pawn attacks
        {0x200, 0x500, 0xa00, 0x1400,
         0x2800, 0x5000, 0xa000, 0x4000,
         0x20000, 0x50000, 0xa0000, 0x140000,
         0x280000, 0x500000, 0xa00000, 0x400000,
         0x2000000, 0x5000000, 0xa000000, 0x14000000,
         0x28000000, 0x50000000, 0xa0000000, 0x40000000,
         0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
         0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
         0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
         0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
         0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
         0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000,
         0x200000000000000, 0x500000000000000, 0xa00000000000000, 0x1400000000000000,
         0x2800000000000000, 0x5000000000000000, 0xa000000000000000, 0x4000000000000000,
         0x0, 0x0, 0x0, 0x0,
         0x0, 0x0, 0x0, 0x0},
        // black pawn attacks
        {0x0, 0x0, 0x0, 0x0,
         0x0, 0x0, 0x0, 0x0,
         0x2, 0x5, 0xa, 0x14,
         0x28, 0x50, 0xa0, 0x40,
         0x200, 0x500, 0xa00, 0x1400,
         0x2800, 0x5000, 0xa000, 0x4000,
         0x20000, 0x50000, 0xa0000, 0x140000,
         0x280000, 0x500000, 0xa00000, 0x400000,
         0x2000000, 0x5000000, 0xa000000, 0x14000000,
         0x28000000, 0x50000000, 0xa0000000, 0x40000000,
         0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
         0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
         0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
         0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
         0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
         0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000}
    };
    // clang-format on

    // pre-calculated lookup table for knight attacks
    static constexpr BitBoard KNIGHT_ATTACKS[64]
        = {0x0000000000020400, 0x0000000000050800, 0x00000000000A1100, 0x0000000000142200,
           0x0000000000284400, 0x0000000000508800, 0x0000000000A01000, 0x0000000000402000,
           0x0000000002040004, 0x0000000005080008, 0x000000000A110011, 0x0000000014220022,
           0x0000000028440044, 0x0000000050880088, 0x00000000A0100010, 0x0000000040200020,
           0x0000000204000402, 0x0000000508000805, 0x0000000A1100110A, 0x0000001422002214,
           0x0000002844004428, 0x0000005088008850, 0x000000A0100010A0, 0x0000004020002040,
           0x0000020400040200, 0x0000050800080500, 0x00000A1100110A00, 0x0000142200221400,
           0x0000284400442800, 0x0000508800885000, 0x0000A0100010A000, 0x0000402000204000,
           0x0002040004020000, 0x0005080008050000, 0x000A1100110A0000, 0x0014220022140000,
           0x0028440044280000, 0x0050880088500000, 0x00A0100010A00000, 0x0040200020400000,
           0x0204000402000000, 0x0508000805000000, 0x0A1100110A000000, 0x1422002214000000,
           0x2844004428000000, 0x5088008850000000, 0xA0100010A0000000, 0x4020002040000000,
           0x0400040200000000, 0x0800080500000000, 0x1100110A00000000, 0x2200221400000000,
           0x4400442800000000, 0x8800885000000000, 0x100010A000000000, 0x2000204000000000,
           0x0004020000000000, 0x0008050000000000, 0x00110A0000000000, 0x0022140000000000,
           0x0044280000000000, 0x0088500000000000, 0x0010A00000000000, 0x0020400000000000};

    // pre-calculated lookup table for king attacks
    static constexpr BitBoard KING_ATTACKS[64]
        = {0x0000000000000302, 0x0000000000000705, 0x0000000000000E0A, 0x0000000000001C14,
           0x0000000000003828, 0x0000000000007050, 0x000000000000E0A0, 0x000000000000C040,
           0x0000000000030203, 0x0000000000070507, 0x00000000000E0A0E, 0x00000000001C141C,
           0x0000000000382838, 0x0000000000705070, 0x0000000000E0A0E0, 0x0000000000C040C0,
           0x0000000003020300, 0x0000000007050700, 0x000000000E0A0E00, 0x000000001C141C00,
           0x0000000038283800, 0x0000000070507000, 0x00000000E0A0E000, 0x00000000C040C000,
           0x0000000302030000, 0x0000000705070000, 0x0000000E0A0E0000, 0x0000001C141C0000,
           0x0000003828380000, 0x0000007050700000, 0x000000E0A0E00000, 0x000000C040C00000,
           0x0000030203000000, 0x0000070507000000, 0x00000E0A0E000000, 0x00001C141C000000,
           0x0000382838000000, 0x0000705070000000, 0x0000E0A0E0000000, 0x0000C040C0000000,
           0x0003020300000000, 0x0007050700000000, 0x000E0A0E00000000, 0x001C141C00000000,
           0x0038283800000000, 0x0070507000000000, 0x00E0A0E000000000, 0x00C040C000000000,
           0x0302030000000000, 0x0705070000000000, 0x0E0A0E0000000000, 0x1C141C0000000000,
           0x3828380000000000, 0x7050700000000000, 0xE0A0E00000000000, 0xC040C00000000000,
           0x0203000000000000, 0x0507000000000000, 0x0A0E000000000000, 0x141C000000000000,
           0x2838000000000000, 0x5070000000000000, 0xA0E0000000000000, 0x40C0000000000000};

    // pre-calculated lookup table for bishop attacks
    static inline AttackEntry BISHOP_TABLE[64] = {};

    // pre-calculated lookup table for rook attacks
    static inline AttackEntry ROOK_TABLE[64] = {};

    // pre-calculated lookup table of ray between squares from (exclusive) and to (inclusive)
    static const MultiArray<BitBoard, 64, 64> SQUARES_BETWEEN;


public:
    template <Color::underlying color>
    [[nodiscard]] static BitBoard pawn_left(BitBoard pawns) {
        constexpr auto dir = relative_direction(Direction::UP_LEFT, color);
        return pawns.shifted<dir>();
    }

    template <Color::underlying color>
    [[nodiscard]] static BitBoard pawn_right(BitBoard pawns) {
        constexpr auto dir = relative_direction(Direction::UP_RIGHT, color);
        return pawns.shifted<dir>();
    }

    [[nodiscard]] static BitBoard pawn(Square sq, Color color) { return PAWN_ATTACKS[color][sq]; }

    [[nodiscard]] static BitBoard knight(Square sq) { return KNIGHT_ATTACKS[sq]; }

    [[nodiscard]] static BitBoard bishop(Square sq, BitBoard occupied) {
        const auto& entry = BISHOP_TABLE[sq];
#ifdef CHESS_USE_PEXT
        u64 index = _pext_u64(static_cast<u64>(occupied), static_cast<u64>(entry.mask));
#else
        u64 index = (static_cast<u64>(occupied | entry.negmask) * entry.magic) >> 55;
#endif
        return entry.attacks[index];
    }

    [[nodiscard]] static BitBoard rook(Square sq, BitBoard occupied) {
        const auto& entry = ROOK_TABLE[sq];
#ifdef CHESS_USE_PEXT
        u64 index = _pext_u64(static_cast<u64>(occupied), static_cast<u64>(entry.mask));
#else
        u64 index = (static_cast<u64>(occupied | entry.negmask) * entry.magic) >> 52;
#endif
        return entry.attacks[index];
    }

    [[nodiscard]] static BitBoard queen(Square sq, BitBoard occupied) {
        return bishop(sq, occupied) | rook(sq, occupied);
    }

    [[nodiscard]] static BitBoard king(Square sq) { return KING_ATTACKS[sq]; }

    [[nodiscard]] static BitBoard between(Square sq1, Square sq2) {
        return SQUARES_BETWEEN[sq1][sq2];
    }

private:
    static void init_attacks() {
#ifdef CHESS_USE_PEXT
        i32 bishop_offset = 0;
        i32 rook_offset = 0;
#endif

        for (Square sq = Square::A1; sq <= Square::H8; sq++) {
            const BitBoard edges
                = ((BitBoard(BitBoard::RANK1 | BitBoard::RANK8)) & ~BitBoard::from_rank(sq.rank()))
                  | ((BitBoard(BitBoard::FILEA | BitBoard::FILEH))
                     & ~BitBoard::from_file(sq.file()));

            // bishop
            BitBoard mask = get_slider_attacks<false>(sq, 0) & ~edges;
#ifdef CHESS_USE_PEXT
            BISHOP_TABLE[sq].mask = mask;
            BitBoard* attacks = &BISHOP_ATTACKS[bishop_offset];
            BISHOP_TABLE[sq].attacks = attacks;
            bishop_offset += (u64(1) << mask.count());
#else
            BISHOP_TABLE[sq].magic = BISHOP_MAGICS[sq].magic;
            BISHOP_TABLE[sq].negmask = ~mask;
            BitBoard* attacks = &BR_ATTACKS[BISHOP_MAGICS[sq].offset];
            BISHOP_TABLE[sq].attacks = attacks;
#endif

            BitBoard subset = 0;
            do {
#ifdef CHESS_USE_PEXT
                u64 index = _pext_u64(static_cast<u64>(subset), static_cast<u64>(mask));
#else
                u64 index = (static_cast<u64>(subset | ~mask) * BISHOP_MAGICS[sq].magic) >> 55;
#endif
                attacks[index] = get_slider_attacks<false>(sq, subset);
                subset = (subset - mask) & mask;
            } while (subset);


            // rook
            mask = get_slider_attacks<true>(sq, 0) & ~edges;
#ifdef CHESS_USE_PEXT
            ROOK_TABLE[sq].mask = mask;
            attacks = &ROOK_ATTACKS[rook_offset];
            ROOK_TABLE[sq].attacks = attacks;
            rook_offset += (u64(1) << mask.count());
#else
            ROOK_TABLE[sq].magic = ROOK_MAGICS[sq].magic;
            ROOK_TABLE[sq].negmask = ~mask;
            attacks = &BR_ATTACKS[ROOK_MAGICS[sq].offset];
            ROOK_TABLE[sq].attacks = attacks;
#endif

            subset = 0;
            do {
#ifdef CHESS_USE_PEXT
                u64 index = _pext_u64(static_cast<u64>(subset), static_cast<u64>(mask));
#else
                u64 index = (static_cast<u64>(subset | ~mask) * ROOK_MAGICS[sq].magic) >> 52;
#endif
                attacks[index] = get_slider_attacks<true>(sq, subset);
                subset = (subset - mask) & mask;
            } while (subset);
        }
    }

    template <bool is_rook>
    [[nodiscard]] static BitBoard get_slider_attacks(Square sq, BitBoard occupied) {
        static constexpr int dirs[2][4][2] = {
            {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}}, // bishops
            {{1, 0}, {0, -1}, {-1, 0},  {0, 1} }  // rooks
        };

        i32 f0 = sq.file();
        i32 r0 = sq.rank();

        BitBoard attacks = 0;
        for (i32 i = 0; i < 4; i++) {
            const i32 df = dirs[is_rook][i][0];
            const i32 dr = dirs[is_rook][i][1];

            i32 f;
            i32 r;
            for (f = f0 + df, r = r0 + dr; f >= 0 && f < 8 && r >= 0 && r < 8; f += df, r += dr) {
                const auto index = Square(static_cast<File>(f), static_cast<Rank>(r));
                attacks.set(index);
                if (occupied.is_set(index)) break;
            }
        }

        return attacks;
    }
};


inline const MultiArray<BitBoard, 64, 64> Attacks::SQUARES_BETWEEN = [] {
    init_attacks();

    MultiArray<BitBoard, 64, 64> squares_between{};
    for (Square sq1 = Square::A1; sq1 <= Square::H8; sq1++) {
        const BitBoard sq1bb = BitBoard::from_square(sq1);

        for (Square sq2 = Square::A1; sq2 <= Square::H8; sq2++) {
            const BitBoard sq2bb = BitBoard::from_square(sq2);

            const bool bishop_sees = bishop(sq1, 0).is_set(sq2);
            if (bishop_sees) squares_between[sq1][sq2] = bishop(sq1, sq2bb) & bishop(sq2, sq1bb);

            const bool rook_sees = rook(sq1, 0).is_set(sq2);
            if (rook_sees) squares_between[sq1][sq2] = rook(sq1, sq2bb) & rook(sq2, sq1bb);

            squares_between[sq1][sq2].set(sq2);
        }
    }
    return squares_between;
}();
}  // namespace chess
