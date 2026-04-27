#include <chess/include.h>

#include <tests/doctest/doctest.hpp>

using namespace chess;



// https://github.com/Disservin/chess-library/blob/master/tests/board.cpp
TEST_SUITE("Zobrist Hash") {
    TEST_CASE("Test Zobrist Hash Startpos") {
        Board b = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        CHECK(b.hash() == 0x19E7225E2ECA1F98);
        CHECK(b.pawn_hash() == 0x37FC40DA841E1692);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        auto mv = Move::make(Square::E2, Square::E4);
        CHECK(b.hash_after<true>(mv) == 0xDDE02F16C54AA292);
        CHECK(b.hash_after<false>(mv) == 0xDDE02F16C54AA292);
        b.make_move(mv);
        CHECK(b.hash() == 0xDDE02F16C54AA292);
        CHECK(b.pawn_hash() == 0xB2D6B38C0B92E91);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::D7, Square::D5);
        CHECK(b.hash_after<true>(mv) == 0x588A0D02599EECB4);
        CHECK(b.hash_after<false>(mv) == 0x588A0D02599EECB4);
        b.make_move(mv);
        CHECK(b.hash() == 0x588A0D02599EECB4);
        CHECK(b.pawn_hash() == 0x76916F86F34AE5BE);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::E4, Square::E5);
        CHECK(b.hash_after<true>(mv) == 0x39F31BFF5D80CAD0);
        CHECK(b.hash_after<false>(mv) == 0x39F31BFF5D80CAD0);
        b.make_move(mv);
        CHECK(b.hash() == 0x39F31BFF5D80CAD0);
        CHECK(b.pawn_hash() == 0xEF3E5FD1587346D3);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::F7, Square::F5);
        CHECK(b.hash_after<true>(mv) == 0x7D783F1CB61C1C7C);
        b.make_move(mv);
        CHECK(b.hash() == 0x7D783F1CB61C1C7C);
        CHECK(b.pawn_hash() == 0x53635D981CC81576);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::E1, Square::E2);
        CHECK(b.hash_after<true>(mv) == 0x3AF6D43A9BA9A1C5);
        b.make_move(mv);
        CHECK(b.hash() == 0x3AF6D43A9BA9A1C5);
        CHECK(b.pawn_hash() == 0x83871FE249DCEE04);
        CHECK(b.major_hash() == 0xF569B3D99F2296AF);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x36AB63FA493EB1CE);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::E8, Square::F7);
        CHECK(b.hash_after<true>(mv) == 0x5F216745F11D5EDD);
        b.make_move(mv);
        CHECK(b.hash() == 0x5F216745F11D5EDD);
        CHECK(b.pawn_hash() == 0x83871FE249DCEE04);
        CHECK(b.major_hash() == 0x4EE1363BF3987BC6);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x36AB63FA493EB1CE);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x4D0789B16683671A);
    }

    TEST_CASE("Test Zobrist Hash Second Position") {
        Board b = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        auto mv = Move::make(Square::A2, Square::A4);
        CHECK(b.hash_after<true>(mv) == 0x722E5CB24359CA56);
        CHECK(b.hash_after<false>(mv) == 0x722E5CB24359CA56);
        b.make_move(mv);
        CHECK(b.pawn_hash() == 0xA4E3189C46AA4655);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::B7, Square::B5);
        CHECK(b.hash_after<true>(mv) == 0x122A36A7D8F4776B);
        CHECK(b.hash_after<false>(mv) == 0x122A36A7D8F4776B);
        b.make_move(mv);
        CHECK(b.pawn_hash() == 0x3C31542372207E61);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::H2, Square::H4);
        CHECK(b.hash_after<true>(mv) == 0x8E89AA8E73CB0E15);
        CHECK(b.hash_after<false>(mv) == 0x8E89AA8E73CB0E15);
        b.make_move(mv);
        CHECK(b.pawn_hash() == 0x5844EEA076388216);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::B5, Square::B4);
        CHECK(b.hash_after<true>(mv) == 0xEF449B50B2D25756);
        CHECK(b.hash_after<false>(mv) == 0xEF449B50B2D25756);
        b.make_move(mv);
        CHECK(b.pawn_hash() == 0xC15FF9D418065E5C);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::C2, Square::C4);
        CHECK(b.hash_after<true>(mv) == 0x635D97AC435D9533);
        b.make_move(mv);
        CHECK(b.pawn_hash() == 0xB590D38246AE1930);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        CHECK(b.hash() == 0x635D97AC435D9533);

        mv = Move::make<Move::ENPASSANT>(Square::B4, Square::C3);
        CHECK(b.hash_after<true>(mv) == 0xCC0F92C440753CAA);
        b.make_move(mv);
        CHECK(b.pawn_hash() == 0xE214F040EAA135A0);
        CHECK(b.major_hash() == 0x35DB1B902419D42F);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x99A544452583308C);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        mv = Move::make(Square::A1, Square::A3);
        CHECK(b.hash_after<true>(mv) == 0x3E32FC4A37C7664);
        b.make_move(mv);
        CHECK(b.pawn_hash() == 0xE214F040EAA135A0);
        CHECK(b.major_hash() == 0x2E1803A68371BE8);
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x5FFA6A68B6247EDB);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x6B8E9986CFAAF062);

        CHECK(b.hash() == 0x3E32FC4A37C7664);
    }

    TEST_CASE("Test Zobrist Hash White Castling") {
        Board b;

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E1, Square::H1);
            CHECK(b.hash_after<true>(mv) == 0xDC0BF6BE2D825847);
            b.make_move(mv);
            CHECK(b.hash() == 0xDC0BF6BE2D825847);

            b.set_fen("r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
            CHECK(b.hash() == 0xDC0BF6BE2D825847);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E1, Square::A1);
            CHECK(b.hash_after<true>(mv) == 0x9A53C8A902538CCF);
            b.make_move(mv);
            CHECK(b.hash() == 0x9A53C8A902538CCF);

            b.set_fen("r3k2r/8/8/8/8/8/8/2KR3R b kq - 1 1");
            CHECK(b.hash() == 0x9A53C8A902538CCF);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make(Square::E1, Square::F2);
            CHECK(b.hash_after<true>(mv) == 0xDA534F84A6816EF2);
            b.make_move(mv);
            CHECK(b.hash() == 0xDA534F84A6816EF2);

            b.set_fen("r3k2r/8/8/8/8/8/5K2/R6R b kq - 1 1");
            CHECK(b.hash() == 0xDA534F84A6816EF2);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
            auto mv = Move::make(Square::E1, Square::D2);
            CHECK(b.hash_after<true>(mv) == 0xD613761AAD34547E);
            b.make_move(mv);
            CHECK(b.hash() == 0xD613761AAD34547E);

            b.set_fen("r3k2r/8/8/8/8/8/3K4/R6R b kq - 1 1");
            CHECK(b.hash() == 0xD613761AAD34547E);
        }

        {
            b.set_fen("r3k3/8/8/8/8/8/8/4K2R w Kq - 0 1");
            auto mv = Move::make(Square::H1, Square::H2);
            CHECK(b.hash_after<true>(mv) == 0xB81C0C1138FC8629);
            b.make_move(mv);
            CHECK(b.hash() == 0xB81C0C1138FC8629);

            b.set_fen("r3k3/8/8/8/8/8/7R/4K3 b q - 1 1");
            CHECK(b.hash() == 0xB81C0C1138FC8629);
        }
    }

    TEST_CASE("Test Zobrist Hash Black Castling") {
        Board b;

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
            auto mv = Move::make<Move::CASTLING>(Square::E8, Square::H8);
            CHECK(b.hash_after<true>(mv) == 0x500046972B0FEFB5);
            b.make_move(mv);
            CHECK(b.hash() == 0x500046972B0FEFB5);

            b.set_fen("r4rk1/8/8/8/8/8/8/R3K2R w KQ - 1 2");
            CHECK(b.hash() == 0x500046972B0FEFB5);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 13 20");
            auto mv = Move::make<Move::CASTLING>(Square::E8, Square::A8);
            CHECK(b.hash_after<true>(mv) == 0xE1002A43733194A6);
            b.make_move(mv);
            CHECK(b.hash() == 0xE1002A43733194A6);

            b.set_fen("2kr3r/8/8/8/8/8/8/R3K2R w KQ - 14 21");
            CHECK(b.hash() == 0xE1002A43733194A6);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 12 20");
            auto mv = Move::make(Square::E8, Square::F7);
            CHECK(b.hash_after<true>(mv) == 0x3F7F185F94E2F946);
            b.make_move(mv);
            CHECK(b.hash() == 0x3F7F185F94E2F946);

            b.set_fen("r6r/5k2/8/8/8/8/8/R3K2R w KQ - 13 21");
            CHECK(b.hash() == 0x3F7F185F94E2F946);
        }

        {
            b.set_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
            auto mv = Move::make(Square::E8, Square::D7);
            CHECK(b.hash_after<true>(mv) == 0x92AEF96F6D40FD8);
            b.make_move(mv);
            CHECK(b.hash() == 0x92AEF96F6D40FD8);

            b.set_fen("r6r/3k4/8/8/8/8/8/R3K2R w KQ - 1 2");
            CHECK(b.hash() == 0x92AEF96F6D40FD8);
        }

        {
            b.set_fen("r3k3/8/8/3B4/8/8/8/4K3 w q - 0 1");
            auto mv = Move::make(Square::D5, Square::A8);
            CHECK(b.hash_after<true>(mv) == 0xC4D8AB371DA6D9F2);
            b.make_move(mv);
            CHECK(b.hash() == 0xC4D8AB371DA6D9F2);

            b.set_fen("B3k3/8/8/8/8/8/8/4K3 b - - 0 1");
            CHECK(b.hash() == 0xC4D8AB371DA6D9F2);
        }

        {
            b.set_fen("r3k3/8/8/8/8/8/8/4K2R b Kq - 0 1");
            auto mv = Move::make(Square::A8, Square::A7);
            CHECK(b.hash_after<true>(mv) == 0x814E300945FD8EDA);
            b.make_move(mv);
            CHECK(b.hash() == 0x814E300945FD8EDA);

            b.set_fen("4k3/r7/8/8/8/8/8/4K2R w K - 1 2");
            CHECK(b.hash() == 0x814E300945FD8EDA);
        }
    }

    TEST_CASE("Test Zobrist Hash Enpassant") {
        Board b;

        {
            b.set_fen("rnbqkbnr/pppppppp/8/5P2/8/8/PPPPP1PP/RNBQKBNR b KQkq - 14 20");
            auto mv = Move::make(Square::E7, Square::E5);
            CHECK(b.hash_after<true>(mv) == 0x32EE89D2E9FB2C44);
            b.make_move(mv);
            CHECK(b.hash() == 0x32EE89D2E9FB2C44);
            CHECK(b.pawn_hash() == 0x1CF5EB56432F254E);

            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 21");
            CHECK(b.hash() == 0x32EE89D2E9FB2C44);
            CHECK(b.pawn_hash() == 0x1CF5EB56432F254E);
        }

        {
            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
            auto mv = Move::make<Move::ENPASSANT>(Square::F5, Square::E6);
            CHECK(b.hash_after<true>(mv) == 0x9905652F9700A173);
            b.make_move(mv);
            CHECK(b.hash() == 0x9905652F9700A173);
            CHECK(b.pawn_hash() == 0x4FC8210192F32D70);

            b.set_fen("rnbqkbnr/pppp1ppp/4P3/8/8/8/PPPPP1PP/RNBQKBNR b KQkq - 0 2");
            CHECK(b.hash() == 0x9905652F9700A173);
            CHECK(b.pawn_hash() == 0x4FC8210192F32D70);
        }

        {
            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
            auto mv = Move::make(Square::E2, Square::E3);
            CHECK(b.hash_after<true>(mv) == 0x50F28EBEC779E669);
            b.make_move(mv);
            CHECK(b.hash() == 0x50F28EBEC779E669);
            CHECK(b.pawn_hash() == 0x863FCA90C28A6A6A);

            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/4P3/PPPP2PP/RNBQKBNR b KQkq - 0 2");
            CHECK(b.hash() == 0x50F28EBEC779E669);
            CHECK(b.pawn_hash() == 0x863FCA90C28A6A6A);
        }

        {
            b.set_fen("rnbqkbnr/pppp1ppp/8/4pP2/8/8/PPPPP1PP/RNBQKBNR w KQkq e6 0 2");
            auto mv = Move::make(Square::F5, Square::F6);
            CHECK(b.hash_after<true>(mv) == 0x40AED32CB43D93D0);
            b.make_move(mv);
            CHECK(b.hash() == 0x40AED32CB43D93D0);
            CHECK(b.pawn_hash() == 0x96639702B1CE1FD3);

            b.set_fen("rnbqkbnr/pppp1ppp/5P2/4p3/8/8/PPPPP1PP/RNBQKBNR b KQkq - 0 2");
            CHECK(b.hash() == 0x40AED32CB43D93D0);
            CHECK(b.pawn_hash() == 0x96639702B1CE1FD3);
        }
    }

    TEST_CASE("Test Zobrist Hash Null Move") {
        Board b;

        b.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 13 20");
        CHECK(b.hash_after<true>(Move::NULL_MOVE) == 0xE13104F481ED9A91);
        b.make_nullmove();
        CHECK(b.hash() == 0xE13104F481ED9A91);
    }

    TEST_CASE("Test Zobrist Hash Pawn") {
        Board b;

        b.set_fen("4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1");
        CHECK(b.pawn_hash() == 0x37FC40DA841E1692);

        b.set_fen("4k3/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/4K3 w - f6 0 3");
        CHECK(b.pawn_hash() == 0x53635D981CC81576);

        b.make_nullmove();
        CHECK(b.pawn_hash() == 0x83871FE249DCEE04);

        b.set_fen("4k3/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/4K3 w - - 0 3");
        CHECK(b.pawn_hash() == 0x83871FE249DCEE04);
    }

    TEST_CASE("Test Zobrist Hash Major") {
        Board b;

        b.set_fen("r3k2r/ppp1pppp/q7/8/8/8/PPPPP1PP/R3KQ1R w KQkq - 0 1");
        CHECK(b.major_hash() == 0xAF0D5B5672DE735C);

        b.make_move(Move::make(Square::F1, Square::F3));
        CHECK(b.major_hash() == 0x60C7F20CB0D8C76);

        b.make_move(Move::make(Square::E7, Square::E6));
        CHECK(b.major_hash() == 0x60C7F20CB0D8C76);

        b.make_move(Move::make<Move::CASTLING>(Square::E1, Square::A1));
        CHECK(b.major_hash() == 0xB9CA7A0FDBE80FCB);

        b.make_move(Move::make(Square::A6, Square::A2));
        CHECK(b.major_hash() == 0x77280A8F92D3A246);

        b.make_move(Move::make(Square::H1, Square::F1));
        CHECK(b.major_hash() == 0x6CBBB843EA744C49);

        b.make_move(Move::make(Square::E8, Square::E7));
        CHECK(b.major_hash() == 0xD7333DA186CEA120);
    }

    TEST_CASE("Test Zobrist Nonpawn Pawn") {
        Board b;

        b.set_fen("r3kbnr/ppp1p1pp/8/8/8/8/PPPP1PPP/2N1K2R w Kkq - 0 3");
        CHECK(b.nonpawn_hash(Color::WHITE) == 0x9AEA3DFDD65D0B88);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0xB146EEEF9EB5E006);

        b.make_move(Move::make<Move::CASTLING>(Square::E1, Square::H1));
        CHECK(b.nonpawn_hash(Color::WHITE) == 0xDCFBC82ABEB21711);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0xB146EEEF9EB5E006);

        b.make_move(Move::make<Move::CASTLING>(Square::E8, Square::A8));
        CHECK(b.nonpawn_hash(Color::WHITE) == 0xDCFBC82ABEB21711);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0xC12ED8F9123EDC5C);

        b.make_move(Move::make(Square::D2, Square::D4));
        CHECK(b.nonpawn_hash(Color::WHITE) == 0xDCFBC82ABEB21711);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0xC12ED8F9123EDC5C);

        b.make_move(Move::make(Square::D8, Square::D4));
        CHECK(b.nonpawn_hash(Color::WHITE) == 0xDCFBC82ABEB21711);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x1FD0EE7063B4A5FC);

        b.make_move(Move::make(Square::C1, Square::D3));
        CHECK(b.nonpawn_hash(Color::WHITE) == 0xD1C7F409513FF167);
        CHECK(b.nonpawn_hash(Color::BLACK) == 0x1FD0EE7063B4A5FC);
    }
}
