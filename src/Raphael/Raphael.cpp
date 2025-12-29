#include <GameEngine/consts.h>
#include <Raphael/Raphael.h>
#include <Raphael/SEE.h>
#include <Raphael/consts.h>
#include <math.h>

#include <chess.hpp>
#include <climits>
#include <future>
#include <iomanip>

using namespace Raphael;
using std::async, std::ref;
using std::cout, std::flush;
using std::fixed, std::setprecision;
using std::max, std::min;
using std::mutex, std::lock_guard;
using std::string;
namespace ch = std::chrono;

#define whiteturn (board.sideToMove() == chess::Color::WHITE)

extern const bool UCI;



string RaphaelNNUE::version = "2.1.0.0";

const RaphaelNNUE::EngineOptions RaphaelNNUE::default_options{
    .hash = {
             .name = "Hash",
             .min = 1,
             .max = TranspositionTable::MAX_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
             .value = TranspositionTable::DEF_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
             },
};

RaphaelNNUE::RaphaelParams::RaphaelParams() {}


RaphaelNNUE::RaphaelNNUE(string name_in): GamePlayer(name_in), tt(default_options.hash.value) {}


void RaphaelNNUE::set_option(SetSpinOption option) {
    if (option.name == "Hash") tt = TranspositionTable(option.value);
}

void RaphaelNNUE::set_searchoptions(SearchOptions options) { searchopt = options; }


chess::Move RaphaelNNUE::get_move(
    chess::Board board,
    const int t_remain,
    const int t_inc,
    volatile cge::MouseInfo& mouse,
    volatile bool& halt
) {
    nodes = 0;
    pvlens[0] = 0;
    seldepth = 0;
    net.set_board(board);

    int depth = 1;
    int eval = -INT_MAX;
    int alpha = -INT_MAX;
    int beta = INT_MAX;
    chess::Move bestmove = chess::Move::NO_MOVE;
    chess::Move prevbest = chess::Move::NO_MOVE;
    int consecutives = 0;

    // stop search after an appropriate duration
    start_search_timer(board, t_remain, t_inc);

    // begin iterative deepening
    while (!halt && depth <= MAX_DEPTH) {
        // max depth override
        if (searchopt.maxdepth != -1 && depth > searchopt.maxdepth) break;

        // stable pv, skip
        if (eval >= params.MIN_SKIP_EVAL && consecutives >= params.PV_STABLE_COUNT
            && !searchopt.infinite)
            halt = true;

        int itereval = negamax(board, depth, 0, params.MAX_EXTENSIONS, alpha, beta, halt);
        if (halt) break;  // don't use results if timeout

        // re-search required
        if ((itereval <= alpha) || (itereval >= beta)) {
            alpha = -INT_MAX;
            beta = INT_MAX;
            continue;
        }

        eval = itereval;
        bestmove = pvtable[0][0];

        // narrow window
        alpha = eval - params.ASPIRATION_WINDOW;
        beta = eval + params.ASPIRATION_WINDOW;
        depth++;

        // count consecutive bestmove
        if (bestmove == prevbest)
            consecutives++;
        else {
            prevbest = bestmove;
            consecutives = 1;
        }

        // checkmate, no need to continue
        if (tt.isMate(eval)) {
            if (UCI) {
                auto now = ch::high_resolution_clock::now();
                auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
                auto nps = (dtime) ? nodes * 1000 / dtime : 0;
                const char* sign = (eval >= 0) ? "" : "-";
                lock_guard<mutex> lock(cout_mutex);
                cout << "info depth " << depth - 1 << " seldepth " << seldepth << " time " << dtime
                     << " nodes " << nodes << " score mate " << sign
                     << (MATE_EVAL - abs(eval) + 1) / 2 << " nps " << nps << " pv " << get_pv_line()
                     << "\n";
                cout << "bestmove " << chess::uci::moveToUci(bestmove) << "\n" << flush;
            }
#ifndef MUTEEVAL
            else {
                // get absolute evaluation (i.e, set to white's perspective)
                const char* sign = (eval >= 0) ? "" : "-";
                lock_guard<mutex> lock(cout_mutex);
                cout << "Eval: " << sign << "#" << (MATE_EVAL - abs(eval) + 1) / 2
                     << "\tNodes: " << nodes << "\n"
                     << flush;
            }
#endif
            halt = true;
            return bestmove;
        } else if (UCI) {
            auto now = ch::high_resolution_clock::now();
            auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
            auto nps = (dtime) ? nodes * 1000 / dtime : 0;
            lock_guard<mutex> lock(cout_mutex);
            cout << "info depth " << depth - 1 << " seldepth " << seldepth << " time " << dtime
                 << " nodes " << nodes << " score cp " << eval << " nps " << nps << " pv "
                 << get_pv_line() << "\n"
                 << flush;
        }
    }

    // last attempt to get bestmove
    if (bestmove == chess::Move::NO_MOVE) bestmove = pvtable[0][0];

    // print bestmove
    if (UCI) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "bestmove " << chess::uci::moveToUci(bestmove) << "\n" << flush;
    }
