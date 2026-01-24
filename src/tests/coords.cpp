#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



// https://github.com/Disservin/chess-library/blob/master/tests/coords.cpp
TEST_SUITE("File") {
    TEST_CASE("File operator int") {
        File f = File::A;
        CHECK(static_cast<int>(f) == 0);
    }

    TEST_CASE("File operator std::string") {
        File f = File::A;
        CHECK(static_cast<std::string>(f) == "a");

        f = File::B;
        CHECK(static_cast<std::string>(f) == "b");

        f = File::C;
        CHECK(static_cast<std::string>(f) == "c");

        f = File::D;
        CHECK(static_cast<std::string>(f) == "d");

        f = File::E;
        CHECK(static_cast<std::string>(f) == "e");

        f = File::F;
        CHECK(static_cast<std::string>(f) == "f");

        f = File::G;
        CHECK(static_cast<std::string>(f) == "g");

        f = File::H;
        CHECK(static_cast<std::string>(f) == "h");
    }
}

TEST_SUITE("Rank") {
    TEST_CASE("Rank operator int") {
        Rank r = Rank::R1;
        CHECK(static_cast<int>(r) == 0);
    }

    TEST_CASE("Rank operator std::string") {
        Rank r = Rank::R1;
        CHECK(static_cast<std::string>(r) == "1");

        r = Rank::R2;
        CHECK(static_cast<std::string>(r) == "2");

        r = Rank::R3;
        CHECK(static_cast<std::string>(r) == "3");

        r = Rank::R4;
        CHECK(static_cast<std::string>(r) == "4");

        r = Rank::R5;
        CHECK(static_cast<std::string>(r) == "5");

        r = Rank::R6;
        CHECK(static_cast<std::string>(r) == "6");

        r = Rank::R7;
        CHECK(static_cast<std::string>(r) == "7");

        r = Rank::R8;
        CHECK(static_cast<std::string>(r) == "8");
    }
}

