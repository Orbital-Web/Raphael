#pragma once
#include <chess/board.h>
#include <chess/movegen_fwd.h>

#include <chrono>
#include <iostream>



namespace chess {
class Perft {
public:
    template <bool bulk>
    static u64 perft(const Board& board, i32 depth) {
        MoveList<ScoredMove> moves;
        Movegen::generate_legals(moves, board);

        if (bulk && depth <= 1)
            return moves.size();
        else if (!bulk && depth <= 0)
            return 1;

        u64 nodes = 0;
        for (const auto& smove : moves) {
            Board newboard = board;
            newboard.make_move(smove.move);
            nodes += perft<bulk>(newboard, depth - 1);
        }

        return nodes;
    }

    static u64 bench_perft(const Board& board, i32 depth, bool bulk) {
        const auto t1 = std::chrono::steady_clock::now();
        const auto nodes = (bulk) ? perft<true>(board, depth) : perft<false>(board, depth);
        const auto t2 = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        std::cout << "depth " << depth << " time " << ms << " nodes " << nodes << " nps "
                  << (nodes * 1000) / (ms + 1) << " fen " << board.get_fen() << "\n"
                  << std::flush;

        return nodes;
    }
};
}  // namespace chess
