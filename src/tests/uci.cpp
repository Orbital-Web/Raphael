#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



TEST_SUITE("UCI") {
    TEST_CASE("from_move") {
        CHECK(uci::from_move(Move::make(Square::E2, Square::E4)) == "e2e4");

        CHECK(uci::from_move(Move::make(Square::E1, Square::F1)) == "e1f1");
        CHECK(uci::from_move(Move::make<Move::CASTLING>(Square::E1, Square::H1)) == "e1g1");
        CHECK(uci::from_move(Move::make<Move::CASTLING>(Square::E1, Square::A1)) == "e1c1");
        CHECK(uci::from_move(Move::make<Move::CASTLING>(Square::E8, Square::H8)) == "e8g8");
        CHECK(uci::from_move(Move::make<Move::CASTLING>(Square::E8, Square::A8)) == "e8c8");

        CHECK(
            uci::from_move(Move::make<Move::PROMOTION>(Square::B7, Square::C8, PieceType::KNIGHT))
            == "b7c8n"
        );
        CHECK(
            uci::from_move(Move::make<Move::PROMOTION>(Square::B7, Square::C8, PieceType::BISHOP))
            == "b7c8b"
        );
        CHECK(
            uci::from_move(Move::make<Move::PROMOTION>(Square::B7, Square::C8, PieceType::ROOK))
            == "b7c8r"
        );
        CHECK(
            uci::from_move(Move::make<Move::PROMOTION>(Square::B7, Square::C8, PieceType::QUEEN))
            == "b7c8q"
        );

        CHECK(uci::from_move(Move::make<Move::ENPASSANT>(Square::F5, Square::E6)) == "f5e6");
    }

    TEST_CASE("to_move") {
        Board board = Board("rnbqk3/pppp1pPp/8/1N2pP2/8/8/PPPPP1PP/R3K2R w KQq e6 0 1");

        CHECK(uci::to_move(board, "e1e2") == Move::make(Square::E1, Square::E2));
        CHECK(uci::to_move(board, "b5c7") == Move::make(Square::B5, Square::C7));

        CHECK(uci::to_move(board, "e1f1") == Move::make(Square::E1, Square::F1));
        CHECK(uci::to_move(board, "e1g1") == Move::make<Move::CASTLING>(Square::E1, Square::H1));
        CHECK(uci::to_move(board, "e1c1") == Move::make<Move::CASTLING>(Square::E1, Square::A1));

        CHECK(
            uci::to_move(board, "g7g8n")
            == Move::make<Move::PROMOTION>(Square::G7, Square::G8, PieceType::KNIGHT)
        );
        CHECK(
            uci::to_move(board, "g7g8b")
            == Move::make<Move::PROMOTION>(Square::G7, Square::G8, PieceType::BISHOP)
        );
        CHECK(
            uci::to_move(board, "g7g8r")
            == Move::make<Move::PROMOTION>(Square::G7, Square::G8, PieceType::ROOK)
        );
        CHECK(
            uci::to_move(board, "g7g8q")
            == Move::make<Move::PROMOTION>(Square::G7, Square::G8, PieceType::QUEEN)
        );

        CHECK(uci::to_move(board, "f5e6") == Move::make<Move::ENPASSANT>(Square::F5, Square::E6));

        board.set_fen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
        CHECK(uci::to_move(board, "e8g8") == Move::make<Move::CASTLING>(Square::E8, Square::H8));
        CHECK(uci::to_move(board, "e8c8") == Move::make<Move::CASTLING>(Square::E8, Square::A8));
    }
}