TEST_SUITE("Square") {
    TEST_CASE("Square operator std::string") {
        Square s = Square::A1;
        CHECK(static_cast<std::string>(s) == "a1");

        s = Square::A2;
        CHECK(static_cast<std::string>(s) == "a2");

        s = Square::A3;
        CHECK(static_cast<std::string>(s) == "a3");

        s = Square::A4;
        CHECK(static_cast<std::string>(s) == "a4");

        s = Square::A5;
        CHECK(static_cast<std::string>(s) == "a5");

        s = Square::A6;
        CHECK(static_cast<std::string>(s) == "a6");

        s = Square::A7;
        CHECK(static_cast<std::string>(s) == "a7");

        s = Square::A8;
        CHECK(static_cast<std::string>(s) == "a8");

        s = Square::B1;
        CHECK(static_cast<std::string>(s) == "b1");

        s = Square::B2;
        CHECK(static_cast<std::string>(s) == "b2");

        s = Square::B3;
        CHECK(static_cast<std::string>(s) == "b3");

        s = Square::B4;
        CHECK(static_cast<std::string>(s) == "b4");

        s = Square::B5;
        CHECK(static_cast<std::string>(s) == "b5");

        s = Square::B6;
        CHECK(static_cast<std::string>(s) == "b6");

        s = Square::B7;
        CHECK(static_cast<std::string>(s) == "b7");

        s = Square::B8;
        CHECK(static_cast<std::string>(s) == "b8");

        s = Square::C1;
        CHECK(static_cast<std::string>(s) == "c1");

        s = Square::C2;
        CHECK(static_cast<std::string>(s) == "c2");

        s = Square::C3;
        CHECK(static_cast<std::string>(s) == "c3");
    }

    TEST_CASE("Square file()") {
        Square s = Square::A1;
        CHECK(s.file() == File::A);

        s = Square::B1;
        CHECK(s.file() == File::B);

        s = Square::C1;
        CHECK(s.file() == File::C);
    }

    TEST_CASE("Square rank()") {
        Square s = Square::A1;
        CHECK(s.rank() == Rank::R1);

        s = Square::A2;
        CHECK(s.rank() == Rank::R2);

        s = Square::A3;
        CHECK(s.rank() == Rank::R3);
    }

    TEST_CASE("Square ()") {
        Square s = Square(File::A, Rank::R1);
        CHECK(s == Square::A1);

        s = Square(File::B, Rank::R1);
        CHECK(s == Square::B1);

        s = Square(File::C, Rank::R1);
        CHECK(s == Square::C1);
    }

    TEST_CASE("Square ()") {
        Square s = Square("a1");
        CHECK(s == Square::A1);

        s = Square("b1");
        CHECK(s == Square::B1);

        s = Square("c1");
        CHECK(s == Square::C1);
    }

    TEST_CASE("is_light") {
        Square s = Square::A1;
        CHECK(!s.is_light());

        s = Square::B1;
        CHECK(s.is_light());

        s = Square::C1;
        CHECK(!s.is_light());
    }

    TEST_CASE("is_dark") {
        Square s = Square::A1;
        CHECK(s.is_dark());

        s = Square::B1;
        CHECK(!s.is_dark());

        s = Square::C1;
        CHECK(s.is_dark());
    }

    TEST_CASE("is_valid") {
        Square s = Square::A1;
        CHECK(s.is_valid());

        s = Square::B1;
        CHECK(s.is_valid());

        s = Square::C1;
        CHECK(s.is_valid());

        s = Square::NONE;
        CHECK(!s.is_valid());
    }

    TEST_CASE("Square same_color") {
        CHECK(Square::same_color(Square::A1, Square::A1));
        CHECK(!Square::same_color(Square::A1, Square::A2));
        CHECK(!Square::same_color(Square::A1, Square::B1));
        CHECK(Square::same_color(Square::A1, Square::B2));
    }

    TEST_CASE("Square value_distance") {
        CHECK(Square::value_distance(Square::A1, Square::A1) == 0);
        CHECK(Square::value_distance(Square::A1, Square::A2) == 8);
        CHECK(Square::value_distance(Square::A1, Square::B1) == 1);
        CHECK(Square::value_distance(Square::A1, Square::B2) == 9);
    }

    TEST_CASE("Square back_rank") {
        CHECK(Square(Square::A1).is_back_rank(Color::WHITE));
        CHECK(!Square(Square::A1).is_back_rank(Color::BLACK));
        CHECK(!Square(Square::B8).is_back_rank(Color::WHITE));
        CHECK(Square(Square::B8).is_back_rank(Color::BLACK));
    }

    TEST_CASE("Square flip/flipped") {
        CHECK(Square(Square::A1).flip() == Square::A8);
        CHECK(Square(Square::A2).flip() == Square::A7);
        CHECK(Square(Square::A3).flip() == Square::A6);

        CHECK(Square(Square::A1).flipped() == Square::A8);
        CHECK(Square(Square::A2).flipped() == Square::A7);
        CHECK(Square(Square::A3).flipped() == Square::A6);
    }

    TEST_CASE("Square relative_square") {
        CHECK(Square(Square::A1).relative(Color::WHITE) == Square::A1);
        CHECK(Square(Square::A1).relative(Color::BLACK) == Square::A8);
        CHECK(Square(Square::A2).relative(Color::WHITE) == Square::A2);
        CHECK(Square(Square::A2).relative(Color::BLACK) == Square::A7);
    }

    TEST_CASE("Square ep_square") {
        CHECK(Square(Square::A3).ep_square() == Square::A4);
        CHECK(Square(Square::A4).ep_square() == Square::A3);
        CHECK(Square(Square::A5).ep_square() == Square::A6);
        CHECK(Square(Square::A6).ep_square() == Square::A5);
    }

    TEST_CASE("Square castling_king_dest") {
        CHECK(Square::castling_king_dest(true, Color::WHITE) == Square::G1);
        CHECK(Square::castling_king_dest(false, Color::WHITE) == Square::C1);
        CHECK(Square::castling_king_dest(true, Color::BLACK) == Square::G8);
        CHECK(Square::castling_king_dest(false, Color::BLACK) == Square::C8);
    }

    TEST_CASE("Square castling_rook_dest") {
        CHECK(Square::castling_rook_dest(true, Color::WHITE) == Square::F1);
        CHECK(Square::castling_rook_dest(false, Color::WHITE) == Square::D1);
        CHECK(Square::castling_rook_dest(true, Color::BLACK) == Square::F8);
        CHECK(Square::castling_rook_dest(false, Color::BLACK) == Square::D8);
    }

    TEST_CASE("Square operator +Direction") {
        Square s = Square::B3;
        CHECK(s + Direction::NORTH == Square::B4);
        CHECK(s + Direction::SOUTH == Square::B2);
        CHECK(s + Direction::WEST == Square::A3);
        CHECK(s + Direction::EAST == Square::C3);
        CHECK(s + Direction::NORTH_WEST == Square::A4);
        CHECK(s + Direction::NORTH_EAST == Square::C4);
        CHECK(s + Direction::SOUTH_WEST == Square::A2);
        CHECK(s + Direction::SOUTH_EAST == Square::C2);
    }
}