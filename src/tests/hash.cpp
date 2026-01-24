#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



// https://github.com/Disservin/chess-library/blob/master/tests/board.cpp
TEST_SUITE("Zobrist Hash") {
    TEST_CASE("Test Zobrist Hash Startpos") {
        Board b = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        CHECK(b.hash() == 0x463b96181691fc9c);

        auto mv = Move::make(Square::E2, Square::E4);
        CHECK(b.hash_after<true>(mv) == 0x823c9b50fd114196);
        CHECK(b.hash_after<false>(mv) == 0x823c9b50fd114196);
        b.make_move(mv);
        CHECK(b.hash() == 0x823c9b50fd114196);

        mv = Move::make(Square::D7, Square::D5);
        CHECK(b.hash_after<true>(mv) == 0x0756b94461c50fb0);
        CHECK(b.hash_after<false>(mv) == 0x0756b94461c50fb0);
        b.make_move(mv);
        CHECK(b.hash() == 0x0756b94461c50fb0);

        mv = Move::make(Square::E4, Square::E5);
        CHECK(b.hash_after<true>(mv) == 0x662fafb965db29d4);
        CHECK(b.hash_after<false>(mv) == 0x662fafb965db29d4);
        b.make_move(mv);
        CHECK(b.hash() == 0x662fafb965db29d4);

        mv = Move::make(Square::F7, Square::F5);
        CHECK(b.hash_after<true>(mv) == 0x22a48b5a8e47ff78);
        b.make_move(mv);
        CHECK(b.hash() == 0x22a48b5a8e47ff78);

        mv = Move::make(Square::E1, Square::E2);
        CHECK(b.hash_after<true>(mv) == 0x652a607ca3f242c1);
        b.make_move(mv);
        CHECK(b.hash() == 0x652a607ca3f242c1);

        mv = Move::make(Square::E8, Square::F7);
        CHECK(b.hash_after<true>(mv) == 0x00fdd303c946bdd9);
        b.make_move(mv);
        CHECK(b.hash() == 0x00fdd303c946bdd9);
    }

    TEST_CASE("Test Zobrist Hash Second Position") {
        Board b = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        auto mv = Move::make(Square::A2, Square::A4);
        CHECK(b.hash_after<true>(mv) == 0x2DF2E8F47B022952);
        CHECK(b.hash_after<false>(mv) == 0x2DF2E8F47B022952);
        b.make_move(mv);

        mv = Move::make(Square::B7, Square::B5);
        CHECK(b.hash_after<true>(mv) == 0x4DF682E1E0AF946F);
        CHECK(b.hash_after<false>(mv) == 0x4DF682E1E0AF946F);
        b.make_move(mv);

        mv = Move::make(Square::H2, Square::H4);
        CHECK(b.hash_after<true>(mv) == 0xD1551EC84B90ED11);
        CHECK(b.hash_after<false>(mv) == 0xD1551EC84B90ED11);
        b.make_move(mv);

        mv = Move::make(Square::B5, Square::B4);
        CHECK(b.hash_after<true>(mv) == 0xB0982F168A89B452);
        CHECK(b.hash_after<false>(mv) == 0xB0982F168A89B452);
        b.make_move(mv);

        mv = Move::make(Square::C2, Square::C4);
        CHECK(b.hash_after<true>(mv) == 0x3C8123EA7B067637);
        b.make_move(mv);

        CHECK(b.hash() == 0x3c8123ea7b067637);

        mv = Move::make<Move::ENPASSANT>(Square::B4, Square::C3);
        CHECK(b.hash_after<true>(mv) == 0x93D32682782EDFAE);
        b.make_move(mv);

        mv = Move::make(Square::A1, Square::A3);
        CHECK(b.hash_after<true>(mv) == 0x5c3f9b829b279560);
        b.make_move(mv);

        CHECK(b.hash() == 0x5c3f9b829b279560);
    }

    TEST_CASE("Test Zobrist Hash White Castling") {
        Board b;

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E1, Square::H1);
            CHECK(b.hash_after<true>(mv) == 9500135572247264067ull);
            b.make_move(mv);
            CHECK(b.hash() == 9500135572247264067ull);

            b.set_fen("r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
            CHECK(b.hash() == 9500135572247264067ull);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E1, Square::A1);
            CHECK(b.hash_after<true>(mv) == 14235734314054086603ull);
            b.make_move(mv);
            CHECK(b.hash() == 14235734314054086603ull);

            b.set_fen("r3k2r/8/8/8/8/8/8/2KR3R b kq - 1 1");
            CHECK(b.hash() == 14235734314054086603ull);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make(Square::E1, Square::F2);
            CHECK(b.hash_after<true>(mv) == 9624187742021389814ull);
            b.make_move(mv);
            CHECK(b.hash() == 9624187742021389814ull);

            b.set_fen("r3k2r/8/8/8/8/8/5K2/R6R b kq - 1 1");
            CHECK(b.hash() == 9624187742021389814ull);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make(Square::E1, Square::D2);
            CHECK(b.hash_after<true>(mv) == 9930369406275139450ull);
            b.make_move(mv);
            CHECK(b.hash() == 9930369406275139450ull);

            b.set_fen("r3k2r/8/8/8/8/8/3K4/R6R b kq - 1 1");
            CHECK(b.hash() == 9930369406275139450ull);
        }

        {
            b.set_fen("r3k3/8/8/8/8/8/8/4K2R w Kq - 0 1");
            auto mv = Move::make(Square::H1, Square::H2);
            CHECK(b.hash_after<true>(mv) == 16699550102102435117ull);
            b.make_move(mv);
            CHECK(b.hash() == 16699550102102435117ull);

            b.set_fen("r3k3/8/8/8/8/8/7R/4K3 b q - 1 1");
            CHECK(b.hash() == 16699550102102435117ull);
        }
    }

    TEST_CASE("Test Zobrist Hash Black Castling") {
        Board b;

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E8, Square::H8);
            CHECK(b.hash_after<true>(mv) == 1143055385231625393ull);
            b.make_move(mv);
            CHECK(b.hash() == 1143055385231625393ull);

            b.set_fen("r4rk1/8/8/8/8/8/8/R3K2R w KQ - 1 2");
            CHECK(b.hash() == 1143055385231625393ull);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E8, Square::A8);
            CHECK(b.hash_after<true>(mv) == 3909839401934106976ull);
            b.make_move(mv);
            CHECK(b.hash() == 3909839401934106976ull);

            b.set_fen("2kr3r/8/8/8/8/8/8/R3K2R w KQ - 1 2");
            CHECK(b.hash() == 3909839401934106976ull);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
            auto mv = Move::make(Square::E8, Square::F7);
            CHECK(b.hash_after<true>(mv) == 6963598675116890690ull);
            b.make_move(mv);
            CHECK(b.hash() == 6963598675116890690ull);

            b.set_fen("r6r/5k2/8/8/8/8/8/R3K2R w KQ - 1 2");
            CHECK(b.hash() == 6963598675116890690ull);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
            auto mv = Move::make(Square::E8, Square::D7);
            CHECK(b.hash_after<true>(mv) == 6266296883909487836ull);
            b.make_move(mv);
            CHECK(b.hash() == 6266296883909487836ull);

            b.set_fen("r6r/3k4/8/8/8/8/8/R3K2R w KQ - 1 2");
            CHECK(b.hash() == 6266296883909487836ull);
        }

        {
            b.set_fen("r3k3/8/8/3B4/8/8/8/4K3 w q - 0 1");
            auto mv = Move::make(Square::D5, Square::A8);
            CHECK(b.hash_after<true>(mv) == 11170087546614790902ull);
            b.make_move(mv);
            CHECK(b.hash() == 11170087546614790902ull);

            b.set_fen("B3k3/8/8/8/8/8/8/4K3 b - - 0 1");
            CHECK(b.hash() == 11170087546614790902ull);
        }

        {
            b.set_fen("r3k3/8/8/8/8/8/8/4K2R b Kq - 0 1");
            auto mv = Move::make(Square::A8, Square::A7);
            CHECK(b.hash_after<true>(mv) == 16038026699965099486ull);
            b.make_move(mv);
            CHECK(b.hash() == 16038026699965099486ull);

            b.set_fen("4k3/r7/8/8/8/8/8/4K2R w K - 1 2");
            CHECK(b.hash() == 16038026699965099486ull);
        }
    }

    TEST_CASE("Test Zobrist Hash Enpassant") {
        Board b;

        {
            b.set_fen("rnbqkbnr/pppppppp/8/5P2/8/8/PPPPP1PP/RNBQKBNR b KQkq - 0 1");
            auto mv = Move::make(Square::E7, Square::E5);
            CHECK(b.hash_after<true>(mv) == 7868419208351108928ull);
            b.make_move(mv);
            CHECK(b.hash() == 7868419208351108928ull);

            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
            CHECK(b.hash() == 7868419208351108928ull);
        }

        {
            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
            auto mv = Move::make<Move::ENPASSANT>(Square::F5, Square::E6);
            CHECK(b.hash_after<true>(mv) == 14328713941299708535ull);
            b.make_move(mv);
            CHECK(b.hash() == 14328713941299708535ull);

            b.set_fen("rnbqkbnr/pppp1ppp/4P3/8/8/8/PPPPP1PP/RNBQKBNR b KQkq - 0 2");
            CHECK(b.hash() == 14328713941299708535ull);
        }

        {
            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
            auto mv = Move::make(Square::E2, Square::E3);
            CHECK(b.hash_after<true>(mv) == 1093876600604329325ull);
            b.make_move(mv);
            CHECK(b.hash() == 1093876600604329325ull);

            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/4P3/PPPP2PP/RNBQKBNR b KQkq - 0 2");
            CHECK(b.hash() == 1093876600604329325ull);
        }

        {
            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
            auto mv = Move::make(Square::F5, Square::F6);
            CHECK(b.hash_after<true>(mv) == 2265987269840498900ull);
            b.make_move(mv);
            CHECK(b.hash() == 2265987269840498900ull);

            b.set_fen("rnbqkbnr/pppp1ppp/5P2/4p3/8/8/PPPPP1PP/RNBQKBNR b KQkq - 0 2");
            CHECK(b.hash() == 2265987269840498900ull);
        }
    }

    TEST_CASE("Test Zobrist Hash Null Move") {
        Board b;

        b.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        CHECK(b.hash_after<true>(Move::NULL_MOVE) == 13757846718353144213ull);
        b.make_nullmove();
        CHECK(b.hash() == 13757846718353144213ull);
    }
}
