#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



// https://github.com/Disservin/chess-library/blob/master/tests/board.cpp
TEST_SUITE("Board") {
    TEST_CASE("Board Fen Get/Set") {
        Board board = Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
        CHECK(
            board.get_fen()
            == "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
        );

        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        CHECK(board.get_fen() == "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");

        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 3 24");
        CHECK(board.get_fen() == "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 3 24");

        board.set_fen("8/2k2p2/8/6P1/1Pp5/8/6K1/8 b - b3 0 1");
        CHECK(board.get_fen() == "8/2k2p2/8/6P1/1Pp5/8/6K1/8 b - b3 0 1");

        // invalid castling rights
        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w KQkq - 0 1");
        CHECK(board.get_fen() == "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");

        // invalid enpassant 1 (no pawns)
        board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w KQkq e3 0 1");
        CHECK(board.get_fen() == "4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");

        // invalid enpassant 2 (pinned)
        board.set_fen("4k1n1/pppp1ppp/8/8/4pP2/8/PPPPQ1PP/4K3 b - f3 0 1");
        CHECK(board.get_fen() == "4k1n1/pppp1ppp/8/8/4pP2/8/PPPPQ1PP/4K3 b - - 0 1");

        // missing halfmoves fullmoves
        board.set_fen("r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq -");
        CHECK(
            board.get_fen() == "r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1"
        );
    }

    TEST_CASE("Board make_move") {
        SUBCASE("makeMove") {
            Board board = Board();
            board = board.make_move(Move::make(Square::E2, Square::E4));

            CHECK(board.at(Square::E2) == Piece::NONE);
            CHECK(board.at(Square::E4) == Piece::WHITEPAWN);

            board = board.make_move(Move::make(Square::E7, Square::E5));

            CHECK(board.at(Square::E7) == Piece::NONE);
            CHECK(board.at(Square::E5) == Piece::BLACKPAWN);
        }

        SUBCASE("make_null_move") {
            Board board = Board();
            Board newboard = board.make_nullmove();

            CHECK(newboard.hash() != board.hash());
            CHECK(newboard.stm() == Color::BLACK);
        }
    }

    TEST_CASE("Board is_kingpawn") {
        Board board = Board("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        CHECK(board.is_kingpawn(board.stm()));
        CHECK(board.is_kingpawn(Color::WHITE));
        CHECK(!board.is_kingpawn(Color::BLACK));
    }

    TEST_CASE("Board HalfMove Draw") {
        SUBCASE("isHalfMoveDraw") {
            Board board = Board("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
            CHECK(board.is_halfmovedraw() == false);

            board.set_fen("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 99 1");
            CHECK(board.is_halfmovedraw() == false);
        }

        SUBCASE("isHalfMoveDraw True") {
            Board board = Board("4k1n1/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 100 1");
            CHECK(board.is_halfmovedraw());
        }

        SUBCASE("isHalfMoveDraw True and Checkmate") {
            Board board = Board("7k/8/5B1K/8/8/1B6/8/8 b - - 100 1");
            CHECK(board.is_halfmovedraw());  // no checkmate detection in is_halfmovedraw
        }
    }

    TEST_CASE("Board Insufficient Material") {
        SUBCASE("Insufficient Material Two White Light Bishops") {
            Board board = Board("8/6k1/8/8/4B3/3B4/8/1K6 w - - 0 1");
            CHECK(board.is_insufficientmaterial());
        }

        SUBCASE("Insufficient Material Two Black Light Bishops") {
            Board board = Board("8/6k1/8/8/4b3/3b4/K7/8 w - - 0 1");
            CHECK(board.is_insufficientmaterial());
        }

        SUBCASE("Insufficient Material White Bishop") {
            Board board = Board("8/7k/8/8/3B4/8/8/1K6 w - - 0 1");
            CHECK(board.is_insufficientmaterial());
        }

        SUBCASE("Insufficient Material Black Bishop") {
            Board board = Board("8/7k/8/8/3b4/8/8/1K6 w - - 0 1");
            CHECK(board.is_insufficientmaterial());
        }

        SUBCASE("Insufficient Material White Knight") {
            Board board = Board("8/7k/8/8/3N4/8/8/1K6 w - - 0 1");
            CHECK(board.is_insufficientmaterial());
        }

        SUBCASE("Insufficient Material Black Knight") {
            Board board = Board("8/7k/8/8/3n4/8/8/1K6 w - - 0 1");
            CHECK(board.is_insufficientmaterial());
        }

        SUBCASE("Insufficient Material White Light Bishop and White Dark Bishop") {
            Board board = Board("8/7k/8/8/3BB3/8/8/1K6 w - - 0 1");
            CHECK(board.is_insufficientmaterial() == false);
        }
    }

    TEST_CASE("Board Capture") {
        SUBCASE("is_capture False") {
            Board board = Board();
            auto mv = Move::make(Square::E2, Square::E4);
            CHECK(board.is_capture(mv) == false);
            CHECK(board.get_captured(mv) == Piece::NONE);
        }

        SUBCASE("is_capture False Castling") {
            Board board = Board("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E1, Square::H1);
            CHECK(board.is_capture(mv) == false);
            CHECK(board.get_captured(mv) == Piece::NONE);

            mv = Move::make<Move::CASTLING>(Square::E8, Square::A8);
            CHECK(board.is_capture(mv) == false);
            CHECK(board.get_captured(mv) == Piece::NONE);
        }

        SUBCASE("is_capture False Promotion") {
            Board board = Board("1k6/6P1/5K2/8/8/8/8/8 w - - 0 1");
            auto mv = Move::make<Move::PROMOTION>(Square::G7, Square::G8, PieceType::QUEEN);
            CHECK(board.is_capture(mv) == false);
            CHECK(board.get_captured(mv) == Piece::NONE);
        }

        SUBCASE("is_capture True") {
            Board board = Board("8/8/8/2nk4/4r3/5P2/2B3K1/8 w - - 0 1");
            auto mv = Move::make(Square::F3, Square::E4);
            CHECK(board.is_capture(mv) == true);
            CHECK(board.get_captured(mv) == Piece::BLACKROOK);
            board = board.make_move(mv);

            mv = Move::make(Square::C5, Square::E4);
            CHECK(board.is_capture(mv) == true);
            CHECK(board.get_captured(mv) == Piece::WHITEPAWN);
            board = board.make_move(mv);

            mv = Move::make(Square::C2, Square::E4);
            CHECK(board.is_capture(mv) == true);
            CHECK(board.get_captured(mv) == Piece::BLACKKNIGHT);
            board = board.make_move(mv);

            mv = Move::make(Square::D5, Square::E4);
            CHECK(board.is_capture(mv) == true);
            CHECK(board.get_captured(mv) == Piece::WHITEBISHOP);
        }

        SUBCASE("is_capture True Enpassant") {
            {
                Board board = Board("8/2k2p2/8/6P1/1Pp5/8/6K1/8 b - b3 0 1");
                auto mv = Move::make<Move::ENPASSANT>(Square::C4, Square::B3);
                CHECK(board.is_capture(mv) == true);
                CHECK(board.get_captured(mv) == Piece::WHITEPAWN);
            }
            {
                Board board = Board("8/2k5/8/5pP1/8/1p6/7K/8 w - f6 0 3");
                auto mv = Move::make<Move::ENPASSANT>(Square::G5, Square::F6);
                CHECK(board.is_capture(mv) == true);
                CHECK(board.get_captured(mv) == Piece::BLACKPAWN);
            }
        }

        SUBCASE("is_capture True Promotion") {
            Board board = Board("1k3n2/6P1/5K2/8/8/8/8/8 w - - 0 1");
            auto mv = Move::make<Move::PROMOTION>(Square::G7, Square::F8, PieceType::ROOK);
            CHECK(board.is_capture(mv) == true);
            CHECK(board.get_captured(mv) == Piece::BLACKKNIGHT);
        }
    }
}