#ifndef MUTEEVAL
    else {
        // get absolute evaluation (i.e, set to white's perspective)
        if (!whiteturn) eval *= -1;
        lock_guard<mutex> lock(cout_mutex);
        cout << "Eval: " << fixed << setprecision(2) << eval / 100.0f << "\tDepth: " << depth - 1
             << "\tNodes: " << nodes << "\n"
             << flush;
    }
#endif
    return bestmove;
}

void RaphaelNNUE::ponder(chess::Board board, volatile bool& halt) {  // FIXME:
    // ponderdepth = 1;
    // pondereval = 0;
    // itermove = chess::Move::NO_MOVE;
    // search_t = 0;  // infinite time

    // // predict opponent's move from pv
    // auto ttkey = board.hash();
    // auto ttentry = tt.get(ttkey, 0);

    // // no valid response in pv or timeout
    // if (halt || !tt.valid(ttentry, ttkey, 0)) {
    //     consecutives = 1;
    //     return;
    // }

    // // play opponent's move and store key to check for ponderhit
    // board.makeMove(ttentry.move);
    // ponderkey = board.hash();
    // history.clear();

    // // set up nnue board
    // net.set_board(board);

    // int alpha = -INT_MAX;
    // int beta = INT_MAX;
    // nodes = 0;
    // consecutives = 1;

    // // begin iterative deepening for our best response
    // while (!halt && ponderdepth <= MAX_DEPTH) {
    //     int itereval = negamax(board, ponderdepth, 0, params.MAX_EXTENSIONS, alpha, beta, halt);

    //     if (!halt) {
    //         pondereval = itereval;

    //         // re-search required
    //         if ((pondereval <= alpha) || (pondereval >= beta)) {
    //             alpha = -INT_MAX;
    //             beta = INT_MAX;
    //             continue;
    //         }

    //         // narrow window
    //         alpha = pondereval - params.ASPIRATION_WINDOW;
    //         beta = pondereval + params.ASPIRATION_WINDOW;
    //         ponderdepth++;

    //         // count consecutive bestmove
    //         if (itermove == prevPlay)
    //             consecutives++;
    //         else {
    //             prevPlay = itermove;
    //             consecutives = 1;
    //         }
    //     }

    //     // checkmate, no need to continue (but don't edit halt)
    //     if (tt.isMate(pondereval)) break;
    // }
}


string RaphaelNNUE::get_pv_line() const {
    string pvline = "";
    for (int i = 0; i < pvlens[0]; i++) pvline += chess::uci::moveToUci(pvtable[0][i]) + " ";
    return pvline;
}


void RaphaelNNUE::reset() {
    nodes = 0;
    pvlens[0] = 0;
    seldepth = 0;
    tt.clear();
    killers.clear();
    history.clear();
    searchopt = SearchOptions();
}


