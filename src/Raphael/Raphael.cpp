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
using std::async;
using std::copy;
using std::cout;
using std::fixed;
using std::flush;
using std::lock_guard;
using std::max;
using std::min;
using std::mutex;
using std::ref;
using std::setprecision;
using std::string;
using std::swap;
namespace ch = std::chrono;

#define whiteturn (board.sideToMove() == chess::Color::WHITE)

extern const bool UCI;



const string RaphaelNNUE::version = "2.1.0.0";

const RaphaelNNUE::EngineOptions RaphaelNNUE::default_params{
    .hash = {
        "Hash",
        TranspositionTable::DEF_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
        1,
        TranspositionTable::MAX_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
    },
    .softnodes = {
        "Softnodes",
        false,
    },
    .softhardmult = {
        "SoftNodeHardLimitMultiplier",
        1678,
        1,
        5000,
    }
};


void RaphaelNNUE::PVList::update(const chess::Move move, const PVList& child) {
    moves[0] = move;
    copy(child.moves, child.moves + child.length, moves + 1);
    length = child.length + 1;
    assert(length == 1 || moves[0] != moves[1]);
}


RaphaelNNUE::RaphaelNNUE(const string& name_in)
    : GamePlayer(name_in), params(default_params), tt(default_params.hash) {}


void RaphaelNNUE::set_option(const std::string& name, int value) {
    for (const auto p : {&params.hash, &params.softhardmult}) {
        if (p->name != name) continue;

        // error checking
        if (value < p->min || value > p->max) {
            lock_guard<mutex> lock(cout_mutex);
            cout << "info string error: option '" << p->name << "' value must be within min "
                 << p->min << " max " << p->max << "\n"
                 << flush;
            return;
        }

        // set value
        p->set(value);
        if (p->name == params.hash.name) tt.resize(value);

        lock_guard<mutex> lock(cout_mutex);
        cout << "info string set " << p->name << " to " << value << "\n" << flush;
        return;
    }

    lock_guard<mutex> lock(cout_mutex);
    cout << "info string error: unknown spin option '" << name << "'\n" << flush;
}
void RaphaelNNUE::set_option(const std::string& name, bool value) {
    for (CheckOption* p : {&params.softnodes}) {
        if (p->name != name) continue;

        // set value
        p->set(value);

        lock_guard<mutex> lock(cout_mutex);
        if (value)
            cout << "info string enabled " << p->name << "\n" << flush;
        else
            cout << "info string disabled " << p->name << "\n" << flush;
        return;
    }

    lock_guard<mutex> lock(cout_mutex);
    cout << "info string error: unknown check option '" << name << "'\n" << flush;
}

void RaphaelNNUE::set_searchoptions(SearchOptions options) { searchopt = options; }


RaphaelNNUE::MoveEval RaphaelNNUE::get_move(
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

    // begin iterative deepening
    while (!halt && depth <= MAX_DEPTH) {
        // max depth override
        if (searchopt.maxdepth != -1 && depth > searchopt.maxdepth) break;

        // soft nodes override
        if (params.softnodes && searchopt.maxnodes != -1 && nodes >= searchopt.maxnodes) break;

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
        alpha = eval - ASPIRATION_WINDOW;
        beta = eval + ASPIRATION_WINDOW;
        depth++;

        // print info
        if (UCI) print_uci_info(depth, eval, ss);
    }

    // last attempt to get bestmove
    if (bestmove == chess::Move::NO_MOVE) bestmove = ss->pv.moves[0];

    // print bestmove
    if (UCI) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "bestmove " << chess::uci::moveToUci(bestmove) << "\n" << flush;
    }
#ifndef MUTEEVAL
    else {
        lock_guard<mutex> lock(cout_mutex);
        if (TranspositionTable::is_mate(eval)) {
            const int mate_dist
                = (((eval >= 0) == whiteturn) ? 1 : -1) * (MATE_EVAL - abs(eval) + 1) / 2;
            cout << "Eval: #" << mate_dist;
        } else {
            const auto eval_p = ((whiteturn) ? 1 : -1) * eval / 100.0f;
            cout << "Eval: " << fixed << setprecision(2) << eval_p;
        }

        cout << "\tDepth: " << depth - 1 << "\tNodes: " << nodes << "\n" << flush;
    }
#endif
    return {bestmove, eval};
}

void RaphaelNNUE::ponder(chess::Board board, volatile bool& halt) {  // FIXME:
}


void RaphaelNNUE::reset() {
    nodes = 0;
    seldepth = 0;
    tt.clear();
    history.clear();
    searchopt = SearchOptions();
}


