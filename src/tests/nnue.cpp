#include <Raphael/nnue.h>

#include <iostream>
#include <tests/doctest/doctest.hpp>

using std::cout;
using std::flush;
using std::string;
using std::vector;


class NnueTester {
private:
    chess::Board board_;
    raphael::Nnue net_;
    raphael::Nnue refnet_;


public:
    void check(i32 ply, i32 depth) {
        if (depth == 0) return;

        chess::MoveList<chess::ScoredMove> moves;
        chess::Movegen::generate_legals(moves, board_);

        for (const auto& smove : moves) {
            net_.make_move(board_, smove.move, ply);
            board_.make_move(smove.move);
            const auto evalw = net_.evaluate(ply, chess::Color::WHITE);
            const auto evalb = net_.evaluate(ply, chess::Color::BLACK);

            check(ply + 1, depth - 1);

            refnet_.set_board(board_);
            const auto true_evalw = refnet_.evaluate(0, chess::Color::WHITE);
            const auto true_evalb = refnet_.evaluate(0, chess::Color::BLACK);

            board_.unmake_move(smove.move);

            if (evalw != true_evalw || evalb != true_evalb) {
                cout << "fail: eval after make_move not consistent with eval after set_board "
                     << "([" << evalw << ", " << evalb << "] != [" << true_evalw << ", "
                     << true_evalb << "]) after move " << chess::uci::from_move(smove.move)
                     << " from position " << board_.get_fen() << "\n"
                     << flush;

                CHECK(false);
            }
        }
    }

    void check_all(const string& fen) {
        board_.set_fen(fen);
        net_.set_board(board_);

        check(1, 4);
        CHECK(true);
    }
};

TEST_SUITE("NNUE") {
    TEST_CASE("Standard Chess") {
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
        for (const auto& fen : test_positions) tester.check_all(fen);
    }

    TEST_CASE("Null Move") {
        chess::Board board;
        raphael::Nnue net;

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

        for (const auto& fen : test_positions) {
            board.set_fen(fen);
            net.set_board(board);

            net.make_move(board, chess::Move::NULL_MOVE, 1);
            board.make_nullmove();
            const auto evalw = net.evaluate(1, chess::Color::WHITE);
            const auto evalb = net.evaluate(1, chess::Color::BLACK);

            net.set_board(board);
            const auto true_evalw = net.evaluate(0, chess::Color::WHITE);
            const auto true_evalb = net.evaluate(0, chess::Color::BLACK);

            board.unmake_nullmove();

            if (evalw != true_evalw || evalb != true_evalb) {
                cout << "fail: eval after make_move not consistent with eval after set_board "
                     << "([" << evalw << ", " << evalb << "] != [" << true_evalw << ", "
                     << true_evalb << "]) after nullmove " << " from position " << board.get_fen()
                     << "\n"
                     << flush;

                CHECK(false);
            }

            CHECK(true);
        }
    }
}
