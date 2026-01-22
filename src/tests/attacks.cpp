#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



TEST_SUITE("Attacks") {
    TEST_CASE("Pawn Left") {
        BitBoard pawns = 0x40000010012c00ULL;
        CHECK(Attacks::pawn_left<Color::WHITE>(pawns) == 0x2000000800160000ULL);
        CHECK(Attacks::pawn_left<Color::BLACK>(pawns) == 0x800000200258ULL);
    }

    TEST_CASE("Pawn Right") {
        BitBoard pawns = 0x40000010012c00ULL;
        CHECK(Attacks::pawn_right<Color::WHITE>(pawns) == 0x8000002002580000ULL);
        CHECK(Attacks::pawn_right<Color::BLACK>(pawns) == 0x200000080016ULL);
    }

    TEST_CASE("Pawn") {
        CHECK(Attacks::pawn(Square::D2, Color::WHITE) == 0x140000ULL);
        CHECK(Attacks::pawn(Square::D2, Color::BLACK) == 0x14ULL);

        CHECK(Attacks::pawn(Square::H6, Color::WHITE) == 0x40000000000000ULL);
        CHECK(Attacks::pawn(Square::H6, Color::BLACK) == 0x4000000000ULL);
    }

    TEST_CASE("Knight") {
        CHECK(Attacks::knight(Square::A1) == 0x20400ULL);
        CHECK(Attacks::knight(Square::D4) == 0x142200221400ULL);
        CHECK(Attacks::knight(Square::F7) == 0x8800885000000000ULL);
    }

    TEST_CASE("Bishop") {
        BitBoard occ = 0x1ULL;
        CHECK(Attacks::bishop(Square::A1, occ) == 0x8040201008040200ULL);

        occ = 0x8000000ULL;
        CHECK(Attacks::bishop(Square::A1, occ) == 0x8040200ULL);

        occ = 0x220000e200001ULL;
        CHECK(Attacks::bishop(Square::A1, occ) == 0x8040200ULL);

        occ = 0x301040002280000ULL;
        CHECK(Attacks::bishop(Square::A8, occ) == 0x2040000000000ULL);

        occ = 0ULL;
        CHECK(Attacks::bishop(Square::A8, occ) == 0x2040810204080ULL);

        occ = 0x40804220080ULL;
        CHECK(Attacks::bishop(Square::D5, occ) == 0x4020140014200000ULL);

        occ = 0x41020d3c0c260090ULL;
        CHECK(Attacks::bishop(Square::D5, occ) == 0x4020140014200000ULL);
    }

    TEST_CASE("Rook") {
        BitBoard occ = 0ULL;
        CHECK(Attacks::rook(Square::A1, occ) == 0x1010101010101feULL);

        occ = 0x8204a0452285200ULL;
        CHECK(Attacks::rook(Square::A1, occ) == 0x1010101010101feULL);

        occ = 0x220ff0005684221ULL;
        CHECK(Attacks::rook(Square::A1, occ) == 0x101013eULL);

        occ = 0x80000000000000c0ULL;
        CHECK(Attacks::rook(Square::H1, occ) == 0x8080808080808040ULL);

        occ = 0x104e001000ULL;
        CHECK(Attacks::rook(Square::E4, occ) == 0x1068101000ULL);

        occ = 0x4804105e245400ULL;
        CHECK(Attacks::rook(Square::E4, occ) == 0x1068101000ULL);

        occ = 0x4010920400001000ULL;
        CHECK(Attacks::rook(Square::E6, occ) == 0x10ee1010101000ULL);
    }

    TEST_CASE("Queen") {
        BitBoard occ = 0ULL;
        CHECK(Attacks::queen(Square::A1, occ) == 0x81412111090503feULL);

        occ = 0x200001000090ULL;
        CHECK(Attacks::queen(Square::A1, occ) == 0x20100905031eULL);

        occ = 0xc0522805a1823c80ULL;
        CHECK(Attacks::queen(Square::A1, occ) == 0x2010090503feULL);

        occ = 0x800200000008080ULL;
        CHECK(Attacks::queen(Square::H8, occ) == 0x78c0a08080808000ULL);

        occ = 0x4000110405001400ULL;
        CHECK(Attacks::queen(Square::C4, occ) == 0x110efb0e1500ULL);

        occ = 0x42005f4405401c25ULL;
        CHECK(Attacks::queen(Square::C4, occ) == 0x110efb0e1500ULL);

        occ = 0x3080248542e0a422ULL;
        CHECK(Attacks::queen(Square::F5, occ) == 0x48870dc70280400ULL);
    }

    TEST_CASE("King") {
        CHECK(Attacks::king(Square::A1) == 0x302ULL);
        CHECK(Attacks::king(Square::D3) == 0x1c141c00ULL);
        CHECK(Attacks::king(Square::H4) == 0xc040c00000ULL);
    }
}
