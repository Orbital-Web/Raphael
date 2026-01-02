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
using std::copy;
using std::cout, std::flush;
using std::fixed, std::setprecision;
using std::max, std::min;
using std::mutex, std::lock_guard;
using std::string;
using std::swap;
namespace ch = std::chrono;

#define whiteturn (board.sideToMove() == chess::Color::WHITE)

extern const bool UCI;



const string RaphaelNNUE::version = "2.1.0.0";

RaphaelNNUE::EngineOptions RaphaelNNUE::params{
    .hash = {
             .name = "Hash",
             .min = 1,
             .max = TranspositionTable::MAX_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
             .def = TranspositionTable::DEF_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
             .value = TranspositionTable::DEF_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
             },
};


void RaphaelNNUE::PVList::update(const chess::Move move, const PVList& child) {
    moves[0] = move;
    copy(child.moves, child.moves + child.length, moves + 1);
    length = child.length + 1;
    assert(length == 1 || moves[0] != moves[1]);
}


RaphaelNNUE::RaphaelNNUE(string name_in)
    : GamePlayer(name_in),
      tt(params.hash),
      history(
          params.HISTORY_BONUS_MAX,
          params.HISTORY_BONUS_OFFSET,
          params.HISTORY_BONUS_SCALE,
          params.HISTORY_MAX
      ) {}


void RaphaelNNUE::set_option(SetSpinOption option) {
    if (option.name == "Hash") {
        params.hash.set(option.value);
        tt.resize(option.value);
    }
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
    seldepth = 0;
    net.set_board(board);

    int depth = 1;
    int eval = -INT_MAX;
    int alpha = -INT_MAX;
    int beta = INT_MAX;
    chess::Move bestmove = chess::Move::NO_MOVE;

    SearchStack stack[MAX_DEPTH + 3];
    SearchStack* ss = &stack[2];

    // stop search after an appropriate duration
    start_search_timer(board, t_remain, t_inc);
    const bool exit_on_mate = !searchopt.infinite && searchopt.maxdepth == -1
                              && searchopt.movetime == -1 && searchopt.maxnodes == -1;

    // begin iterative deepening
    while (!halt && depth <= MAX_DEPTH) {
        // max depth override
        if (searchopt.maxdepth != -1 && depth > searchopt.maxdepth) break;

        const int itereval = negamax<true>(board, depth, 0, alpha, beta, ss, halt);
        if (halt) break;  // don't use results if timeout

        // re-search required
        if ((itereval <= alpha) || (itereval >= beta)) {
            alpha = -INT_MAX;
            beta = INT_MAX;
            continue;
        }

        eval = itereval;
        bestmove = ss->pv.moves[0];

        // narrow window
        alpha = eval - params.ASPIRATION_WINDOW;
        beta = eval + params.ASPIRATION_WINDOW;
        depth++;

        // checkmate, no need to continue
        if (tt.isMate(eval)) {
            if (UCI) {
                const auto now = ch::high_resolution_clock::now();
                const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
                const auto nps = (dtime) ? nodes * 1000 / dtime : 0;
                const char* sign = (eval >= 0) ? "" : "-";
                lock_guard<mutex> lock(cout_mutex);
                cout << "info depth " << depth - 1 << " seldepth " << seldepth << " time " << dtime
                     << " nodes " << nodes << " score mate " << sign
                     << (MATE_EVAL - abs(eval) + 1) / 2 << " nps " << nps << " pv "
                     << get_pv_line(ss->pv) << "\n"
                     << flush;
            }
#ifndef MUTEEVAL
            else {
                // get absolute evaluation (i.e, set to white's perspective)
                const char* sign = ((eval >= 0) == whiteturn) ? "" : "-";
                lock_guard<mutex> lock(cout_mutex);
                cout << "Eval: " << sign << "#" << (MATE_EVAL - abs(eval) + 1) / 2
                     << "\tNodes: " << nodes << "\n"
                     << flush;
            }
#endif
            if (exit_on_mate) halt = true;
        } else if (UCI) {
            const auto now = ch::high_resolution_clock::now();
            const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
            const auto nps = (dtime) ? nodes * 1000 / dtime : 0;
            lock_guard<mutex> lock(cout_mutex);
            cout << "info depth " << depth - 1 << " seldepth " << seldepth << " time " << dtime
                 << " nodes " << nodes << " score cp " << eval << " nps " << nps << " pv "
                 << get_pv_line(ss->pv) << "\n"
                 << flush;
        }
    }

    // last attempt to get bestmove
    if (bestmove == chess::Move::NO_MOVE) bestmove = ss->pv.moves[0];

    // print bestmove
    if (UCI) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "bestmove " << chess::uci::moveToUci(bestmove) << "\n" << flush;
    }
#ifndef MUTEEVAL
    else if (!tt.isMate(eval)) {
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
}


void RaphaelNNUE::reset() {
    nodes = 0;
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

    const float n = board.occ().count();
    // 0~1, higher the more time it uses (max at 20 pieces left)
    const float ratio = 0.0044f * (n - 32) * (-n / 32) * pow(2.5f + n / 32, 3);
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
        const auto now = ch::high_resolution_clock::now();
        const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
        if (dtime >= search_t) halt = true;
    }
    return halt;
}


string RaphaelNNUE::get_pv_line(const PVList& pv) const {
    string pvline = "";
    for (int i = 0; i < pv.length; i++) pvline += chess::uci::moveToUci(pv.moves[i]) + " ";
    return pvline;
}