void RaphaelNNUE::start_search_timer(
    const chess::Board& board, const int t_remain, const int t_inc
) {
    // if movetime is specified, use that instead
    if (searchopt.movetime != -1) {
        search_t = searchopt.movetime;
        start_t = ch::high_resolution_clock::now();
        return;
    }

    // set to infinite if other searchoptions are specified
    if (searchopt.maxdepth != -1 || searchopt.maxnodes != -1 || searchopt.infinite) {
        search_t = 0;
        start_t = ch::high_resolution_clock::now();
        return;
    }

    float n = chess::builtin::popcount(board.occ());
    // 0~1, higher the more time it uses (max at 20 pieces left)
    float ratio = 0.0044f * (n - 32) * (-n / 32) * pow(2.5f + n / 32, 3);
    // use 1~5% of the remaining time based on the ratio + buffered increment
    int duration = t_remain * (0.01f + 0.04f * ratio) + max(t_inc - 30, 1);
    // try to use all of our time if timer resets after movestogo (unless it's 1, then be fast)
    if (searchopt.movestogo > 1) duration += (t_remain - duration) / searchopt.movestogo;
    search_t = min(duration, t_remain);
    start_t = ch::high_resolution_clock::now();
}

bool RaphaelNNUE::is_time_over(volatile bool& halt) const {
    // if max nodes is specified, check that instead
    if (searchopt.maxnodes != -1 && nodes >= searchopt.maxnodes) {
        halt = true;
        return true;
    }
    // otherwise, check timeover every 2048 nodes
    if (search_t && !(nodes & 2047)) {
        auto now = ch::high_resolution_clock::now();
        auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
        if (dtime >= search_t) halt = true;
    }
    return halt;
}


int RaphaelNNUE::negamax(
    chess::Board& board,
    const int depth,
    const int ply,
    const int ext,
    int alpha,
    int beta,
    volatile bool& halt
) {
    // timeout
    if (is_time_over(halt)) return 0;
    nodes++;
    pvlens[ply] = ply;

    if (ply) {
        // prevent draw in winning positions
        // technically this ignores checkmate on the 50th move
        if (board.isRepetition(1) || board.isHalfMoveDraw()) return 0;

        // mate distance pruning
        alpha = max(alpha, -MATE_EVAL + ply);
        beta = min(beta, MATE_EVAL - ply - 1);
        if (alpha >= beta) return alpha;
    }

    // terminal depth
    if (depth <= 0 || ply >= MAX_DEPTH - 1) return quiescence(board, ply, alpha, beta, halt);

    // probe transposition table
    auto ttkey = board.hash();
    auto entry = tt.get(ttkey, ply);
    auto ttmove = (entry.key == ttkey) ? entry.move : chess::Move::NO_MOVE;
    if (ply && tt.valid(entry, ttkey, depth)) {
        if (entry.flag == tt.LOWER)
            alpha = max(alpha, entry.eval);
        else
            beta = min(beta, entry.eval);

        if (alpha >= beta) return entry.eval;  // prune
    }

    // terminal analysis
    if (board.isInsufficientMaterial()) return 0;
    chess::Movelist movelist;
    chess::Movelist quietlist;
    chess::movegen::legalmoves<chess::MoveGenType::ALL>(movelist, board);
    if (movelist.empty()) {
        if (board.inCheck()) return -MATE_EVAL + ply;  // reward faster checkmate
        return 0;
    }

    // one reply extension
    int extension = 0;
    if (movelist.size() > 1)
        order_moves(movelist, ttmove, board, ply);
    else if (ext > 0)
        extension++;

    // search
    int movei = 0;
    int alphaorig = alpha;
    int besteval = -INT_MAX;
    chess::Move bestmove = chess::Move::NO_MOVE;
    killers.clear_ply(ply + 1);

    for (const auto& move : movelist) {
        bool is_quiet = !board.isCapture(move) && move.typeOf() != chess::Move::PROMOTION;
        if (is_quiet) quietlist.add(move);

        net.make_move(ply + 1, move, board);
        board.makeMove(move);

        // check extension
        if (ext > extension && board.inCheck()) extension++;

        bool fullwindow = true;
        int eval;
        // late move reduction with zero window for quiet moves
        if (extension == 0 && depth >= 3 && movei >= params.REDUCTION_FROM && is_quiet) {
            eval = -negamax(board, depth - 2, ply + 1, ext, -alpha - 1, -alpha, halt);
            fullwindow = eval > alpha;
        }
        if (fullwindow)
            eval = -negamax(
                board, depth - 1 + extension, ply + 1, ext - extension, -beta, -alpha, halt
            );

        board.unmakeMove(move);
        extension = 0;
        movei++;

        if (eval > besteval) {
            besteval = eval;
            bestmove = move;

            // update pv
            pvtable[ply][ply] = move;
            for (int i = ply + 1; i < pvlens[ply + 1]; i++) pvtable[ply][i] = pvtable[ply + 1][i];
            pvlens[ply] = pvlens[ply + 1];

            if (eval > alpha) {
                alpha = eval;

                if (eval >= beta) {
                    // store killer moves and update history
                    if (is_quiet) {
                        killers.put(move, ply);
                        history.update(move, quietlist, depth, whiteturn);
                    }
                    break;  // prune
                }
            }
        }
    }

    // update transposition table
    if (!halt) {
        auto flag = tt.INVALID;
        if (besteval >= beta)
            flag = tt.LOWER;
        else
            flag = (alpha != alphaorig) ? tt.EXACT : tt.UPPER;
        tt.set({ttkey, depth, flag, bestmove, besteval}, ply);
    }
    return besteval;
}

