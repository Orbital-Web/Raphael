#pragma once
#include <Raphael/consts.h>

#include <Raphael/Raphael_v1.8.hpp>
#include <string>

using std::string;



namespace Raphael {
class v1_8_traingen : public v1_8 {
public:
    v1_8_traingen(string name_in) : v1_8(name_in) {}

    // Returns the relative eval of this position using the get_move logic of the engine
    int get_eval(chess::Board board) {
        bool halt = false;
        int depth = 1;
        int eval = 0;
        int alpha = -INT_MAX;
        int beta = INT_MAX;
        history.clear();
        itermove = chess::Move::NO_MOVE;
        nodes = 0;
        startSearchTimer(board, 0, 0);

        // begin iterative deepening
        while (!halt && depth <= MAX_DEPTH) {
            // max depth override
            if (searchopt.maxdepth != -1 && depth > searchopt.maxdepth) break;

            int itereval = negamax(board, depth, 0, params.MAX_EXTENSIONS, alpha, beta, halt);

            // not timeout
            if (!halt) {
                eval = itereval;

                // re-search required
                if ((eval <= alpha) || (eval >= beta)) {
                    alpha = -INT_MAX;
                    beta = INT_MAX;
                    continue;
                }

                // narrow window
                alpha = eval - params.ASPIRATION_WINDOW;
                beta = eval + params.ASPIRATION_WINDOW;
                depth++;
            }

            // checkmate, no need to continue
            if (tt.isMate(eval)) return MATE_EVAL;
        }
        return eval;
    }

    // Returns the relative static eval of the board
    int static_eval(const chess::Board& board) { return evaluate(board); }

    // Returns the relative eval of the board after quiescence
    int quiescence_eval(chess::Board board) {
        bool halt = false;
        history.clear();
        nodes = 0;
        startSearchTimer(board, 0, 0);

        return quiescence(board, -INT_MAX, INT_MAX, halt);
    }
};  // Raphael
}  // namespace Raphael
