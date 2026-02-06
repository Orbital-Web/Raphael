#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



// https://github.com/Disservin/chess-library/blob/master/tests/move.cpp
TEST_SUITE("Move") {
    TEST_CASE("from") {
        CHECK(Move::make(Square::A1, Square::A2).from() == Square::A1);
        CHECK(Move::make(Square::H8, Square::H1).from() == Square::H8);
    }

    TEST_CASE("to") {
        CHECK(Move::make(Square::A1, Square::A2).to() == Square::A2);
        CHECK(Move::make(Square::H8, Square::H1).to() == Square::H1);
    }

    TEST_CASE("type") {
        CHECK(Move::make(Square::A1, Square::A2).type() == Move::NORMAL);
        CHECK(Move::make<Move::PROMOTION>(Square::H7, Square::H8).type() == Move::PROMOTION);
        CHECK(Move::make<Move::ENPASSANT>(Square::D5, Square::C6).type() == Move::ENPASSANT);
        CHECK(Move::make<Move::CASTLING>(Square::E8, Square::H8).type() == Move::CASTLING);
    }

    TEST_CASE("promotion_type") {
        CHECK(
            Move::make<Move::PROMOTION>(Square::A1, Square::A2, PieceType::BISHOP).promotion_type()
            == PieceType::BISHOP
        );
        CHECK(
            Move::make<Move::PROMOTION>(Square::H7, Square::H8, PieceType::KNIGHT).promotion_type()
            == PieceType::KNIGHT
        );
        CHECK(
            Move::make<Move::PROMOTION>(Square::D5, Square::C6, PieceType::ROOK).promotion_type()
            == PieceType::ROOK
        );
        CHECK(
            Move::make<Move::PROMOTION>(Square::E8, Square::H8, PieceType::QUEEN).promotion_type()
            == PieceType::QUEEN
        );
    }

    TEST_CASE("operator bool") {
        CHECK(bool(Move(Move::NO_MOVE)) == false);
        CHECK(bool(Move::make(Square::E2, Square::E4)) == true);
    }
}

TEST_SUITE("MoveList") {
    TEST_CASE("add") {
        MoveList<ScoredMove> moves;
        auto mv = Move::make(Square::A1, Square::A2);
        moves.push({.move = mv});

        CHECK(moves.size() == 1);
        CHECK(moves[0].move == mv);
        CHECK(moves.empty() == false);
    }

    TEST_CASE("pop") {
        MoveList<Move> moves;
        auto mv = Move::make(Square::A1, Square::A2);
        moves.push(mv);

        CHECK(moves.size() == 1);
        CHECK(moves.pop() == mv);
        CHECK(moves.empty() == true);
    }

    TEST_CASE("clear") {
        MoveList<ScoredMove> moves;
        auto mv = Move::make(Square::A1, Square::A2);
        moves.push({.move = mv});

        moves.clear();
        CHECK(moves.size() == 0);
        CHECK(moves.empty() == true);
    }
}