template <bool is_PV>
int RaphaelNNUE::negamax(
    chess::Board& board,
    const int depth,
    const int ply,
    int alpha,
    int beta,
    SearchStack* ss,
    volatile bool& halt
) {
    // timeout
    if (is_time_over(halt)) return 0;
    nodes++;
    if constexpr (is_PV) ss->pv.length = 0;

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
    const auto ttkey = board.hash();
    const auto entry = tt.get(ttkey, ply);
    const auto ttmove = (entry.key == ttkey) ? entry.move : chess::Move::NO_MOVE;
    if (ply && tt.valid(entry, ttkey, depth)) {
        if (entry.flag == tt.LOWER)
            alpha = max(alpha, entry.eval);
        else
            beta = min(beta, entry.eval);

        if (alpha >= beta) return entry.eval;  // prune
    }

    const bool in_check = board.inCheck();
    ss->static_eval = (entry.key == ttkey) ? entry.eval : net.evaluate(ply, whiteturn);

    // pre-moveloop pruning
    if (!is_PV && ply && !in_check) {
        // reverse futility pruning
        const int rfp_margin = params.RFP_MARGIN * depth;
        if (depth <= params.RFP_DEPTH && ss->static_eval - rfp_margin >= beta)
            return ss->static_eval;

        // null move pruning
        const auto side = board.sideToMove();
        const bool non_pk
            = ((board.us(side) ^ board.pieces(chess::PieceType::PAWN, side)).count() > 1);
        if (depth >= params.NMP_DEPTH && non_pk && ss->static_eval >= beta
            && (ss - 1)->move != chess::Move::NULL_MOVE && non_pk) {
            board.makeNullMove();
            net.make_move(ply + 1, chess::Move::NULL_MOVE, board);
            ss->move = chess::Move::NULL_MOVE;

            const int red_depth = depth - params.NMP_REDUCTION;
            const int eval
                = -negamax<false>(board, red_depth, ply + 1, -beta, -beta + 1, ss + 1, halt);

            board.unmakeNullMove();

            if (eval >= beta) return (beta > MATE_EVAL) ? beta : eval;
        }
    }

    // terminal analysis
    if (board.isInsufficientMaterial()) return 0;
    chess::Movelist movelist;
    chess::Movelist quietlist;
    chess::movegen::legalmoves<chess::movegen::MoveGenType::ALL>(movelist, board);
    if (movelist.empty()) return (in_check) ? -MATE_EVAL + ply : 0;  // reward faster mate


    // one reply extension
    int extension = 0;
    if (movelist.size() > 1)
        score_moves(movelist, ttmove, board, ply);
    else
        extension++;

    // search
    const int alphaorig = alpha;
    int besteval = -INT_MAX;
    chess::Move bestmove = chess::Move::NO_MOVE;
    killers.clear_ply(ply + 1);

    for (int movei = 0; movei < movelist.size(); movei++) {
        const auto move = pick_move(movei, movelist);
        const bool is_quiet = !board.isCapture(move) && move.typeOf() != chess::Move::PROMOTION;
        if (is_quiet) quietlist.add(move);

        net.make_move(ply + 1, move, board);
        board.makeMove(move);
        ss->move = move;

        // check extension
        if (board.inCheck()) extension++;

        // principle variation search
        int eval;
        const int new_depth = depth - 1 + extension;
        if (depth >= 3 && movei >= params.REDUCTION_FROM && is_quiet) {
            // late move reduction
            const int red_depth = max(new_depth - 1, 0);
            eval = -negamax<false>(board, red_depth, ply + 1, -alpha - 1, -alpha, ss + 1, halt);
            if (eval > alpha && red_depth < new_depth)
                eval = -negamax<false>(board, new_depth, ply + 1, -alpha - 1, -alpha, ss + 1, halt);
        } else if (!is_PV || movei >= 1)
            eval = -negamax<false>(board, new_depth, ply + 1, -alpha - 1, -alpha, ss + 1, halt);

        if (is_PV && (movei == 0 || eval > alpha))
            eval = -negamax<true>(board, new_depth, ply + 1, -beta, -alpha, ss + 1, halt);

        board.unmakeMove(move);
        extension = 0;

        if (eval > besteval) {
            besteval = eval;
            bestmove = move;

            if (eval > alpha) {
                alpha = eval;

                // update pv
                if constexpr (is_PV) ss->pv.update(move, (ss + 1)->pv);

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
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(movelist, board);
    score_moves(movelist, chess::Move::NO_MOVE, board, ply);

    for (int movei = 0; movei < movelist.size(); movei++) {
        const auto move = pick_move(movei, movelist);

        // delta pruning
        if (SEE::estimate(move, board) + besteval + params.DELTA_THRESHOLD < alpha) continue;

        net.make_move(ply + 1, move, board);
        board.makeMove(move);

        const int eval = -quiescence(board, ply + 1, -beta, -alpha, halt);
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


void RaphaelNNUE::score_moves(
    chess::Movelist& movelist, const chess::Move& ttmove, const chess::Board& board, const int ply
) const {
    for (auto& move : movelist) {
        // prioritize transposition table move
        if (move == ttmove) {
            move.setScore(INT16_MAX);
            continue;
        }

        int16_t score = 0;
        const bool is_capture = board.isCapture(move);
        const bool is_quiet = !is_capture && move.typeOf() != chess::Move::PROMOTION;

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
}

chess::Move RaphaelNNUE::pick_move(const int movei, chess::Movelist& movelist) {
    int besti = movei;
    auto bestscore = movelist[movei].score();

    for (int i = movei + 1; i < movelist.size(); i++) {
        if (movelist[i].score() > bestscore) {
            bestscore = movelist[i].score();
            besti = i;
        }
    }

    if (besti != movei) swap(movelist[movei], movelist[besti]);

    return movelist[movei];
}
