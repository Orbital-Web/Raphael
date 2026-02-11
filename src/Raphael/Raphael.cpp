#include <GameEngine/consts.h>
#include <Raphael/Raphael.h>
#include <Raphael/SEE.h>
#include <Raphael/consts.h>
#include <Raphael/movepick.h>
#include <Raphael/utils.h>

#include <climits>
#include <cmath>
#include <future>
#include <iostream>

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



const string Raphael::version = "3.0.0-dev";

const Raphael::EngineOptions& Raphael::default_params() {
    static EngineOptions opts{
        .hash
        = {"Hash", TranspositionTable::DEF_TABLE_SIZE_MB, 1, TranspositionTable::MAX_TABLE_SIZE_MB},
        .threads = {"Threads", 1, 1, 1},
        .moveoverhead = {"MoveOverhead", 10, 0, 5000},
        .datagen = {"Datagen", false},
        .softnodes = {"Softnodes", false},
        .softhardmult = {"SoftNodeHardLimitMultiplier", 1678, 1, 5000}
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
    for (const auto p : {
             &params.hash,
             &params.threads,
             &params.moveoverhead,
             &params.softhardmult,
         }) {
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
    for (CheckOption* p : {&params.datagen, &params.softnodes}) {
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


void Raphael::set_board(const chess::Board& board) {
    board_ = board;
    net.set_board(board);
}

Raphael::MoveScore Raphael::get_move(
    const i32 t_remain, const i32 t_inc, volatile cge::MouseInfo&, volatile bool& halt
) {
    nodes_ = 0;
    seldepth_ = 0;

    i32 depth = 1;
    i32 score = -INF_SCORE;
    chess::Move bestmove = chess::Move::NO_MOVE;

    SearchStack stack[MAX_DEPTH + 3];
    SearchStack* ss = &stack[2];

    // stop search after an appropriate duration
    start_search_timer(t_remain, t_inc);

    // begin iterative deepening
    while (!halt && depth <= MAX_DEPTH) {
        // max depth override
        if (searchopt.maxdepth != -1 && depth > searchopt.maxdepth) break;

        // soft nodes override
        if (params.softnodes && searchopt.maxnodes != -1 && nodes_ >= searchopt.maxnodes) break;

        // soft time limit
        if (is_soft_time_over(halt)) break;

        // initialize aspiration window
        i32 delta = ASPIRATION_INIT_SIZE;
        i32 alpha = -INF_SCORE;
        i32 beta = INF_SCORE;

        if (depth >= ASPIRATION_DEPTH) {
            alpha = max(score - delta, -INF_SCORE);
            beta = min(score + delta, INF_SCORE);
        }

        // search until score lies between alpha and beta
        i32 iterscore;
        while (!halt) {
            iterscore = negamax<true>(depth, 0, 0, alpha, beta, false, ss, halt);

            if (iterscore <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = max(score - delta, -INF_SCORE);
            } else if (iterscore >= beta)
                beta = min(score + delta, INF_SCORE);
            else
                break;

            delta += delta * ASPIRATION_WIDENING_FACTOR / 16;
        }

        if (halt) break;  // don't use results if timeout

        score = iterscore;
        bestmove = ss->pv.moves[0];
        depth++;

        // print info
        if (UCI) print_uci_info(depth, score, ss);
    }

    // last attempt to get bestmove
    if (!bestmove) bestmove = ss->pv.moves[0];

    // print bestmove
    if (UCI) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "bestmove " << chess::uci::from_move(bestmove) << "\n" << flush;
    }

    // age tt
    tt.do_age();

    // return result
    if (utils::is_mate(score)) return {bestmove, utils::mate_distance(score), true, nodes_};
    return {bestmove, score, false, nodes_};
}

void Raphael::ponder(volatile bool& halt) {
    // just get move with infinite time to fill up the transposition table
    auto inf_before = searchopt.infinite;
    searchopt.infinite = true;
    cge::MouseInfo mouse = {.x = 0, .y = 0, .event = cge::MouseEvent::NONE};
    get_move(0, 0, mouse, halt);
    searchopt.infinite = inf_before;
}


void Raphael::reset() {
    nodes_ = 0;
    seldepth_ = 0;
    tt.clear();
    history.clear();
    searchopt = SearchOptions();
}


void Raphael::start_search_timer(i32 t_remain, i32 t_inc) {
    // if movetime is specified, use that instead
    if (searchopt.movetime != -1) {
        hard_t_ = searchopt.movetime;
        soft_t_ = 0;
        start_t_ = ch::high_resolution_clock::now();
        return;
    }

    // set to infinite if other searchoptions are specified
    if (searchopt.maxdepth != -1 || searchopt.maxnodes != -1 || searchopt.infinite) {
        hard_t_ = 0;
        soft_t_ = 0;
        start_t_ = ch::high_resolution_clock::now();
        return;
    }

    // some guis send negative time, scary...
    if (t_remain < 0) t_remain = 1;
    if (t_inc < 0) t_inc = 0;

    f64 t_base = t_remain * (TIME_FACTOR / 100.0) + t_inc * (INC_FACTOR / 100.0);
    hard_t_ = max<i64>(
        min<i64>(i64(t_base * HARD_TIME_FACTOR / 100.0), t_remain) - params.moveoverhead, 1
    );
    soft_t_ = i64(t_base * SOFT_TIME_FACTOR / 100.0);

    // TODO: do something with searchopt.movestogo

    start_t_ = ch::high_resolution_clock::now();
}

bool Raphael::is_time_over(volatile bool& halt) const {
    // if max nodes is specified, check that instead
    if (searchopt.maxnodes != -1
        && nodes_ >= searchopt.maxnodes * ((params.softnodes) ? params.softhardmult : 1)) {
        halt = true;
        return true;
    }

    // otherwise, check timeover every 2048 nodes
    if (hard_t_ != 0 && !(nodes_ & 2047)) {
        const auto now = ch::high_resolution_clock::now();
        const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t_).count();
        if (dtime >= hard_t_) halt = true;
    }
    return halt;
}

bool Raphael::is_soft_time_over(volatile bool& halt) const {
    // ignore if infinite
    if (soft_t_ == 0) return halt;

    const auto now = ch::high_resolution_clock::now();
    const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t_).count();
    if (dtime >= soft_t_) halt = true;

    return halt;
}


void Raphael::print_uci_info(i32 depth, i32 score, const SearchStack* ss) const {
    const auto now = ch::high_resolution_clock::now();
    const auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t_).count();
    const auto nps = (dtime) ? nodes_ * 1000 / dtime : 0;

    lock_guard<mutex> lock(cout_mutex);
    cout << "info depth " << depth - 1 << " seldepth " << seldepth_ << " time " << dtime
         << " nodes " << nodes_ << " nps " << nps;

    if (utils::is_mate(score))
        cout << " score mate " << utils::mate_distance(score);
    else
        cout << " score cp " << score;

    cout << " hashfull " << tt.hashfull() << " pv " << get_pv_line(ss->pv) << "\n" << flush;
}