void RaphaelNNUE::start_search_timer(const chess::Board& board, int t_remain, int t_inc) {
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
    if (searchopt.maxnodes != -1
        && nodes >= searchopt.maxnodes * ((params.softnodes) ? params.softhardmult : 1)) {
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


void RaphaelNNUE::print_uci_info(int depth, int eval, const SearchStack* ss) const {
    const auto now = ch::high_resolution_clock::now();
    const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
    const auto nps = (dtime) ? nodes * 1000 / dtime : 0;

    lock_guard<mutex> lock(cout_mutex);
    cout << "info depth " << depth - 1 << " seldepth " << seldepth << " time " << dtime << " nodes "
         << nodes;

    if (TranspositionTable::is_mate(eval)) {
        const int mate_dist = ((eval >= 0) ? 1 : -1) * (MATE_EVAL - abs(eval) + 1) / 2;
        cout << " score mate " << mate_dist;
    } else
        cout << " score cp " << eval;

    cout << " nps " << nps << " pv " << get_pv_line(ss->pv) << "\n" << flush;
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
    ss->static_eval = net.evaluate(ply, whiteturn);

    // pre-moveloop pruning
    if (!is_PV && ply && !in_check) {
        // reverse futility pruning
        const int rfp_margin = RFP_MARGIN * depth;
        if (depth <= RFP_DEPTH && ss->static_eval - rfp_margin >= beta) return ss->static_eval;

        // null move pruning
        const auto side = board.sideToMove();
        const bool non_pk
            = ((board.us(side) ^ board.pieces(chess::PieceType::PAWN, side)).count() > 1);
        if (depth >= NMP_DEPTH && non_pk && ss->static_eval >= beta
            && (ss - 1)->move != chess::Move::NULL_MOVE && non_pk) {
            board.makeNullMove();
            net.make_move(ply + 1, chess::Move::NULL_MOVE, board);
            ss->move = chess::Move::NULL_MOVE;

            const int red_depth = depth - NMP_REDUCTION;
            const int eval
                = -negamax<false>(board, red_depth, ply + 1, -beta, -beta + 1, ss + 1, halt);

            board.unmakeNullMove();

            if (eval >= beta) return (eval >= MATE_EVAL - 1000) ? beta : eval;
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
        score_moves(movelist, ttmove, board, ss);
    else
        extension++;

    // search
    const int alphaorig = alpha;
    int besteval = -INT_MAX;
    chess::Move bestmove = chess::Move::NO_MOVE;
    (ss + 1)->killer = chess::Move::NO_MOVE;

    for (int movei = 0; movei < movelist.size(); movei++) {
        const auto move = pick_move(movei, movelist);
        const bool is_quiet = !board.isCapture(move) && move.typeOf() != chess::Move::PROMOTION;
        if (is_quiet) quietlist.add(move);

        net.make_move(ply + 1, move, board);
        board.makeMove(move);
        tt.prefetch(board.hash());
        ss->move = move;

        // check extension
        if (board.inCheck()) extension++;

        // principle variation search
        int eval;
        const int new_depth = depth - 1 + extension;
        if (depth >= 3 && movei >= REDUCTION_FROM && is_quiet) {
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
                        ss->killer = move;
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
    score_moves(movelist, board);

    for (int movei = 0; movei < movelist.size(); movei++) {
        const auto move = pick_move(movei, movelist);

        // delta pruning
        if (SEE::estimate(move, board) + besteval + DELTA_THRESHOLD < alpha) continue;

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
    chess::Movelist& movelist,
    const chess::Move& ttmove,
    const chess::Board& board,
    const SearchStack* ss
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

            score += SEE::good_capture(move, board, -GOOD_NOISY_THRESH) ? GOOD_NOISY_FLOOR
                                                                        : BAD_NOISY_FLOOR;

        } else if (move == ss->killer)
            // killer moves
            score = KILLER_FLOOR;
        else
            // quiet moves
            score = history.get(move, whiteturn);

        move.setScore(score);
    }
}

void RaphaelNNUE::score_moves(chess::Movelist& movelist, const chess::Board& board) const {
    for (auto& move : movelist) {
        int16_t score = 0;

        // assume noisy
        if (board.isCapture(move)) {
            // captures
            auto attacker = board.at<chess::PieceType>(move.from());
            auto victim = board.at<chess::PieceType>(move.to());
            score += 100 * (int)victim + 5 - (int)attacker;  // MVV/LVA
        }

        score += SEE::good_capture(move, board, -GOOD_NOISY_THRESH) ? GOOD_NOISY_FLOOR
                                                                    : BAD_NOISY_FLOOR;

        move.setScore(score);
    }
}

chess::Move RaphaelNNUE::pick_move(int movei, chess::Movelist& movelist) const {
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
