#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



// https://github.com/Disservin/chess-library/blob/master/tests/bitboard.cpp
TEST_SUITE("Bitboard") {
    TEST_CASE("lsb") {
        BitBoard b = 0x0000000000000001ULL;
        CHECK(b.lsb() == 0);

        b = 0x0000000000000002ULL;
        CHECK(b.lsb() == 1);

        b = 0x0000000000000004ULL;
        CHECK(b.lsb() == 2);
    }

    TEST_CASE("count") {
        BitBoard b = 0x0000000000000001ULL;
        CHECK(b.count() == 1);

        b = 0x0000000000000003ULL;
        CHECK(b.count() == 2);

        b = 0x0000000000000007ULL;
        CHECK(b.count() == 3);
    }

    TEST_CASE("poplsb") {
        BitBoard b = 0x0000000000000001ULL;

        CHECK(b.poplsb() == 0);
        CHECK(b == 0);

        b = 0x0000000000000003ULL;
        CHECK(b.poplsb() == 0);
        CHECK(b == 0x0000000000000002ULL);

        b = 0x0000000000000007ULL;
        CHECK(b.poplsb() == 0);
        CHECK(b == 0x0000000000000006ULL);
    }

    TEST_CASE("empty") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK(b.is_empty());

        b = 0x0000000000000001ULL;
        CHECK(!b.is_empty());

        b = 0x0000000000000003ULL;
        CHECK(!b.is_empty());
    }

    TEST_CASE("operator==") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK(b == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        CHECK(b == 0x0000000000000001ULL);

        b = 0x0000000000000003ULL;
        CHECK(b == 0x0000000000000003ULL);
    }

    TEST_CASE("operator!=") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK(b != 0x0000000000000001ULL);

        b = 0x0000000000000001ULL;
        CHECK(b != 0x0000000000000002ULL);

        b = 0x0000000000000003ULL;
        CHECK(b != 0x0000000000000004ULL);
    }

    TEST_CASE("operator&") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK((b & 0x0000000000000000ULL) == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        CHECK((b & 0x0000000000000001ULL) == 0x0000000000000001ULL);

        b = 0x0000000000000003ULL;
        CHECK((b & 0x0000000000000003ULL) == 0x0000000000000003ULL);
    }

    TEST_CASE("operator|") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK((b | 0x0000000000000000ULL) == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        CHECK((b | 0x0000000000000001ULL) == 0x0000000000000001ULL);

        b = 0x0000000000000003ULL;
        CHECK((b | 0x0000000000000003ULL) == 0x0000000000000003ULL);
    }

    TEST_CASE("operator^") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK((b ^ 0x0000000000000000ULL) == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        CHECK((b ^ 0x0000000000000001ULL) == 0x0000000000000000ULL);

        b = 0x0000000000000003ULL;
        CHECK((b ^ 0x0000000000000003ULL) == 0x0000000000000000ULL);
    }

    TEST_CASE("operator<<") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK((b << 0) == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        CHECK((b << 1) == 0x0000000000000002ULL);

        b = 0x0000000000000003ULL;
        CHECK((b << 2) == 0x000000000000000cULL);
    }

    TEST_CASE("operator>>") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK((b >> 0) == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        CHECK((b >> 1) == 0x0000000000000000ULL);

        b = 0x0000000000000003ULL;
        CHECK((b >> 2) == 0x0000000000000000ULL);
    }

    TEST_CASE("operator&=") {
        BitBoard b = 0x0000000000000000ULL;

        b &= 0x0000000000000000ULL;
        CHECK(b == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        b &= 0x0000000000000001ULL;
        CHECK(b == 0x0000000000000001ULL);

        b = 0x0000000000000003ULL;
        b &= 0x0000000000000003ULL;
        CHECK(b == 0x0000000000000003ULL);
    }

    TEST_CASE("operator|=") {
        BitBoard b = 0x0000000000000000ULL;

        b |= 0x0000000000000000ULL;
        CHECK(b == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        b |= 0x0000000000000001ULL;
        CHECK(b == 0x0000000000000001ULL);

        b = 0x0000000000000003ULL;
        b |= 0x0000000000000003ULL;
        CHECK(b == 0x0000000000000003ULL);
    }

    TEST_CASE("operator^=") {
        BitBoard b = 0x0000000000000000ULL;

        b ^= 0x0000000000000000ULL;
        CHECK(b == 0x0000000000000000ULL);

        b = 0x0000000000000001ULL;
        b ^= 0x0000000000000001ULL;
        CHECK(b == 0x0000000000000000ULL);

        b = 0x0000000000000003ULL;
        b ^= 0x0000000000000003ULL;
        CHECK(b == 0x0000000000000000ULL);
    }

    TEST_CASE("operator~") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK(~b == 0xffffffffffffffffULL);

        b = 0x0000000000000001ULL;
        CHECK(~b == 0xfffffffffffffffeULL);

        b = 0x0000000000000003ULL;
        CHECK(~b == 0xfffffffffffffffcULL);
    }

    TEST_CASE("set") {
        BitBoard b = 0x0000000000000000ULL;

        b.set(0);
        CHECK(b == 0x0000000000000001ULL);

        b.set(1);
        CHECK(b == 0x0000000000000003ULL);

        b.set(2);
        CHECK(b == 0x0000000000000007ULL);
    }

    TEST_CASE("is_set") {
        BitBoard b = 0x0000000000000000ULL;
        CHECK(!b.is_set(0));

        b.set(0);
        CHECK(b.is_set(0));

        b.set(1);
        CHECK(b.is_set(1));
    }

    TEST_CASE("unset") {
        BitBoard b = 0x0000000000000000ULL;

        b.unset(0);
        CHECK(b == 0x0000000000000000ULL);

        b.set(0);
        b.unset(0);
        CHECK(b == 0x0000000000000000ULL);

        b.set(1);
        b.unset(1);
        CHECK(b == 0x0000000000000000ULL);
    }

    TEST_CASE("clear") {
        BitBoard b = 0x0000000000000000ULL;

        b.clear();
        CHECK(b == 0x0000000000000000ULL);

        b.set(0);
        b.clear();
        CHECK(b == 0x0000000000000000ULL);

        b.set(1);
        b.clear();
        CHECK(b == 0x0000000000000000ULL);
    }

    TEST_CASE("from_square") {
        BitBoard b = BitBoard::from_square(Square::A1);
        CHECK(b == 0x0000000000000001ULL);

        b = BitBoard::from_square(Square::B1);
        CHECK(b == 0x0000000000000002ULL);

        b = BitBoard::from_square(Square::C1);
        CHECK(b == 0x0000000000000004ULL);
    }
}
