#include <Raphael/position.h>

#include <iostream>
#include <tests/doctest/doctest.hpp>

using namespace raphael;
using std::cout;
using std::flush;
using std::string;
using std::vector;


class NnueTester {
private:
    Position<true> position_;
    Nnue refnet_;


public:
    void check(i32 depth) {
        if (depth == 0) return;

        const auto& oldboard = position_.board();
        chess::MoveList<chess::ScoredMove> moves;
        chess::Movegen::generate_legals(moves, oldboard);

        for (const auto& smove : moves) {
            position_.make_move(smove.move);
            const auto eval = position_.evaluate();

            const auto& newboard = position_.board();
            refnet_.set_board(newboard);
            const auto true_eval = refnet_.evaluate(newboard.stm());

            if (eval != true_eval) {
                cout << "fail: eval after make_move not consistent with eval after set_board "
                     << eval << " != " << true_eval << " after move "
                     << chess::uci::from_move(smove.move) << " from position " << oldboard.get_fen()
                     << "\n"
                     << flush;

                CHECK(false);
            }

            check(depth - 1);
            position_.unmake_move();
        }

        // check nullmove
        position_.make_nullmove();
        const auto eval = position_.evaluate();

        const auto& newboard = position_.board();
        refnet_.set_board(newboard);
        const auto true_eval = refnet_.evaluate(newboard.stm());

        if (eval != true_eval) {
            cout << "fail: eval after make_move not consistent with eval after set_board " << eval
                 << " != " << true_eval << " after nullmove " << " from position "
                 << oldboard.get_fen() << "\n"
                 << flush;

            CHECK(false);
        }
        position_.unmake_nullmove();
    }

    void check_all(const chess::Board& board) {
        position_.set_board(board);

        check(4);
        CHECK(true);
    }
};

TEST_SUITE("Position") {
    TEST_CASE("Make Unmake") {
        SUBCASE("make_move") {
            const chess::Board board(
                "rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2"
            );
            Position<false> position;
            position.set_board(board);

            CHECK(position.board().at(chess::Square::D2) == chess::Piece::NONE);
            CHECK(position.board().at(chess::Square::D4) == chess::Piece::WHITEPAWN);
            CHECK(position.board().at(chess::Square::D7) == chess::Piece::NONE);
            CHECK(position.board().at(chess::Square::D5) == chess::Piece::BLACKPAWN);

            position.make_move(chess::Move::make(chess::Square::E2, chess::Square::E4));
            CHECK(position.board().at(chess::Square::E2) == chess::Piece::NONE);
            CHECK(position.board().at(chess::Square::E4) == chess::Piece::WHITEPAWN);

            position.make_move(chess::Move::make(chess::Square::E7, chess::Square::E5));
            CHECK(position.board().at(chess::Square::E7) == chess::Piece::NONE);
            CHECK(position.board().at(chess::Square::E5) == chess::Piece::BLACKPAWN);

            position.unmake_move();
            CHECK(position.board().at(chess::Square::E7) == chess::Piece::BLACKPAWN);
            CHECK(position.board().at(chess::Square::E5) == chess::Piece::NONE);

            position.unmake_move();
            CHECK(position.board().at(chess::Square::E2) == chess::Piece::WHITEPAWN);
            CHECK(position.board().at(chess::Square::E4) == chess::Piece::NONE);

            CHECK(position.board().at(chess::Square::D2) == chess::Piece::NONE);
            CHECK(position.board().at(chess::Square::D4) == chess::Piece::WHITEPAWN);
            CHECK(position.board().at(chess::Square::D7) == chess::Piece::NONE);
            CHECK(position.board().at(chess::Square::D5) == chess::Piece::BLACKPAWN);
        }

        SUBCASE("make_nullmove") {
            const chess::Board board(
                "rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2"
            );
            Position<false> position;
            position.set_board(board);

            position.make_nullmove();
            CHECK(position.board().hash() != board.hash());
            CHECK(position.board().stm() == chess::Color::BLACK);

            position.unmake_nullmove();
            CHECK(position.board().hash() == board.hash());
            CHECK(position.board().stm() == chess::Color::WHITE);
        }
    }

    TEST_CASE("Repetition") {
        chess::Board board = chess::Board("7k/8/8/8/8/Q7/8/3K4 w - - 0 1");
        Position<false> position;
        position.set_board(board);

        CHECK(!position.is_repetition(1));
        CHECK(!position.is_repetition(2));

        position.make_move(chess::Move::make(chess::Square::D1, chess::Square::D2));
        position.make_move(chess::Move::make(chess::Square::H8, chess::Square::H7));
        CHECK(!position.is_repetition(1));
        CHECK(!position.is_repetition(2));

        position.make_move(chess::Move::make(chess::Square::D2, chess::Square::D1));
        position.make_move(chess::Move::make(chess::Square::H7, chess::Square::H8));
        CHECK(position.is_repetition(1));
        CHECK(!position.is_repetition(2));

        position.make_move(chess::Move::make(chess::Square::D1, chess::Square::D2));
        position.make_move(chess::Move::make(chess::Square::H8, chess::Square::H7));
        CHECK(position.is_repetition(1));
        CHECK(!position.is_repetition(2));

        position.make_move(chess::Move::make(chess::Square::D2, chess::Square::D1));
        position.make_move(chess::Move::make(chess::Square::H7, chess::Square::H8));
        CHECK(position.is_repetition(1));
        CHECK(position.is_repetition(2));
    }

    TEST_CASE("NNUE") {
        SUBCASE("Standard Chess") {
            const string test_positions[] = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
                "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
                "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1",
                "2r1n3/1N1NpPk1/4p1P1/p1pP1p1P/1rQ3p1/1b6/2B5/R3K2R w KQ c6 0 1",
                "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1",
                "8/8/p1p5/1p5p/1P5p/8/PPP2K1p/4R1rk w - - 0 1",
                "r3k2r/pppp1ppp/4P3/3b2N1/2n2B2/4p3/PPPP1PPP/R3K2R w KQkq - 0 1",
            };

            NnueTester tester;
            chess::Board board;

            for (const auto& fen : test_positions) {
                board.set_fen(fen);
                tester.check_all(board);
            }
        }
    }
}
