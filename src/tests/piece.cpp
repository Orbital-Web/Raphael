#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



// https://github.com/Disservin/chess-library/blob/master/tests/piece.cpp
TEST_SUITE("PieceType") {
    TEST_CASE("operator std::string") {
        CHECK(static_cast<std::string>(PieceType(PieceType::PAWN)) == "p");
        CHECK(static_cast<std::string>(PieceType(PieceType::KNIGHT)) == "n");
        CHECK(static_cast<std::string>(PieceType(PieceType::BISHOP)) == "b");
        CHECK(static_cast<std::string>(PieceType(PieceType::ROOK)) == "r");
        CHECK(static_cast<std::string>(PieceType(PieceType::QUEEN)) == "q");
        CHECK(static_cast<std::string>(PieceType(PieceType::KING)) == "k");
        CHECK(static_cast<std::string>(PieceType(PieceType::NONE)) == " ");
    }
}

TEST_SUITE("Piece") {
    TEST_CASE("operator std::string") {
        CHECK(static_cast<std::string>(Piece(PieceType::PAWN, Color::WHITE)) == "P");
        CHECK(static_cast<std::string>(Piece(PieceType::KNIGHT, Color::WHITE)) == "N");
        CHECK(static_cast<std::string>(Piece(PieceType::BISHOP, Color::WHITE)) == "B");
        CHECK(static_cast<std::string>(Piece(PieceType::ROOK, Color::WHITE)) == "R");
        CHECK(static_cast<std::string>(Piece(PieceType::QUEEN, Color::WHITE)) == "Q");
        CHECK(static_cast<std::string>(Piece(PieceType::KING, Color::WHITE)) == "K");
        CHECK(static_cast<std::string>(Piece(PieceType::NONE, Color::WHITE)) == ".");

        CHECK(static_cast<std::string>(Piece(PieceType::PAWN, Color::BLACK)) == "p");
        CHECK(static_cast<std::string>(Piece(PieceType::KNIGHT, Color::BLACK)) == "n");
        CHECK(static_cast<std::string>(Piece(PieceType::BISHOP, Color::BLACK)) == "b");
        CHECK(static_cast<std::string>(Piece(PieceType::ROOK, Color::BLACK)) == "r");
        CHECK(static_cast<std::string>(Piece(PieceType::QUEEN, Color::BLACK)) == "q");
        CHECK(static_cast<std::string>(Piece(PieceType::KING, Color::BLACK)) == "k");
        CHECK(static_cast<std::string>(Piece(PieceType::NONE, Color::BLACK)) == ".");
    }

    TEST_CASE("operator int") {
        CHECK(static_cast<i32>(Piece(PieceType::PAWN, Color::WHITE)) == 0);
        CHECK(static_cast<i32>(Piece(PieceType::KNIGHT, Color::WHITE)) == 1);
        CHECK(static_cast<i32>(Piece(PieceType::BISHOP, Color::WHITE)) == 2);
        CHECK(static_cast<i32>(Piece(PieceType::ROOK, Color::WHITE)) == 3);
        CHECK(static_cast<i32>(Piece(PieceType::QUEEN, Color::WHITE)) == 4);
        CHECK(static_cast<i32>(Piece(PieceType::KING, Color::WHITE)) == 5);
        CHECK(static_cast<i32>(Piece(PieceType::NONE, Color::WHITE)) == 12);

        CHECK(static_cast<i32>(Piece(PieceType::PAWN, Color::BLACK)) == 6);
        CHECK(static_cast<i32>(Piece(PieceType::KNIGHT, Color::BLACK)) == 7);
        CHECK(static_cast<i32>(Piece(PieceType::BISHOP, Color::BLACK)) == 8);
        CHECK(static_cast<i32>(Piece(PieceType::ROOK, Color::BLACK)) == 9);
        CHECK(static_cast<i32>(Piece(PieceType::QUEEN, Color::BLACK)) == 10);
        CHECK(static_cast<i32>(Piece(PieceType::KING, Color::BLACK)) == 11);
        CHECK(static_cast<i32>(Piece(PieceType::NONE, Color::BLACK)) == 12);
    }

    TEST_CASE("color") {
        CHECK(Piece(PieceType::PAWN, Color::WHITE).color() == Color::WHITE);
        CHECK(Piece(PieceType::KNIGHT, Color::WHITE).color() == Color::WHITE);
        CHECK(Piece(PieceType::BISHOP, Color::WHITE).color() == Color::WHITE);
        CHECK(Piece(PieceType::ROOK, Color::WHITE).color() == Color::WHITE);
        CHECK(Piece(PieceType::QUEEN, Color::WHITE).color() == Color::WHITE);
        CHECK(Piece(PieceType::KING, Color::WHITE).color() == Color::WHITE);
        CHECK(Piece(PieceType::NONE, Color::WHITE).color() == Color::NONE);

        CHECK(Piece(PieceType::PAWN, Color::BLACK).color() == Color::BLACK);
        CHECK(Piece(PieceType::KNIGHT, Color::BLACK).color() == Color::BLACK);
        CHECK(Piece(PieceType::BISHOP, Color::BLACK).color() == Color::BLACK);
        CHECK(Piece(PieceType::ROOK, Color::BLACK).color() == Color::BLACK);
        CHECK(Piece(PieceType::QUEEN, Color::BLACK).color() == Color::BLACK);
        CHECK(Piece(PieceType::KING, Color::BLACK).color() == Color::BLACK);

        CHECK(Piece(PieceType::NONE, Color::BLACK).color() == Color::NONE);
    }

    TEST_CASE("type") {
        CHECK(Piece(Piece::WHITEPAWN).type() == PieceType::PAWN);
        CHECK(Piece(Piece::WHITEKING).type() == PieceType::KING);
        CHECK(Piece(Piece::BLACKPAWN).type() == PieceType::PAWN);
        CHECK(Piece(Piece::BLACKKING).type() == PieceType::KING);
        CHECK(Piece(Piece::NONE).type() == PieceType::NONE);
    }
}
