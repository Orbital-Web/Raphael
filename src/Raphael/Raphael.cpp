#include <GameEngine/consts.h>
#include <Raphael/Raphael.h>
#include <Raphael/SEE.h>
#include <Raphael/consts.h>
#include <Raphael/utils.h>

#include <climits>
#include <cmath>
#include <future>

using namespace raphael;
using std::copy;
using std::cout;
using std::flush;
using std::lock_guard;
using std::max;
using std::min;
using std::mutex;
using std::string;
using std::swap;
namespace ch = std::chrono;

extern const bool UCI;



const string Raphael::version = "2.2.0.0";

const Raphael::EngineOptions& Raphael::default_params() {
    static EngineOptions opts{
        .hash = {
            "Hash",
            TranspositionTable::DEF_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
            1,
            TranspositionTable::MAX_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
            nullptr,
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
            nullptr,
        }
    };
    return opts;
};


void Raphael::PVList::update(const chess::Move move, const PVList& child) {
    moves[0] = move;
    copy(child.moves, child.moves + child.length, moves + 1);
    length = child.length + 1;
    assert(length == 1 || moves[0] != moves[1]);
}


Raphael::Raphael(const string& name_in)
    : GamePlayer(name_in), params(default_params()), tt(params.hash) {
    params.hash.set_callback([this]() { tt.resize(params.hash); });
    init_tunables();
}


