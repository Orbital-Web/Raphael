#include <GameEngine/consts.h>
#include <Raphael/Raphael.h>
#include <Raphael/SEE.h>
#include <Raphael/consts.h>
#include <Raphael/movepick.h>
#include <Raphael/utils.h>
#include <Raphael/wdl.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <future>
#include <iostream>

using namespace raphael;
using std::abs;
using std::atomic;
using std::barrier;
using std::clamp;
using std::copy;
using std::cout;
using std::flush;
using std::lock_guard;
using std::make_unique;
using std::max;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::min;
using std::mutex;
using std::string;
using std::swap;
using std::unique_lock;



const string Raphael::version = "4.0.0-dev";

const Raphael::EngineOptions& Raphael::default_params() {
    static EngineOptions opts{
        .hash
        = {"Hash", TranspositionTable::DEF_TABLE_SIZE_MB, 1, TranspositionTable::MAX_TABLE_SIZE_MB},
        .threads = {"Threads", 1, 1, 1024},
        .moveoverhead = {"MoveOverhead", 10, 0, 5000},
        .chess960 = {"UCI_Chess960", false},
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
    : name(name_in), params_(default_params()), tt_(params_.hash) {
    params_.hash.set_callback([this]() { tt_.resize(params_.hash); });
    params_.threads.set_callback([this]() { set_threads(params_.threads); });
    set_threads(params_.threads);
    init_tunables();
}

Raphael::~Raphael() { kill_search(); }


void Raphael::set_option(const std::string& name, i32 value) {
    assert(!is_searching.load(memory_order_acquire));

    for (const auto p : {
             &params_.hash,
             &params_.threads,
             &params_.moveoverhead,
             &params_.softhardmult,
         }) {
        if (!utils::is_case_insensitive_equals(p->name, name)) continue;

        // error checking
        if (value < p->min_val || value > p->max_val) {
            lock_guard<mutex> lock(cout_mutex);
            cout << "info string error: option '" << p->name << "' value must be within min "
                 << p->min_val << " max " << p->max_val << "\n"
                 << flush;
            return;
        }

        // set value
        p->set(value);

        if (ucilevel_ != UciInfoLevel::NONE) {
            lock_guard<mutex> lock(cout_mutex);
            cout << "info string set " << p->name << " to " << value << "\n" << flush;
        }
        return;
    }

    lock_guard<mutex> lock(cout_mutex);
    cout << "info string error: unknown spin option '" << name << "'\n" << flush;
}
void Raphael::set_option(const std::string& name, bool value) {
    assert(!is_searching.load(memory_order_acquire));

    for (CheckOption* p : {&params_.chess960, &params_.datagen, &params_.softnodes}) {
        if (!utils::is_case_insensitive_equals(p->name, name)) continue;

        // set value
        p->set(value);

        if (ucilevel_ != UciInfoLevel::NONE) {
            lock_guard<mutex> lock(cout_mutex);
            if (value)
                cout << "info string enabled " << p->name << "\n" << flush;
            else
                cout << "info string disabled " << p->name << "\n" << flush;
        }
        return;
    }

    lock_guard<mutex> lock(cout_mutex);
    cout << "info string error: unknown check option '" << name << "'\n" << flush;
}

void Raphael::set_uciinfolevel(UciInfoLevel level) {
    assert(!is_searching.load(memory_order_acquire));
    ucilevel_ = level;
}


void Raphael::set_position(const Position<false>& position) {
    assert(!is_searching.load(memory_order_acquire));
    for (auto& tdata : thread_data) tdata.position_.set_position(position);
}

void Raphael::set_board(const chess::Board& board) {
    assert(!is_searching.load(memory_order_acquire));
    for (auto& tdata : thread_data) tdata.position_.set_board(board);
}


void Raphael::set_threads(i32 num_searchers) {
    assert(num_searchers >= 1);
    kill_search();

    assert(!is_searching.load(memory_order_acquire));
    stop.store(false, memory_order_relaxed);
    quit.store(false, memory_order_relaxed);

    start_sync = make_unique<barrier<>>(num_searchers + 1);
    end_sync = make_unique<barrier<>>(num_searchers + 1);

    thread_data.assign(num_searchers, ThreadData{});
    searchers.reserve(num_searchers);
    for (i32 t = 0; t < num_searchers; t++) searchers.emplace_back(&t_search_function, this, t);
}


void Raphael::start_search(const TimeManager::SearchOptions& options) {
    // no need to do compare exchange as all public functions should get called from a single thread
    assert(!is_searching.load(memory_order_acquire));
    is_searching.store(true, memory_order_release);

    end_sync->arrive_and_wait();

    // setup
    tm_.start_timer(options, params_.moveoverhead, (params_.softnodes) ? params_.softhardmult : 0);

    start_sync->arrive_and_wait();
}

bool Raphael::is_search_complete() { return !is_searching.load(memory_order_acquire); }

Raphael::MoveScore Raphael::wait_search() {
    is_searching.wait(true, memory_order_acquire);
    return search_result;
}

Raphael::MoveScore Raphael::search(const TimeManager::SearchOptions& options) {
    start_search(options);
    return wait_search();
}

void Raphael::stop_search() { stop.store(true, memory_order_relaxed); }



i32 Raphael::static_eval(bool corrected) {
    assert(!is_searching.load(memory_order_acquire));
    assert(thread_data.size() >= 1);

    const auto raw_score = thread_data[0].position_.evaluate(!params_.datagen);
    return (corrected) ? adjust_score(0, raw_score) : raw_score;
}


void Raphael::reset() {
    assert(!is_searching.load(memory_order_acquire));
    tt_.clear();  // TODO: multithreaded clearing
    for (auto& tdata : thread_data) tdata.history.clear();
}


void Raphael::kill_search() {
    stop.store(true, memory_order_relaxed);
    quit.store(true, memory_order_relaxed);

    // wait for threads to finish current search
    if (end_sync) end_sync->arrive_and_wait();

    for (auto& t : searchers)
        if (t.joinable()) t.join();

    searchers.clear();
}


void Raphael::t_search_function(i32 thread_id) {
    auto& my_data = thread_data[thread_id];

    while (true) {
        // wait until last search finishes
        end_sync->arrive_and_wait();
        if (quit.load(memory_order_relaxed)) break;

        // wait until uci thread sets up search
        start_sync->arrive_and_wait();

        const auto result = iterative_deepen(thread_id);

        if (thread_id == 0) {
            lock_guard<mutex> lock(cout_mutex);  // FIXME: remove cout mutex
            cout << "bestmove " << chess::uci::from_move(result.move, params_.chess960) << "\n"
                 << flush;
            search_result = result;
            stop.store(true, memory_order_relaxed);
            is_searching.store(false, memory_order_release);
            is_searching.notify_one();
        }
    }
}


void Raphael::print_uci_info(
    i32 depth, i32 score, const chess::Board& board, const SearchStack* ss
) const {
    const auto dtime = tm_.get_time();
    const auto nodes = tm_.get_nodes();
    const auto nps = (dtime) ? nodes * 1000 / dtime : 0;

    lock_guard<mutex> lock(cout_mutex);
    cout << "info depth " << depth << " seldepth " << tm_.get_seldepth() << " time " << dtime
         << " nodes " << nodes << " nps " << nps;

    if (utils::is_mate(score))
        cout << " score mate " << utils::mate_distance(score);
    else {
        // adjust draw randomization
        if (abs(score) < 2) score = 0;

        cout << " score cp " << wdl::normalize_score(score, board);
    }

    const auto wdl_res = wdl::get_wdl(score, board);
    cout << " wdl " << wdl_res.win << " " << wdl_res.draw << " " << wdl_res.loss;

    cout << " hashfull " << tt_.hashfull() << " pv " << get_pv_line(ss->pv) << "\n" << flush;
}

string Raphael::get_pv_line(const PVList& pv) const {
    string pvline = "";
    for (i32 i = 0; i < pv.length; i++)
        pvline += chess::uci::from_move(pv.moves[i], params_.chess960) + " ";
    return pvline;
}


i32 Raphael::adjust_score(i32 thread_id, i32 raw_static_eval) const {
    const auto& position = thread_data[thread_id].position_;
    const auto& history = thread_data[thread_id].history;
    const auto& board = position.board();

    // halfmove scaling
    if (!params_.datagen) raw_static_eval = raw_static_eval * (200 - board.halfmoves()) / 200;

    // corrhist
    raw_static_eval = history.correct(position, raw_static_eval);

    return clamp(raw_static_eval, -MATE_SCORE + 1, MATE_SCORE - 1);
}


Raphael::MoveScore Raphael::iterative_deepen(i32 thread_id) {
    auto& my_data = thread_data[thread_id];
    const auto& board = my_data.position_.board();
    auto ss = &my_data.search_stack[2];
    auto mv = my_data.move_stack;

    i32 score = -INF_SCORE;
    chess::Move bestmove = chess::Move::NO_MOVE;

    // begin iterative deepening
    i32 depth = 1;
    for (; depth <= MAX_DEPTH; depth++) {
        // stop if search stopped
        if (stop.load(memory_order_relaxed)) break;

        // initialize aspiration window
        i32 delta = ASP_INIT_SIZE;
        i32 alpha = -INF_SCORE;
        i32 beta = INF_SCORE;

        if (depth >= ASP_MIN_DEPTH) {
            alpha = max(score - delta, -INF_SCORE);
            beta = min(score + delta, INF_SCORE);
        }

        // search until score lies between alpha and beta
        i32 iterscore;
        while (!stop.load(memory_order_relaxed)) {
            iterscore = negamax<true>(thread_id, depth, 0, alpha, beta, false, ss, mv);

            if (iterscore <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = max(score - delta, -INF_SCORE);
            } else if (iterscore >= beta)
                beta = min(score + delta, INF_SCORE);
            else
                break;

            delta += delta * ASP_WIDENING_FACTOR / 16;
        }

        if (stop.load(memory_order_relaxed)) break;  // don't use results if timeout

        score = iterscore;
        bestmove = ss->pv.moves[0];

        // print info
        if (thread_id == 0 && ucilevel_ == UciInfoLevel::ALL)
            print_uci_info(depth, score, board, ss);

        // soft limit
        if (tm_.is_soft_limit_reached(thread_id, stop, bestmove, score, depth)) break;
    }

    // last attempt to get bestmove
    if (!bestmove) bestmove = ss->pv.moves[0];

    // print last info
    if (thread_id == 0 && ucilevel_ == UciInfoLevel::MINIMAL)
        print_uci_info(depth, score, board, ss);

    // age tt
    tt_.do_age();

    // return result
    if (utils::is_mate(score))
        return {bestmove, utils::mate_distance(score), true, tm_.get_nodes()};
    return {bestmove, score, false, tm_.get_nodes()};
}

template <bool is_PV>
i32 Raphael::negamax(
    const i32 thread_id,
    i32 depth,
    const i32 ply,
    i32 alpha,
    i32 beta,
    bool cutnode,
    SearchStack* ss,
    MoveStack* mv
) {
    auto& my_data = thread_data[thread_id];
    auto& position = my_data.position_;
    auto& history = my_data.history;
    const auto& board = position.board();

    const bool is_root = (ply == 0);
    assert(!is_root || is_PV);
    assert(!is_PV || !cutnode);
    assert(!is_root || !ss->excluded);

    // timeout
    if (tm_.is_hard_limit_reached(halt)) return 0;

    if constexpr (is_PV) ss->pv.length = 0;

    if (!is_root) {
        // prevent draw in winning positions
        // technically this ignores checkmate on the 50th move
        if (position.is_repetition(1) || board.is_halfmovedraw())
            return (tm_.get_nodes(thread_id) & 0x2) - 1;

        // mate distance pruning
        alpha = max(alpha, -MATE_SCORE + ply);
        beta = min(beta, MATE_SCORE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    // terminal depth or max ply
    if (depth <= 0 || ply >= MAX_DEPTH - 1)
        return quiescence<is_PV>(thread_id, ply, alpha, beta, mv);

    // probe transposition table
    const auto ttkey = board.hash();
    auto ttentry = TranspositionTable::ProbedEntry();
    bool tthit = false;

    if (!ss->excluded) {
        tthit = tt_.get(ttentry, ttkey, ply);

        // tt cutoff
        if (!is_PV && tthit && ttentry.depth >= depth
            && (ttentry.flag == tt_.EXACT                                 // exact
                || (ttentry.flag == tt_.LOWER && ttentry.score >= beta)   // lower
                || (ttentry.flag == tt_.UPPER && ttentry.score <= alpha)  // upper
        ))
            return ttentry.score;
    }
    const auto ttmove = ttentry.move;

    // internal iterative reduction
    if (depth >= IIR_MIN_DEPTH && !ss->excluded && (is_PV || cutnode) && !ttmove) depth--;

    const bool in_check = board.in_check();
    i32 raw_static_eval;

    if (!ss->excluded) {
        if (in_check) {
            raw_static_eval = NONE_SCORE;
            ss->static_eval = NONE_SCORE;
        } else {
            if (tthit && ttentry.static_eval != NONE_SCORE)
                raw_static_eval = ttentry.static_eval;
            else
                raw_static_eval = position.evaluate(!params_.datagen);

            ss->static_eval = adjust_score(raw_static_eval);
        }
    }
    const bool improving = !in_check && ss->static_eval > (ss - 2)->static_eval;

    // pre-moveloop pruning
    if (!is_PV && !in_check && !ss->excluded) {
        // reverse futility pruning
        const i32 rfp_margin = RFP_MARGIN_DEPTH_MUL * depth - RFP_MARGIN_IMPROVING * improving;
        if (depth <= RFP_MAX_DEPTH && ss->static_eval - rfp_margin >= beta) return ss->static_eval;

        // razoring
        const i32 razor_margin = RAZOR_MARGIN_BASE + RAZOR_MARGIN_DEPTH_MUL * depth * depth;
        if (depth <= RAZOR_MAX_DEPTH && alpha <= 2048 && ss->static_eval + razor_margin <= alpha) {
            const i32 score = quiescence<false>(thread_id, ply, alpha, alpha + 1, mv);
            if (score <= alpha) return score;
        }

        // null move pruning
        if (depth >= NMP_MIN_DEPTH && ss->static_eval >= beta
            && (ss - 1)->move != chess::Move::NULL_MOVE
            && !(ttentry.flag == tt_.UPPER && ttentry.score < beta)
            && !board.is_kingpawn(board.stm())) {
            tt_.prefetch(board.hash_after<true>(chess::Move::NULL_MOVE));
            position.make_nullmove();
            ss->move = chess::Move::NULL_MOVE;

            i32 red_factor = NMP_RED_BASE;
            red_factor += depth * NMP_RED_DEPTH_MUL;
            red_factor += min<i32>((ss->static_eval - beta) * NMP_RED_EVAL_MUL, NMP_RED_EVAL_MAX);

            const i32 red_depth = depth - red_factor / 128;
            const i32 score = -negamax<false>(
                thread_id, red_depth, ply + 1, -beta, -beta + 1, !cutnode, ss + 1, mv
            );

            position.unmake_nullmove();

            if (score >= beta) return (utils::is_win(score)) ? beta : score;
        }
    }

    // draw analysis
    if (board.is_insufficientmaterial()) return (tm_.get_nodes(thread_id) & 0x2) - 1;

    // initialize move generator
    mv->quietlist.clear();
    mv->noisylist.clear();
    auto generator = MoveGenerator::negamax(&mv->movelist, &position, &history, ttmove, ss->killer);

    // search
    i32 bestscore = -INF_SCORE;
    chess::Move bestmove = chess::Move::NO_MOVE;
    (ss + 1)->killer = chess::Move::NO_MOVE;
    auto ttflag = tt_.UPPER;

    i32 move_searched = 0;
    while (const auto move = generator.next()) {
        if (move == ss->excluded) continue;

        const bool is_quiet = board.is_quiet(move);
        const auto base_lmr = LMR_TABLE[is_quiet][depth][move_searched + 1];
        const auto history = (is_quiet) ? history.get_quietscore(move, position)
                                        : history.get_noisyscore(move, board.get_captured(move));

        // moveloop pruning
        if (!is_root && !utils::is_loss(bestscore) && (!params_.datagen || !is_PV)) {
            const auto lmr_depth = max(depth - base_lmr / 128, 0);

            if (is_quiet) {
                // late move pruning
                if (move_searched >= LMP_TABLE[improving][depth]) {
                    generator.skip_quiets();
                    continue;
                }

                // futility pruning
                const i32 futility
                    = ss->static_eval + FP_MARGIN_BASE + FP_MARGIN_DEPTH_MUL * lmr_depth;
                if (!in_check && lmr_depth <= FP_MAX_DEPTH && futility <= alpha
                    && !board.gives_direct_check(move)) {
                    generator.skip_quiets();
                    continue;
                }
            }

            // SEE pruning
            const i32 see_thresh = (is_quiet) ? SEE_QUIET_DEPTH_MUL * lmr_depth * lmr_depth
                                              : SEE_NOISY_DEPTH_MUL * depth;
            if (!SEE::see(move, board, see_thresh)) continue;
        }

        // extensions
        i32 extension = 0;
        if (!is_root && depth >= SE_MIN_DEPTH && move == ttmove && !ss->excluded
            && ttentry.depth >= depth - SE_MIN_TT_DEPTH && ttentry.flag != tt_.UPPER) {
            const i32 s_beta = max(
                -MATE_SCORE + 1,
                ttentry.score
                    - depth * SE_MARGIN_DEPTH_MUL * ((ttentry.flag == tt_.EXACT) ? 1 : 2) / 16
            );
            const i32 s_depth = (depth - 1) / 2;

            ss->excluded = move;
            const i32 score
                = negamax<false>(thread_id, s_depth, ply, s_beta - 1, s_beta, cutnode, ss, mv + 1);
            ss->excluded = chess::Move::NO_MOVE;

            if (score < s_beta) {
                if (!is_PV && score + DE_MARGIN < s_beta)
                    extension = 2 + (is_quiet && score + TE_MARGIN < s_beta);  // double/triple
                else
                    extension = 1;  // singular extensions
            } else if (s_beta >= beta)
                return s_beta;  // multicut
            else if (ttentry.score >= beta) {
                extension = -1;  // negative extensions
            } else if (cutnode) {
                extension = -1;  // cutnode negative extensions
            }
        }

        const u64 old_nodes = tm_.get_nodes(thread_id);

        tt_.prefetch(board.hash_after<false>(move));
        position.make_move(move);
        ss->move = move;
        move_searched++;
        tm_.inc_nodes();

        const auto& new_board = position.board();
        const bool gives_check = new_board.in_check();

        // principle variation search
        i32 score = INT32_MIN;
        const i32 new_depth = depth - 1 + extension;
        if (depth >= LMR_MIN_DEPTH && move_searched > LMR_FROMMOVE) {
            // late move reduction
            i32 red_factor = LMR_TABLE[is_quiet][depth][move_searched];
            red_factor += !is_PV * LMR_NONPV;
            red_factor += cutnode * LMR_CUTNODE;
            red_factor -= improving * LMR_IMPROVING;
            red_factor -= gives_check * LMR_CHECK;
            red_factor -= history * 128 / ((is_quiet) ? LMR_QUIET_HIST_DIV : LMR_NOISY_HIST_DIV);

            const i32 red_depth = min(max(new_depth - red_factor / 128, 1), new_depth);
            score = -negamax<false>(
                thread_id, red_depth, ply + 1, -alpha - 1, -alpha, true, ss + 1, mv + 1
            );

            if (score > alpha && red_depth < new_depth)
                score = -negamax<false>(
                    thread_id, new_depth, ply + 1, -alpha - 1, -alpha, !cutnode, ss + 1, mv + 1
                );
        } else if (!is_PV || move_searched > 1)
            score = -negamax<false>(
                thread_id, new_depth, ply + 1, -alpha - 1, -alpha, !cutnode, ss + 1, mv + 1
            );

        assert(!(is_PV && move_searched != 1 && score == INT32_MIN));
        if (is_PV && (move_searched == 1 || score > alpha))
            score = -negamax<true>(
                thread_id, new_depth, ply + 1, -beta, -alpha, false, ss + 1, mv + 1
            );
        assert(score != INT32_MIN);

        position.unmake_move();

        if (is_root) tm_.inc_nodes(move, tm_.get_nodes(thread_id) - old_nodes);

        if (score > bestscore) {
            bestscore = score;

            if (score > alpha) {
                alpha = score;
                bestmove = move;
                ttflag = tt_.EXACT;

                // update pv
                if constexpr (is_PV) ss->pv.update(move, (ss + 1)->pv);

                if (score >= beta) {
                    ttflag = tt_.LOWER;

                    const auto history_depth = depth + (!in_check && ss->static_eval <= alpha);

                    if (is_quiet) {
                        // store killer moves and update quiet history
                        ss->killer = move;

                        const auto quiet_bonus = history.quiet_bonus(history_depth);
                        const auto quiet_penalty = history.quiet_penalty(history_depth);

                        history.update_quiet(bestmove, position, quiet_bonus);
                        for (const auto quietmove : mvstack.quietlist)
                            history.update_quiet(quietmove, position, quiet_penalty);
                    } else {
                        // apply capthist bonus
                        const auto noisy_bonus = history.noisy_bonus(history_depth);
                        history.update_noisy(bestmove, board.get_captured(bestmove), noisy_bonus);
                    }

                    // always apply capthist penalty
                    const auto noisy_penalty = history.noisy_penalty(history_depth);
                    for (const auto noisymove : mvstack.noisylist)
                        history.update_noisy(
                            noisymove, board.get_captured(noisymove), noisy_penalty
                        );

                    break;  // prune
                }
            }
        }

        if (move != bestmove) {
            if (is_quiet)
                mvstack.quietlist.push(move);
            else
                mvstack.noisylist.push(move);
        }
    }

    // terminal analysis
    if (move_searched == 0) return (in_check) ? -MATE_SCORE + ply : 0;  // reward faster mate

    // update transposition table
    if (!ss->excluded && !halt.load(memory_order_relaxed)) {
        // update corrhist
        if (!in_check && (bestmove == chess::Move::NO_MOVE || board.is_quiet(bestmove))
            && (ttflag == tt_.EXACT || (ttflag == tt_.LOWER && bestscore > ss->static_eval)
                || (ttflag == tt_.UPPER && bestscore < ss->static_eval)))
            history.update_corrections(position, depth, bestscore, ss->static_eval);
        tt_.set(ttkey, bestscore, raw_static_eval, bestmove, depth, ttflag, ply);
    }

    return bestscore;
}

template <bool is_PV>
i32 Raphael::quiescence(const i32 thread_id, const i32 ply, i32 alpha, i32 beta, MoveStack* mv) {
    auto& my_data = thread_data[thread_id];
    auto& position = my_data.position_;
    const auto& board = position.board();

    // timeout
    if (tm_.is_hard_limit_reached(stop)) return 0;

    if constexpr (is_PV) tm_.update_seldepth(thread_id, ply);

    // max ply
    const bool in_check = board.in_check();
    if (ply >= MAX_DEPTH - 1)
        return (in_check) ? 0 : adjust_score(position.evaluate(!params_.datagen));

    // probe transposition table
    const auto ttkey = board.hash();
    auto ttentry = TranspositionTable::ProbedEntry();
    const bool tthit = tt_.get(ttentry, ttkey, ply);
    const auto ttmove = ttentry.move;

    // tt cutoff
    if (!is_PV && tthit
        && (ttentry.flag == tt_.EXACT                                 // exact
            || (ttentry.flag == tt_.LOWER && ttentry.score >= beta)   // lower
            || (ttentry.flag == tt_.UPPER && ttentry.score <= alpha)  // upper
    ))
        return ttentry.score;

    // standing pat
    i32 raw_static_eval;
    i32 static_eval;

    if (in_check) {
        raw_static_eval = NONE_SCORE;
        static_eval = -MATE_SCORE + ply;
    } else {
        if (tthit && ttentry.static_eval != NONE_SCORE)
            raw_static_eval = ttentry.static_eval;
        else
            raw_static_eval = position.evaluate(!params_.datagen);

        static_eval = adjust_score(raw_static_eval);

        if (static_eval >= beta) return static_eval;

        alpha = max(alpha, static_eval);
    }

    // initialize move generator
    mv->quietlist.clear();
    mv->noisylist.clear();
    auto generator = MoveGenerator::quiescence(&mv->movelist, &position, &history, ttmove);

    // search
    i32 bestscore = static_eval;
    chess::Move bestmove = chess::Move::NO_MOVE;
    auto ttflag = tt_.UPPER;

    const i32 futility = bestscore + QS_FP_MARGIN;

    i32 move_searched = 0;
    while (const auto move = generator.next()) {
        if (!utils::is_loss(bestscore)) {
            // qs late move pruning
            if (move_searched >= QS_MAX_MOVES) break;

            // qs futility pruning
            if (!in_check && futility <= alpha && !board.gives_direct_check(move)
                && !SEE::see(move, board, 1)) {
                bestscore = max(bestscore, futility);
                continue;
            }

            // qs see pruning
            if (!SEE::see(move, board, QS_SEE_THRESH)) continue;
        }

        tt_.prefetch(board.hash_after<false>(move));
        position.make_move(move);
        move_searched++;
        tm_.inc_nodes();

        const i32 score = -quiescence<is_PV>(thread_id, ply + 1, -beta, -alpha, mv + 1);
        position.unmake_move();

        if (!utils::is_loss(score)) generator.skip_quiets();

        if (score > bestscore) {
            bestscore = score;

            if (score > alpha) {
                alpha = score;
                bestmove = move;
                ttflag = tt_.EXACT;

                if (score >= beta) {
                    ttflag = tt_.LOWER;
                    break;  // prune
                }
            }
        }
    }

    // terminal analysis (no stalemate handling)
    if (in_check && move_searched == 0) return -MATE_SCORE + ply;

    // update transposition table
    if (!halt.load(memory_order_relaxed))
        tt_.set(ttkey, bestscore, raw_static_eval, bestmove, 0, ttflag, ply);

    return bestscore;
}