int RaphaelNNUE::quiescence(
    chess::Board& board, const int ply, int alpha, int beta, volatile bool& halt
) {
    // timeout
    if (is_time_over(halt)) return 0;
    nodes++;
    seldepth = max(seldepth, ply);

    // get standing pat and prune
    int besteval = net.evaluate(ply, whiteturn);
    if (besteval >= beta) return besteval;
    alpha = max(alpha, besteval);

    // terminal depth
    if (ply >= MAX_DEPTH - 1) return besteval;

    // search
    chess::Movelist movelist;
    chess::movegen::legalmoves<chess::MoveGenType::CAPTURE>(movelist, board);
    order_moves(movelist, chess::Move::NO_MOVE, board, 0);

    for (const auto& move : movelist) {
        net.make_move(ply + 1, move, board);
        board.makeMove(move);

        int eval = -quiescence(board, ply + 1, -beta, -alpha, halt);
        board.unmakeMove(move);

        if (eval > besteval) {
            besteval = eval;

            if (eval > alpha) {
                alpha = eval;
                if (eval >= beta) break;  // prune
            }
        }
    }

    return besteval;
}


void RaphaelNNUE::order_moves(
    chess::Movelist& movelist, const chess::Move& ttmove, const chess::Board& board, const int ply
) const {
    for (auto& move : movelist) score_move(move, ttmove, board, ply);
    movelist.sort();
}

void RaphaelNNUE::score_move(
    chess::Move& move, const chess::Move& ttmove, const chess::Board& board, const int ply
) const {
    // prioritize transposition table move
    if (move == ttmove) {
        move.setScore(INT16_MAX);
        return;
    }

    int16_t score = 0;
    bool is_capture = board.isCapture(move);
    bool is_quiet = !is_capture && move.typeOf() != chess::Move::PROMOTION;

    if (!is_quiet) {
        // noisy moves
        if (is_capture) {
            // captures
            auto attacker = board.at<chess::PieceType>(move.from());
            auto victim = board.at<chess::PieceType>(move.to());
            score += 100 * (int)victim + 5 - (int)attacker;  // MVV/LVA
        }

        score += SEE::goodCapture(move, board, -15) ? params.GOOD_NOISY_FLOOR
                                                    : params.BAD_NOISY_FLOOR;

    } else if (ply > 0 && killers.is_killer(move, ply))
        // killer moves
        score = params.KILLER_FLOOR;
    else
        // quiet moves
        score = history.get(move, whiteturn);

    move.setScore(score);
}