void Raphael::set_option(const std::string& name, i32 value) {
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

        lock_guard<mutex> lock(cout_mutex);
        cout << "info string set " << p->name << " to " << value << "\n" << flush;
        return;
    }

    lock_guard<mutex> lock(cout_mutex);
    cout << "info string error: unknown spin option '" << name << "'\n" << flush;
}
void Raphael::set_option(const std::string& name, bool value) {
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

void Raphael::set_searchoptions(SearchOptions options) { searchopt = options; }


Raphael::MoveEval Raphael::get_move(
    chess::Board board,
    const i32 t_remain,
    const i32 t_inc,
    volatile cge::MouseInfo&,
    volatile bool& halt
) {
    nodes = 0;
    seldepth = 0;
    net.set_board(board);

    i32 depth = 1;
    i32 eval = -INT32_MAX;
    i32 alpha = -INT32_MAX;
    i32 beta = INT32_MAX;
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

        const i32 itereval = negamax<true>(board, depth, 0, alpha, beta, ss, halt);
        if (halt) break;  // don't use results if timeout

        // re-search required
        if ((itereval <= alpha) || (itereval >= beta)) {
            alpha = -INT32_MAX;
            beta = INT32_MAX;
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

    // return result
    if (utils::is_mate(eval)) return {bestmove, utils::mate_distance(eval), true, nodes};
    return {bestmove, eval, false, nodes};
}

void Raphael::ponder(chess::Board board, volatile bool& halt) {
    // just get move with infinite time to fill up the transposition table
    cge::MouseInfo mouse = {.x = 0, .y = 0, .event = cge::MouseEvent::NONE};
    get_move(board, 0, 0, mouse, halt);
}


void Raphael::reset() {
    nodes = 0;
    seldepth = 0;
    tt.clear();
    history.clear();
    searchopt = SearchOptions();
}


void Raphael::start_search_timer(const chess::Board& board, i32 t_remain, i32 t_inc) {
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
    i32 duration = t_remain * (0.01f + 0.04f * ratio) + max(t_inc - 30, 1);
    // try to use all of our time if timer resets after movestogo (unless it's 1, then be fast)
    if (searchopt.movestogo > 1) duration += (t_remain - duration) / searchopt.movestogo;
    search_t = min(duration, t_remain);
    start_t = ch::high_resolution_clock::now();
}

bool Raphael::is_time_over(volatile bool& halt) const {
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


void Raphael::print_uci_info(i32 depth, i32 eval, const SearchStack* ss) const {
    const auto now = ch::high_resolution_clock::now();
    const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
    const auto nps = (dtime) ? nodes * 1000 / dtime : 0;

    lock_guard<mutex> lock(cout_mutex);
    cout << "info depth " << depth - 1 << " seldepth " << seldepth << " time " << dtime << " nodes "
         << nodes;

    if (utils::is_mate(eval))
        cout << " score mate " << utils::mate_distance(eval);
    else
        cout << " score cp " << eval;

    cout << " nps " << nps << " pv " << get_pv_line(ss->pv) << "\n" << flush;
}

string Raphael::get_pv_line(const PVList& pv) const {
    string pvline = "";
    for (i32 i = 0; i < pv.length; i++) pvline += chess::uci::moveToUci(pv.moves[i]) + " ";
    return pvline;
}


template <bool is_PV>
i32 Raphael::negamax(
    chess::Board& board,
    const i32 depth,
    const i32 ply,
    i32 alpha,
    i32 beta,
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
    const auto ttentry = tt.get(ttkey, ply);
    const bool tthit = ttentry.key == ttkey;
    const auto ttmove = (tthit) ? ttentry.move : chess::Move::NO_MOVE;

    // tt cutoff
    if (!is_PV && tthit && ttentry.depth >= depth
        && (ttentry.flag == tt.EXACT                                // exact
            || (ttentry.flag == tt.LOWER && ttentry.eval >= beta)   // lower
            || (ttentry.flag == tt.UPPER && ttentry.eval <= alpha)  // upper
    ))
        return ttentry.eval;

    const bool side = utils::stm(board);
    const bool in_check = board.inCheck();
    ss->static_eval = net.evaluate(ply, side);
    const bool improving = !in_check && ss->static_eval > (ss - 2)->static_eval;

    // pre-moveloop pruning
    if (!is_PV && ply && !in_check) {
        // reverse futility pruning
        const i32 rfp_margin = RFP_DEPTH_SCALE * depth - RFP_IMPROV_SCALE * improving;
        if (depth <= RFP_DEPTH && ss->static_eval - rfp_margin >= beta) return ss->static_eval;

        // razoring
        const i32 razor_margin = RAZORING_MARGIN_BASE + RAZORING_DEPTH_SCALE * depth * depth;
        if (depth <= RAZORING_DEPTH && alpha <= 2048 && ss->static_eval + razor_margin <= alpha) {
            const i32 eval = quiescence(board, ply, alpha, alpha + 1, halt);
            if (eval <= alpha) return eval;
        }

        // null move pruning
        if (depth >= NMP_DEPTH && ss->static_eval >= beta
            && (ss - 1)->move != chess::Move::NULL_MOVE
            && board.hasNonPawnMaterial(board.sideToMove())) {
            net.make_move(ply + 1, chess::Move::NULL_MOVE, board);
            board.makeNullMove();
            ss->move = chess::Move::NULL_MOVE;

            const i32 red_depth = depth - NMP_REDUCTION;
            const i32 eval
                = -negamax<false>(board, red_depth, ply + 1, -beta, -beta + 1, ss + 1, halt);

            board.unmakeNullMove();

            if (eval >= beta) return (utils::is_win(eval)) ? beta : eval;
        }
    }

    // terminal analysis
    if (board.isInsufficientMaterial()) return 0;
    auto& mvstack = movestack[ply];
    mvstack.quietlist.clear();
    mvstack.noisylist.clear();
    chess::movegen::legalmoves<chess::movegen::MoveGenType::ALL>(mvstack.movelist, board);
    if (mvstack.movelist.empty()) return (in_check) ? -MATE_EVAL + ply : 0;  // reward faster mate


    // one reply extension
    i32 extension = 0;
    if (mvstack.movelist.size() > 1)
        score_moves(mvstack.movelist, ttmove, board, ss);
    else
        extension++;

    // search
    const i32 alphaorig = alpha;
    i32 besteval = -INT32_MAX;
    chess::Move bestmove = chess::Move::NO_MOVE;
    (ss + 1)->killer = chess::Move::NO_MOVE;
    bool skip_quiets = false;

    i32 move_searched = 0;
    for (i32 _movei = 0; _movei < mvstack.movelist.size(); _movei++) {
        const auto move = pick_move(_movei, mvstack.movelist);
        const bool is_quiet = utils::is_quiet(move, board);

        if (is_quiet && skip_quiets) continue;

        // moveloop pruning
        if (ply && !utils::is_loss(besteval)) {
            // late move pruning
            if (move_searched >= LMP_TABLE[improving][depth]) {
                skip_quiets = true;
                continue;
            }

            // futility pruning
            const i32 futility = ss->static_eval + FP_MARGIN_BASE + FP_DEPTH_SCALE * depth;
            if (!in_check && is_quiet && depth <= FP_DEPTH && futility <= alpha) {
                skip_quiets = true;
                continue;
            }

            // SEE pruning
            const i32 see_thresh = (is_quiet) ? SEE_QUIET_DEPTH_SCALE * depth * depth
                                              : SEE_NOISY_DEPTH_SCALE * depth;
            if (!SEE::see(move, board, see_thresh)) continue;
        }

        tt.prefetch(board.zobristAfter<false>(move));
        net.make_move(ply + 1, move, board);
        board.makeMove(move);
        ss->move = move;
        move_searched++;

        if (is_quiet)
            mvstack.quietlist.add(move);
        else
            mvstack.noisylist.add(move);

        // check extension
        if (board.inCheck()) extension++;

        // principle variation search
        i32 eval = INT32_MIN;
        const i32 new_depth = depth - 1 + extension;
        if (depth >= LMR_DEPTH && move_searched > LMR_FROMMOVE && is_quiet) {
            // late move reduction
            const i32 red_factor = LMR_TABLE[is_quiet][depth][move_searched] + !is_PV * LMR_NONPV;
            const i32 red_depth = max(new_depth - red_factor / 128, 0);
            eval = -negamax<false>(board, red_depth, ply + 1, -alpha - 1, -alpha, ss + 1, halt);
            if (eval > alpha && red_depth < new_depth)
                eval = -negamax<false>(board, new_depth, ply + 1, -alpha - 1, -alpha, ss + 1, halt);
        } else if (!is_PV || move_searched > 1)
            eval = -negamax<false>(board, new_depth, ply + 1, -alpha - 1, -alpha, ss + 1, halt);

        assert(!(is_PV && move_searched != 1 && eval == INT32_MIN));
        if (is_PV && (move_searched == 1 || eval > alpha))
            eval = -negamax<true>(board, new_depth, ply + 1, -beta, -alpha, ss + 1, halt);
        assert(eval != INT32_MIN);

        board.unmakeMove(move);
        extension = 0;

        if (halt) return 0;

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

                        const auto quiet_bonus = history.quiet_bonus(depth);
                        const auto quiet_penalty = history.quiet_penalty(depth);

                        for (const auto quietmove : mvstack.quietlist)
                            history.update_quiet(
                                quietmove, side, (quietmove == move) ? quiet_bonus : quiet_penalty
                            );
                    }

                    // always update capthist
                    const auto noisy_bonus = history.noisy_bonus(depth);
                    const auto noisy_penalty = history.noisy_penalty(depth);

                    for (const auto noisymove : mvstack.noisylist) {
                        const auto captured = utils::piece_captured(noisymove, board);
                        history.update_noisy(
                            noisymove, captured, (noisymove == move) ? noisy_bonus : noisy_penalty
                        );
                    }

                    break;  // prune
                }
            }
        }
    }

    // update transposition table
    auto flag = tt.INVALID;
    if (besteval >= beta)
        flag = tt.LOWER;
    else
        flag = (alpha != alphaorig) ? tt.EXACT : tt.UPPER;
    tt.set({ttkey, depth, flag, bestmove, besteval}, ply);
    return besteval;
}

