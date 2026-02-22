#include <chess/include.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <tests/doctest/doctest.hpp>

using namespace chess;
using namespace std::chrono;
using std::cout;
using std::flush;
using std::left;
using std::setw;
using std::string;



// https://github.com/Disservin/chess-library/blob/master/tests/perft.cpp
class Perft {
public:
    static u64 perft(const Board& board, i32 depth) {
        MoveList<ScoredMove> moves;
        Movegen::generate_legals(moves, board);

        if (depth == 1) return moves.size();

        u64 nodes = 0;
        for (const auto& smove : moves) {
            Board newboard = board;
            newboard.make_move(smove.move);
            nodes += perft(newboard, depth - 1);
        }

        return nodes;
    }

    static void bench_perft(const Board& board, i32 depth, u64 expected_node_count) {
        const auto t1 = steady_clock::now();
        const auto nodes = perft(board, depth);
        const auto t2 = steady_clock::now();
        const auto ms = duration_cast<milliseconds>(t2 - t1).count();

        cout << "depth " << left << setw(2) << depth << " time " << setw(5) << ms << " nodes "
             << setw(12) << nodes << " nps " << setw(9) << (nodes * 1000) / (ms + 1) << " fen "
             << setw(87) << board.get_fen() << "\n"
             << flush;

        CHECK(nodes == expected_node_count);
    }
};

struct Test {
    string fen;
    u64 expected_node_count;
    i32 depth;
};

TEST_SUITE("PERFT") {
    TEST_CASE("Standard Chess") {
        const Test test_positions[] = {
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",                3195901860, 7},
            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",       193690690,  5},
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",                                  178633661,  7},
            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",        706045033,  6},
            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",               89941194,   5},
            {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1",
             164075551,                                                                             5}
        };

        Perft perft;
        Board board;

        for (const auto& test : test_positions) {
            board.set_fen(test.fen);
            perft.bench_perft(board, test.depth, test.expected_node_count);
        }
    }
}