string Raphael::get_pv_line(const PVList& pv) const {
    string pvline = "";
    for (i32 i = 0; i < pv.length; i++) pvline += chess::uci::from_move(pv.moves[i]) + " ";
    return pvline;
}


template <bool is_PV>
i32 Raphael::negamax(
    i32 depth,
    const i32 ply,
    const i32 mvidx,
    i32 alpha,
    i32 beta,
    bool cutnode,
    SearchStack* ss,
    volatile bool& halt
) {
    const bool is_root = (ply == 0);
    assert(!is_root || is_PV);
    assert(!is_PV || !cutnode);
    assert(!is_root || !ss->excluded);

    // timeout
    if (is_time_over(halt)) return 0;
    nodes_++;
    if constexpr (is_PV) ss->pv.length = 0;

    if (!is_root) {
        // prevent draw in winning positions
        // technically this ignores checkmate on the 50th move
        if (board_.is_repetition(1) || board_.is_halfmovedraw()) return 0;

        // mate distance pruning
        alpha = max(alpha, -MATE_SCORE + ply);
        beta = min(beta, MATE_SCORE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    // terminal depth or max ply
    if (depth <= 0 || ply >= MAX_DEPTH - 1) return quiescence<is_PV>(ply, mvidx, alpha, beta, halt);

    // probe transposition table
    const auto ttkey = board_.hash();
    auto ttentry = TranspositionTable::ProbedEntry();

    if (!ss->excluded) {
        const bool tthit = tt.get(ttentry, ttkey, ply);

        // tt cutoff
        if (!is_PV && tthit && ttentry.depth >= depth
            && (ttentry.flag == tt.EXACT                                 // exact
                || (ttentry.flag == tt.LOWER && ttentry.score >= beta)   // lower
                || (ttentry.flag == tt.UPPER && ttentry.score <= alpha)  // upper
        ))
            return ttentry.score;
    }
    const auto ttmove = ttentry.move;

    // internal iterative reduction
    if (depth >= IIR_DEPTH && !ss->excluded && (is_PV || cutnode) && !ttmove) depth--;

    const bool in_check = board_.in_check();
    if (!ss->excluded) ss->static_eval = (in_check) ? NONE_SCORE : net.evaluate(ply, board_.stm());
    const bool improving = !in_check && ss->static_eval > (ss - 2)->static_eval;

    // pre-moveloop pruning
    if (!is_PV && !in_check && !ss->excluded) {
        // reverse futility pruning
        const i32 rfp_margin = RFP_DEPTH_SCALE * depth - RFP_IMPROV_SCALE * improving;
        if (depth <= RFP_DEPTH && ss->static_eval - rfp_margin >= beta) return ss->static_eval;

        // razoring
        const i32 razor_margin = RAZORING_MARGIN_BASE + RAZORING_DEPTH_SCALE * depth * depth;
        if (depth <= RAZORING_DEPTH && alpha <= 2048 && ss->static_eval + razor_margin <= alpha) {
            const i32 score = quiescence<false>(ply, mvidx, alpha, alpha + 1, halt);
            if (score <= alpha) return score;
        }

        // null move pruning
        if (depth >= NMP_DEPTH && ss->static_eval >= beta
            && (ss - 1)->move != chess::Move::NULL_MOVE && !board_.is_kingpawn(board_.stm())) {
            tt.prefetch(board_.hash_after<true>(chess::Move::NULL_MOVE));
            net.make_move(ply + 1, chess::Move::NULL_MOVE, board_);
            board_.make_nullmove();
            ss->move = chess::Move::NULL_MOVE;

            const i32 red_depth = depth - NMP_REDUCTION;
            const i32 score = -negamax<false>(
                red_depth, ply + 1, mvidx, -beta, -beta + 1, !cutnode, ss + 1, halt
            );

            board_.unmake_nullmove();

            if (score >= beta) return (utils::is_win(score)) ? beta : score;
        }
    }

    // draw analysis
    if (board_.is_insufficientmaterial()) return 0;

    // initialize move generator
    assert(mvidx < 2 * MAX_DEPTH);
    auto& mvstack = movestack[mvidx];
    mvstack.quietlist.clear();
    mvstack.noisylist.clear();
    auto generator
        = MoveGenerator::negamax(&mvstack.movelist, &board_, &history, ttmove, ss->killer);

    // search
    i32 bestscore = -INF_SCORE;
    chess::Move bestmove = chess::Move::NO_MOVE;
    (ss + 1)->killer = chess::Move::NO_MOVE;
    auto ttflag = tt.UPPER;

    i32 move_searched = 0;
    while (const auto move = generator.next()) {
        if (move == ss->excluded) continue;

        const bool is_quiet = board_.is_quiet(move);
        const auto base_lmr = LMR_TABLE[is_quiet][depth][move_searched + 1];

        // moveloop pruning
        if (!is_root && !utils::is_loss(bestscore) && (!params.datagen || !is_PV)) {
            const auto lmr_depth = max(depth - base_lmr / 128, 0);

            if (is_quiet) {
                // late move pruning
                if (move_searched >= LMP_TABLE[improving][depth]) {
                    generator.skip_quiets();
                    continue;
                }

                // futility pruning
                const i32 futility = ss->static_eval + FP_MARGIN_BASE + FP_DEPTH_SCALE * depth;
                if (!in_check && lmr_depth <= FP_DEPTH && futility <= alpha) {
                    generator.skip_quiets();
                    continue;
                }
            }

            // SEE pruning
            const i32 see_thresh = (is_quiet) ? SEE_QUIET_DEPTH_SCALE * lmr_depth * lmr_depth
                                              : SEE_NOISY_DEPTH_SCALE * depth;
            if (!SEE::see(move, board_, see_thresh)) continue;
        }

        // extensions
        i32 extension = 0;
        if (!is_root && depth >= SE_DEPTH && move == ttmove && !ss->excluded
            && ttentry.depth >= depth - SE_TT_DEPTH && ttentry.flag != tt.UPPER) {
            const i32 s_beta = max(-MATE_SCORE + 1, ttentry.score - depth * SE_DEPTH_MARGIN / 16);
            const i32 s_depth = (depth - 1) / 2;

            ss->excluded = move;
            const i32 score
                = negamax<false>(s_depth, ply, mvidx + 1, s_beta - 1, s_beta, cutnode, ss, halt);
            ss->excluded = chess::Move::NO_MOVE;

            if (score < s_beta) {
                if (!is_PV && score + DE_MARGIN < s_beta)
                    extension = 2;  // double extensions
                else
                    extension = 1;  // singular extensions
            } else if (ttentry.score >= beta)
                extension = -1;  // negative extensions
        }

        tt.prefetch(board_.hash_after<false>(move));
        net.make_move(ply + 1, move, board_);
        board_.make_move(move);
        ss->move = move;
        move_searched++;

        if (is_quiet)
            mvstack.quietlist.push(move);
        else
            mvstack.noisylist.push(move);

        // principle variation search
        i32 score = INT32_MIN;
        const i32 new_depth = depth - 1 + extension;
        if (depth >= LMR_DEPTH && move_searched > LMR_FROMMOVE && is_quiet) {
            // late move reduction
            const i32 red_factor = LMR_TABLE[is_quiet][depth][move_searched] + !is_PV * LMR_NONPV;
            const i32 red_depth = min(max(new_depth - red_factor / 128, 1), new_depth);
            score = -negamax<false>(
                red_depth, ply + 1, mvidx + 1, -alpha - 1, -alpha, true, ss + 1, halt
            );

            if (score > alpha && red_depth < new_depth)
                score = -negamax<false>(
                    new_depth, ply + 1, mvidx + 1, -alpha - 1, -alpha, !cutnode, ss + 1, halt
                );
        } else if (!is_PV || move_searched > 1)
            score = -negamax<false>(
                new_depth, ply + 1, mvidx + 1, -alpha - 1, -alpha, !cutnode, ss + 1, halt
            );

        assert(!(is_PV && move_searched != 1 && score == INT32_MIN));
        if (is_PV && (move_searched == 1 || score > alpha))
            score
                = -negamax<true>(new_depth, ply + 1, mvidx + 1, -beta, -alpha, false, ss + 1, halt);
        assert(score != INT32_MIN);

        board_.unmake_move(move);

        if (score > bestscore) {
            bestscore = score;

            if (score > alpha) {
                alpha = score;
                bestmove = move;
                ttflag = tt.EXACT;

                // update pv
                if constexpr (is_PV) ss->pv.update(move, (ss + 1)->pv);

                if (score >= beta) {
                    ttflag = tt.LOWER;

                    // store killer moves and update history
                    if (is_quiet) {
                        ss->killer = move;

                        const auto quiet_bonus = history.quiet_bonus(depth);
                        const auto quiet_penalty = history.quiet_penalty(depth);

                        for (const auto quietmove : mvstack.quietlist)
                            history.update_quiet(
                                quietmove,
                                board_.stm(),
                                (quietmove == move) ? quiet_bonus : quiet_penalty
                            );
                    }

                    // always update capthist
                    const auto noisy_bonus = history.noisy_bonus(depth);
                    const auto noisy_penalty = history.noisy_penalty(depth);

                    for (const auto noisymove : mvstack.noisylist) {
                        const auto captured = board_.get_captured(noisymove);
                        history.update_noisy(
                            noisymove, captured, (noisymove == move) ? noisy_bonus : noisy_penalty
                        );
                    }

                    break;  // prune
                }
            }
        }
    }

    // terminal analysis
    if (move_searched == 0) return (in_check) ? -MATE_SCORE + ply : 0;  // reward faster mate

    // update transposition table
    if (!halt && !ss->excluded) tt.set(ttkey, bestscore, bestmove, depth, ttflag, ply);

    return bestscore;
}

template <bool is_PV>
i32 Raphael::quiescence(const i32 ply, const i32 mvidx, i32 alpha, i32 beta, volatile bool& halt) {
    // timeout
    if (is_time_over(halt)) return 0;
    nodes_++;
    if (is_PV) seldepth_ = max(seldepth_, ply);

    // max ply
    const bool in_check = board_.in_check();
    if (ply >= MAX_DEPTH - 1) return (in_check) ? 0 : net.evaluate(ply, board_.stm());

    // probe transposition table
    const auto ttkey = board_.hash();
    auto ttentry = TranspositionTable::ProbedEntry();
    const bool tthit = tt.get(ttentry, ttkey, ply);
    // const auto ttmove = ttentry.move;

    // tt cutoff
    if (!is_PV && tthit
        && (ttentry.flag == tt.EXACT                                 // exact
            || (ttentry.flag == tt.LOWER && ttentry.score >= beta)   // lower
            || (ttentry.flag == tt.UPPER && ttentry.score <= alpha)  // upper
    ))
        return ttentry.score;

    // standing pat
    i32 static_eval;
    if (in_check)
        static_eval = -MATE_SCORE + ply;
    else {
        static_eval = net.evaluate(ply, board_.stm());

        if (static_eval >= beta) return static_eval;

        alpha = max(alpha, static_eval);
    }

    // initialize move generator
    assert(mvidx < 2 * MAX_DEPTH);
    auto& mvstack = movestack[mvidx];
    mvstack.quietlist.clear();
    mvstack.noisylist.clear();
    auto generator = MoveGenerator::quiescence(&mvstack.movelist, &board_, &history);

    // search
    i32 bestscore = static_eval;

    const i32 futility = bestscore + QS_FUTILITY_MARGIN;

    while (const auto move = generator.next()) {
        if (!utils::is_loss(bestscore)) {
            // qs futility pruning
            if (!in_check && futility <= alpha && !SEE::see(move, board_, 1)) {
                bestscore = max(bestscore, futility);
                continue;
            }

            // qs see pruning
            if (!SEE::see(move, board_, QS_SEE_THRESH)) continue;
        }

        tt.prefetch(board_.hash_after<false>(move));
        net.make_move(ply + 1, move, board_);
        board_.make_move(move);

        const i32 score = -quiescence<is_PV>(ply + 1, mvidx + 1, -beta, -alpha, halt);
        board_.unmake_move(move);

        if (score > bestscore) {
            bestscore = score;

            if (score > alpha) {
                alpha = score;
                if (score >= beta) break;  // prune
            }
        }
    }

    return bestscore;
}