i32 Raphael::quiescence(
    chess::Board& board, const i32 ply, i32 alpha, i32 beta, volatile bool& halt
) {
    // timeout
    if (is_time_over(halt)) return 0;
    nodes++;
    seldepth = max(seldepth, ply);

    // get standing pat and prune
    const bool side = utils::stm(board);
    i32 besteval = net.evaluate(ply, side);
    if (besteval >= beta) return besteval;
    alpha = max(alpha, besteval);

    // terminal depth
    if (ply >= MAX_DEPTH - 1) return besteval;

    // search
    auto& mvstack = movestack[ply];
    mvstack.quietlist.clear();
    mvstack.noisylist.clear();
    chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(mvstack.movelist, board);
    score_moves(mvstack.movelist, board);

    const i32 futility = besteval + QS_FUTILITY_MARGIN;
    const bool in_check = board.inCheck();

    for (i32 _movei = 0; _movei < mvstack.movelist.size(); _movei++) {
        const auto move = pick_move(_movei, mvstack.movelist);

        // qs futility pruning
        if (!in_check && futility <= alpha && !SEE::see(move, board, 1)) {
            besteval = max(besteval, futility);
            continue;
        }

        // qs see pruning
        if (!SEE::see(move, board, QS_SEE_THRESH)) continue;

        net.make_move(ply + 1, move, board);
        board.makeMove(move);

        const i32 eval = -quiescence(board, ply + 1, -beta, -alpha, halt);
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


void Raphael::score_moves(
    chess::Movelist& movelist,
    const chess::Move& ttmove,
    const chess::Board& board,
    const SearchStack* ss
) const {
    const bool side = utils::stm(board);

    for (auto& move : movelist) {
        if (move == ttmove)
            move.setScore(TT_MOVE_FLOOR);  // tt move

        else if (!utils::is_quiet(move, board)) {
            // noisy moves
            const auto victim = utils::piece_captured(move, board);

            i16 score = 0;
            score += history.get_noisyscore(move, victim) / CAPTHIST_DIVISOR;
            score += SEE_TABLE[victim];
            if (move.typeOf() == chess::Move::PROMOTION)
                score += SEE_TABLE[move.promotionType()] - SEE_TABLE[(i32)chess::PieceType::PAWN];

            score += SEE::see(move, board, GOOD_NOISY_SEE_THRESH) ? GOOD_NOISY_FLOOR
                                                                  : BAD_NOISY_FLOOR;
            move.setScore(score);
        } else if (move == ss->killer)
            move.setScore(KILLER_FLOOR);  // killer moves
        else
            move.setScore(history.get_quietscore(move, side));  // quiet moves
    }
}

void Raphael::score_moves(chess::Movelist& movelist, const chess::Board& board) const {
    for (auto& move : movelist) {
        // assume noisy
        const auto victim = utils::piece_captured(move, board);

        i16 score = 0;
        score += history.get_noisyscore(move, victim) / CAPTHIST_DIVISOR;
        score += SEE_TABLE[victim];
        if (move.typeOf() == chess::Move::PROMOTION)
            score += SEE_TABLE[move.promotionType()] - SEE_TABLE[(i32)chess::PieceType::PAWN];
        move.setScore(score);
    }
}

chess::Move Raphael::pick_move(i32 movei, chess::Movelist& movelist) const {
    i32 besti = movei;
    auto bestscore = movelist[movei].score();

    for (i32 i = movei + 1; i < movelist.size(); i++) {
        if (movelist[i].score() > bestscore) {
            bestscore = movelist[i].score();
            besti = i;
        }
    }

    if (besti != movei) swap(movelist[movei], movelist[besti]);

    return movelist[movei];
}
