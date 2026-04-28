#include <Raphael/Raphael.h>
#include <Raphael/SEE.h>
#include <Raphael/consts.h>
#include <Raphael/movepick.h>
#include <Raphael/utils.h>
#include <Raphael/wdl.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
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
using std::make_unique;
using std::max;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::memset;
using std::min;
using std::string;
using std::swap;



const string Raphael::version = "4.0.0";

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


Raphael::Raphael(): params_(default_params()), tt_(params_.hash) {
    params_.hash.set_callback([this]() { tt_.resize(params_.hash, params_.threads); });
    params_.threads.set_callback([this]() { set_threads(params_.threads); });
    set_threads(params_.threads);
    init_tunables();
}

Raphael::~Raphael() { kill_search(); }


void Raphael::set_option(const std::string& name, i32 value) {
    assert(!is_searching_.load(memory_order_acquire));

    for (const auto p :
         {
             &params_.hash,
             &params_.threads,
             &params_.moveoverhead,
             &params_.softhardmult,
         })
    {
        if (!utils::is_case_insensitive_equals(p->name, name)) continue;

        // error checking
        if (value < p->min_val || value > p->max_val) {
            cout << "info string error: option '" << p->name << "' value must be within min "
                 << p->min_val << " max " << p->max_val << "\n"
                 << flush;
            return;
        }

        // set value
        p->set(value);

        if (ucilevel_ != UciInfoLevel::NONE)
            cout << "info string set " << p->name << " to " << value << "\n" << flush;
        return;
    }

    cout << "info string error: unknown spin option '" << name << "'\n" << flush;
}
void Raphael::set_option(const std::string& name, bool value) {
    assert(!is_searching_.load(memory_order_acquire));

    for (CheckOption* p : {&params_.chess960, &params_.datagen, &params_.softnodes}) {
        if (!utils::is_case_insensitive_equals(p->name, name)) continue;

        // set value
        p->set(value);

        if (ucilevel_ != UciInfoLevel::NONE) {
            if (value)
                cout << "info string enabled " << p->name << "\n" << flush;
            else
                cout << "info string disabled " << p->name << "\n" << flush;
        }
        return;
    }

    cout << "info string error: unknown check option '" << name << "'\n" << flush;
}

void Raphael::set_uciinfolevel(UciInfoLevel level) {
    assert(!is_searching_.load(memory_order_acquire));
    ucilevel_ = level;
}


void Raphael::set_position(const Position<false>& position) {
    assert(!is_searching_.load(memory_order_acquire));
    for (auto& tdata : thread_data_) tdata->position_.set_position(position);
}

void Raphael::set_board(const chess::Board& board) {
    assert(!is_searching_.load(memory_order_acquire));
    for (auto& tdata : thread_data_) tdata->position_.set_board(board);
}


void Raphael::set_threads(i32 num_searchers) {
    assert(num_searchers >= 1);
    kill_search();

    assert(!is_searching_.load(memory_order_acquire));
    stop_.store(false, memory_order_relaxed);
    quit_.store(false, memory_order_relaxed);

    init_barrier_ = make_unique<barrier<>>(num_searchers + 1);
    idle_barrier_ = make_unique<barrier<>>(num_searchers + 1);
    search_end_barrier_ = make_unique<barrier<>>(num_searchers);

    tm_.set_threads(num_searchers);
    thread_data_.clear();
    thread_data_.shrink_to_fit();
    thread_data_.resize(num_searchers);
    searchers_.reserve(num_searchers);
    for (i32 t = 0; t < num_searchers; t++)
        searchers_.emplace_back(&Raphael::t_search_function, this, t);
    init_barrier_->arrive_and_wait();
}


void Raphael::start_search(const TimeManager::SearchOptions& options) {
    // no need to do compare exchange as all public functions should get called from a single thread
    assert(!is_searching_.load(memory_order_acquire));
    search_opt_ = options;
    stop_.store(false, memory_order_relaxed);
    is_searching_.store(true, memory_order_release);

    idle_barrier_->arrive_and_wait();
}

bool Raphael::is_search_complete() { return !is_searching_.load(memory_order_acquire); }

Raphael::MoveScore Raphael::wait_search() {
    is_searching_.wait(true, memory_order_acquire);
    return search_result_;
}

Raphael::MoveScore Raphael::search(const TimeManager::SearchOptions& options) {
    start_search(options);
    return wait_search();
}

void Raphael::stop_search() {
    stop_.store(true, memory_order_relaxed);
    is_searching_.wait(true, memory_order_acquire);
}



i32 Raphael::static_eval(bool corrected) {
    assert(!is_searching_.load(memory_order_acquire));
    assert(thread_data_.size() >= 1);

    auto& tdata = *thread_data_[0];
    const auto raw_score = tdata.position_.evaluate(!params_.datagen);
    return (corrected) ? adjust_score(tdata, raw_score) : raw_score;
}


void Raphael::reset() {
    assert(!is_searching_.load(memory_order_acquire));
    tt_.clear(params_.threads);
    for (auto& tdata : thread_data_) tdata->history.clear();
}


void Raphael::kill_search() {
    stop_search();
    quit_.store(true, memory_order_relaxed);

    // let threads proceed so it can quit
    if (idle_barrier_) idle_barrier_->arrive_and_wait();

    for (auto& t : searchers_)
        if (t.joinable()) t.join();

    searchers_.clear();
}


void Raphael::t_search_function(i32 thread_id) {
    thread_data_[thread_id] = make_unique<ThreadData>();
    auto& tdata = *thread_data_[thread_id];
    tdata.thread_id = thread_id;

    init_barrier_->arrive_and_wait();

    while (true) {
        // wait for new search request to arrive
        idle_barrier_->arrive_and_wait();
        if (quit_.load(memory_order_relaxed)) break;

        tm_.start_timer(
            search_opt_,
            thread_id,
            params_.moveoverhead,
            (params_.softnodes) ? params_.softhardmult : 0
        );
        memset(&tdata.search_stack, 0, sizeof(tdata.search_stack));
        const auto result = iterative_deepen(tdata);

        // wait until all threads finish
        if (thread_id == 0) stop_.store(true, memory_order_relaxed);
        search_end_barrier_->arrive_and_wait();

        if (thread_id == 0) {
            search_result_ = result;
            is_searching_.store(false, memory_order_release);
            is_searching_.notify_one();

            if (ucilevel_ != UciInfoLevel::NONE)
                cout << "bestmove " << chess::uci::from_move(result.move, params_.chess960) << "\n"
                     << flush;
        }
    }
}


void Raphael::print_uci_info(
    i32 depth, i32 score, UCIScoreType score_type, const chess::Board& board, const SearchStack* ss
) const {
    const auto dtime = tm_.get_time();
    const auto nodes = tm_.get_nodes();
    const auto nps = (dtime) ? nodes * 1000 / dtime : 0;

    cout << "info depth " << depth << " seldepth " << tm_.get_seldepth() << " time " << dtime
         << " nodes " << nodes << " nps " << nps;

    if (utils::is_mate(score))
        cout << " score mate " << utils::mate_distance(score);
    else {
        // adjust draw randomization
        if (abs(score) < 2) score = 0;

        cout << " score cp " << wdl::normalize_score(score, board);
        if (score_type == UCIScoreType::LOWER)
            cout << " lowerbound";
        else if (score_type == UCIScoreType::UPPER)
            cout << " upperbound";
    }

    const auto wdl_res = wdl::get_wdl(score, board);
    cout << " wdl " << wdl_res.win << " " << wdl_res.draw << " " << wdl_res.loss;

    cout << " hashfull " << tt_.hashfull();
    if (score_type == UCIScoreType::EXACT) cout << " pv " << get_pv_line(ss->pv);
    cout << "\n" << flush;
}

string Raphael::get_pv_line(const PVList& pv) const {
    string pvline = "";
    for (i32 i = 0; i < pv.length; i++)
        pvline += chess::uci::from_move(pv.moves[i], params_.chess960) + " ";
    return pvline;
}


i32 Raphael::adjust_score(const ThreadData& tdata, i32 raw_static_eval) const {
    const auto& position = tdata.position_;
    const auto& history = tdata.history;
    const auto& board = position.board();

    // halfmove scaling
    if (!params_.datagen) raw_static_eval = raw_static_eval * (200 - board.halfmoves()) / 200;

    // corrhist
    raw_static_eval = history.correct(position, raw_static_eval);

    return clamp(raw_static_eval, -MATE_SCORE + 1, MATE_SCORE - 1);
}


Raphael::MoveScore Raphael::iterative_deepen(ThreadData& tdata) {
    const i32 thread_id = tdata.thread_id;
    const auto& board = tdata.position_.board();
    auto ss = &tdata.search_stack[2];
    auto mv = tdata.move_stack;

    i32 score = -INF_SCORE;
    chess::Move bestmove = chess::Move::NO_MOVE;

    // begin iterative deepening
    i32 depth = 1;
    for (; depth <= MAX_DEPTH; depth++) {
        // stop if search stopped
        if (stop_.load(memory_order_relaxed)) break;

        // initialize aspiration window
        i32 delta = ASP_INIT_SIZE;
        i32 alpha = -INF_SCORE;
        i32 beta = INF_SCORE;
        i32 asp_fred = 0;

        if (depth >= ASP_MIN_DEPTH) {
            alpha = max(score - delta, -INF_SCORE);
            beta = min(score + delta, INF_SCORE);
        }

        // search until score lies between alpha and beta
        i32 iterscore;
        while (!stop_.load(memory_order_relaxed)) {
            const i32 asp_fdepth = max(depth * DEPTH_SCALE - asp_fred, DEPTH_SCALE);
            iterscore = negamax<true>(tdata, asp_fdepth, 0, alpha, beta, false, ss, mv);

            if (iterscore <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = max(score - delta, -INF_SCORE);
                asp_fred = 0;
                if (thread_id == 0 && ucilevel_ == UciInfoLevel::ALL)
                    print_uci_info(depth, score, UCIScoreType::UPPER, board, ss);
            } else if (iterscore >= beta) {
                beta = min(score + delta, INF_SCORE);
                asp_fred = min<i32>(asp_fred + ASP_RED, ASP_MAX_RED);
                if (thread_id == 0 && ucilevel_ == UciInfoLevel::ALL)
                    print_uci_info(depth, score, UCIScoreType::LOWER, board, ss);
            } else
                break;

            delta += delta * ASP_WIDENING_FACTOR / 128;
        }

        if (stop_.load(memory_order_relaxed)) break;  // don't use results if timeout

        score = iterscore;
        bestmove = ss->pv.moves[0];

        // print info
        if (thread_id == 0 && ucilevel_ == UciInfoLevel::ALL)
            print_uci_info(depth, score, UCIScoreType::EXACT, board, ss);

        // soft limit
        if (tm_.is_soft_limit_reached(thread_id, stop_, bestmove, score, depth)) break;
    }

    // last attempt to get bestmove
    if (!bestmove) bestmove = ss->pv.moves[0];

    // print last info
    if (thread_id == 0 && ucilevel_ == UciInfoLevel::MINIMAL)
        print_uci_info(depth, score, UCIScoreType::EXACT, board, ss);

    // age tt
    tt_.do_age();

    // return result
    const auto nodes = tm_.get_nodes(thread_id);
    if (utils::is_mate(score)) return {bestmove, utils::mate_distance(score), true, nodes};
    return {bestmove, score, false, nodes};
}

template <bool is_PV>
i32 Raphael::negamax(
    ThreadData& tdata,
    i32 fdepth,
    const i32 ply,
    i32 alpha,
    i32 beta,
    bool cutnode,
    SearchStack* ss,
    MoveStack* mv
) {
    const i32 thread_id = tdata.thread_id;
    auto& position = tdata.position_;
    auto& history = tdata.history;
    const auto& board = position.board();

    const bool is_root = (ply == 0);
    assert(!is_root || is_PV);
    assert(!is_PV || !cutnode);
    assert(!is_root || !ss->excluded);

    // timeout
    if (tm_.is_hard_limit_reached(thread_id, stop_)) return 0;

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
    if (fdepth <= 0 || ply >= MAX_DEPTH - 1) return quiescence<is_PV>(tdata, ply, alpha, beta, mv);

    // probe transposition table
    const auto ttkey = board.hash();
    auto ttentry = TranspositionTable::ProbedEntry();
    bool tthit = false;

    if (!ss->excluded) {
        tthit = tt_.get(ttentry, ttkey, ply);

        // tt cutoff
        if (!is_PV && tthit && ttentry.fdepth >= fdepth && (ttentry.score <= alpha || cutnode)
            && (ttentry.flag == tt_.EXACT                                 // exact
                || (ttentry.flag == tt_.LOWER && ttentry.score >= beta)   // lower
                || (ttentry.flag == tt_.UPPER && ttentry.score <= alpha)  // upper
        ))
            return ttentry.score;

        ss->ttpv = is_PV || ttentry.was_pv;
    }
    const auto ttmove = ttentry.move;

    // internal iterative reduction
    if (fdepth >= IIR_MIN_DEPTH && !ss->excluded && (is_PV || cutnode) && !ttmove)
        fdepth -= IIR_RED;

    const bool in_check = board.in_check();
    i32 raw_static_eval;
    i32 score_estimate;

    if (!ss->excluded) {
        if (in_check) {
            raw_static_eval = NONE_SCORE;
            ss->static_eval = NONE_SCORE;
            score_estimate = ss->static_eval;
        } else {
            if (tthit && ttentry.static_eval != NONE_SCORE)
                raw_static_eval = ttentry.static_eval;
            else if (!tt_.get_static_eval(ttkey, raw_static_eval)) {
                raw_static_eval = position.evaluate(!params_.datagen);
                tt_.set_static_eval(ttkey, raw_static_eval);
            }

            ss->static_eval = adjust_score(tdata, raw_static_eval);
            score_estimate = ss->static_eval;

            if (tthit
                && (ttentry.flag == tt_.EXACT
                    || (ttentry.flag == tt_.LOWER && ttentry.score > ss->static_eval)
                    || (ttentry.flag == tt_.UPPER && ttentry.score < ss->static_eval)))
                score_estimate = ttentry.score;
        }
    }
    const bool improving = !in_check && ss->static_eval > (ss - 2)->static_eval;

    // pre-moveloop pruning
    if (!is_PV && !in_check && !ss->excluded) {
        // hindsight extension
        if ((ss - 1)->freductions >= HINDSIGHT_MIN_RED && (ss - 1)->static_eval != NONE_SCORE
            && ss->static_eval <= -(ss - 1)->static_eval)
            fdepth += HINDSIGHT_EXT;

        // reverse futility pruning
        const i32 rfp_margin
            = RFP_MARGIN_DEPTH_MUL * fdepth / DEPTH_SCALE - RFP_MARGIN_IMPROVING * improving;
        if (fdepth <= RFP_MAX_DEPTH && score_estimate - rfp_margin >= beta) return score_estimate;

        // razoring
        const i32 razor_margin
            = RAZOR_MARGIN_BASE
              + RAZOR_MARGIN_DEPTH_MUL * fdepth / DEPTH_SCALE * fdepth / DEPTH_SCALE;
        if (fdepth <= RAZOR_MAX_DEPTH && alpha <= 2048 && score_estimate + razor_margin <= alpha) {
            const i32 score = quiescence<false>(tdata, ply, alpha, alpha + 1, mv);
            if (score <= alpha) return score;
        }

        // null move pruning
        const i32 nmp_margin
            = max(NMP_MARGIN_BASE - NMP_MARGIN_DEPTH_MUL * fdepth / (DEPTH_SCALE * 128), 0);
        if (fdepth >= NMP_MIN_DEPTH && ply >= tdata.min_nmp_ply
            && ss->static_eval >= beta + nmp_margin && (ss - 1)->move != chess::Move::NULL_MOVE
            && !(ttentry.flag == tt_.UPPER && ttentry.score < beta)
            && !board.is_kingpawn(board.stm()))
        {
            tt_.prefetch(board.hash_after<true>(chess::Move::NULL_MOVE));
            position.make_nullmove();
            ss->move = chess::Move::NULL_MOVE;

            i32 fred = NMP_RED_BASE;
            fred += fdepth * NMP_RED_DEPTH_MUL / 1024;
            fred += min<i32>((ss->static_eval - beta) * NMP_RED_EVAL_MUL, NMP_RED_EVAL_MAX);

            const i32 red_fdepth = fdepth - fred;
            const i32 score = -negamax<false>(
                tdata, red_fdepth, ply + 1, -beta, -beta + 1, !cutnode, ss + 1, mv
            );

            position.unmake_nullmove();

            if (score >= beta) {
                if (fdepth < NMP_VERIF_MIN_DEPTH || tdata.min_nmp_ply > 0)
                    return (utils::is_win(score)) ? beta : score;

                // verification search (disable nmp for a fraction of the depths)
                tdata.min_nmp_ply = ply + NMP_VERIF_DEPTH_FACTOR * red_fdepth / (DEPTH_SCALE * 128);
                const i32 verif_score
                    = negamax<false>(tdata, red_fdepth, ply, beta - 1, beta, true, ss, mv + 1);
                tdata.min_nmp_ply = 0;

                if (verif_score >= beta) return verif_score;
            }
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
        const auto base_lmr = LMR_TABLE[is_quiet][fdepth / DEPTH_SCALE][move_searched + 1];
        const auto hist = (is_quiet) ? history.get_quietscore(move, position)
                                     : history.get_noisyscore(move, board.get_captured(move));

        // moveloop pruning
        if (!is_root && !utils::is_loss(bestscore) && (!params_.datagen || !is_PV)) {
            const i32 fred = base_lmr;
            const auto lmr_fdepth = max(fdepth - fred, 0);

            if (is_quiet) {
                // late move pruning
                if (move_searched >= LMP_TABLE[improving][fdepth / DEPTH_SCALE]) {
                    generator.skip_quiets();
                    continue;
                }

                // futility pruning
                const i32 futility = ss->static_eval + FP_MARGIN_BASE
                                     + FP_MARGIN_DEPTH_MUL * lmr_fdepth / DEPTH_SCALE
                                     + FP_MARGIN_HIST_MUL * hist / HISTORY_MAX;
                if (!in_check && lmr_fdepth <= FP_MAX_DEPTH && futility <= alpha
                    && !board.gives_direct_check(move))
                {
                    generator.skip_quiets();
                    continue;
                }
            }

            // SEE pruning
            const i32 see_thresh = (is_quiet) ? SEE_QUIET_DEPTH_MUL * lmr_fdepth / DEPTH_SCALE
                                                    * lmr_fdepth / DEPTH_SCALE
                                              : SEE_NOISY_DEPTH_MUL * fdepth / DEPTH_SCALE;
            if (!SEE::see(move, board, see_thresh)) continue;
        }

        // extensions
        i32 fext = 0;
        if (!is_root && move == ttmove && !ss->excluded) {
            if (fdepth >= SE_MIN_DEPTH + ss->ttpv * SE_MIN_DEPTH_TTPV
                && ttentry.fdepth >= fdepth - SE_MIN_TT_DEPTH && ttentry.flag != tt_.UPPER)
            {
                const i32 s_beta = max(
                    ttentry.score
                        - SE_MARGIN_DEPTH_MUL * ((ttentry.flag == tt_.EXACT) ? 1 : 2) * fdepth
                              / (DEPTH_SCALE * 128),
                    -MATE_SCORE + 1
                );
                const i32 s_fdepth = (fdepth - DEPTH_SCALE) / 2;

                ss->excluded = move;
                const i32 score
                    = negamax<false>(tdata, s_fdepth, ply, s_beta - 1, s_beta, cutnode, ss, mv + 1);
                ss->excluded = chess::Move::NO_MOVE;

                if (score < s_beta) {
                    if (!is_PV && score + DE_MARGIN < s_beta)
                        fext = SE_EXT + DE_EXT + TE_EXT * (is_quiet && score + TE_MARGIN < s_beta);
                    else
                        fext = SE_EXT;  // singular extensions
                } else if (s_beta >= beta)
                    return s_beta;  // multicut
                else if (cutnode)
                    fext = -CUTNODE_NE_RED;  // cutnode negative extensions
                else if (ttentry.score >= beta)
                    fext = -NE_RED;  // negative extensions

            } else if (
                fdepth <= LDSE_MAX_DEPTH && !in_check && ss->static_eval <= alpha - LDSE_MARGIN
                && ttentry.flag == tt_.LOWER
            )
                fext = LDSE_EXT;  // low depth singular extensions
        }

        const u64 old_nodes = tm_.get_nodes(thread_id);

        tt_.prefetch(board.hash_after<false>(move));
        position.make_move(move);
        ss->move = move;
        move_searched++;
        tm_.inc_nodes(thread_id);

        const auto& new_board = position.board();
        const bool gives_check = new_board.in_check();

        // principle variation search
        i32 score = INT32_MIN;
        i32 new_fdepth = fdepth - DEPTH_SCALE + fext;
        if (fdepth >= LMR_MIN_DEPTH && move_searched > LMR_FROMMOVE) {
            // late move reduction
            i32 fred = base_lmr;
            fred += !is_PV * LMR_NONPV;
            fred += cutnode * LMR_CUTNODE;
            fred -= improving * LMR_IMPROVING;
            fred -= gives_check * LMR_CHECK;
            fred -= hist * DEPTH_SCALE / ((is_quiet) ? LMR_QUIET_HIST_DIV : LMR_NOISY_HIST_DIV);

            ss->freductions = fred;
            const i32 red_fdepth = min(max(new_fdepth - fred, DEPTH_SCALE), new_fdepth);
            score = -negamax<false>(
                tdata, red_fdepth, ply + 1, -alpha - 1, -alpha, true, ss + 1, mv + 1
            );
            ss->freductions = 0;

            if (score > alpha && red_fdepth < new_fdepth) {
                const bool do_deeper = score > bestscore + DO_DEEPER_BASE
                                                   + DO_DEEPER_DEPTH_MUL * new_fdepth / DEPTH_SCALE;
                const bool do_shallower
                    = score < bestscore + DO_SHALLOWER_BASE
                                  + DO_SHALLOWER_DEPTH_MUL * new_fdepth / DEPTH_SCALE;
                new_fdepth += do_deeper * DO_DEEPER_EXT;
                new_fdepth -= do_shallower * DO_SHALLOWER_RED;

                score = -negamax<false>(
                    tdata, new_fdepth, ply + 1, -alpha - 1, -alpha, !cutnode, ss + 1, mv + 1
                );
            }
        } else if (!is_PV || move_searched > 1)
            score = -negamax<false>(
                tdata, new_fdepth, ply + 1, -alpha - 1, -alpha, !cutnode, ss + 1, mv + 1
            );

        assert(!(is_PV && move_searched != 1 && score == INT32_MIN));
        if (is_PV && (move_searched == 1 || score > alpha))
            score
                = -negamax<true>(tdata, new_fdepth, ply + 1, -beta, -alpha, false, ss + 1, mv + 1);
        assert(score != INT32_MIN);

        position.unmake_move();

        if (is_root) tm_.inc_nodes(thread_id, move, tm_.get_nodes(thread_id) - old_nodes);

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

                    const auto history_fdepth
                        = fdepth + (!in_check && ss->static_eval <= alpha) * DEPTH_SCALE;

                    if (is_quiet) {
                        // store killer moves and update quiet history
                        ss->killer = move;

                        const auto quiet_bonus = history.quiet_bonus(history_fdepth);
                        const auto quiet_penalty = history.quiet_penalty(history_fdepth);

                        history.update_quiet(bestmove, position, quiet_bonus);
                        for (const auto quietmove : mv->quietlist)
                            history.update_quiet(quietmove, position, quiet_penalty);
                    } else {
                        // apply capthist bonus
                        const auto noisy_bonus = history.noisy_bonus(history_fdepth);
                        history.update_noisy(bestmove, board.get_captured(bestmove), noisy_bonus);
                    }

                    // always apply capthist penalty
                    const auto noisy_penalty = history.noisy_penalty(history_fdepth);
                    for (const auto noisymove : mv->noisylist)
                        history.update_noisy(
                            noisymove, board.get_captured(noisymove), noisy_penalty
                        );

                    break;  // prune
                }
            }
        }

        if (move != bestmove) {
            if (is_quiet)
                mv->quietlist.push(move);
            else
                mv->noisylist.push(move);
        }
    }

    // terminal analysis
    if (move_searched == 0) return (in_check) ? -MATE_SCORE + ply : 0;  // reward faster mate

    // update transposition table
    if (!ss->excluded && !stop_.load(memory_order_relaxed)) {
        // update corrhist
        if (!in_check && (bestmove == chess::Move::NO_MOVE || board.is_quiet(bestmove))
            && (ttflag == tt_.EXACT || (ttflag == tt_.LOWER && bestscore > ss->static_eval)
                || (ttflag == tt_.UPPER && bestscore < ss->static_eval)))
            history.update_corrections(position, fdepth, bestscore, ss->static_eval);
        tt_.set(ttkey, bestscore, raw_static_eval, bestmove, fdepth, ss->ttpv, ttflag, ply);
    }

    return bestscore;
}

template <bool is_PV>
i32 Raphael::quiescence(ThreadData& tdata, const i32 ply, i32 alpha, i32 beta, MoveStack* mv) {
    const i32 thread_id = tdata.thread_id;
    auto& position = tdata.position_;
    auto& history = tdata.history;
    const auto& board = position.board();

    // timeout
    if (tm_.is_hard_limit_reached(thread_id, stop_)) return 0;

    if constexpr (is_PV) tm_.update_seldepth(thread_id, ply);

    // max ply
    const bool in_check = board.in_check();
    if (ply >= MAX_DEPTH - 1)
        return (in_check) ? 0 : adjust_score(tdata, position.evaluate(!params_.datagen));

    // probe transposition table
    const auto ttkey = board.hash();
    auto ttentry = TranspositionTable::ProbedEntry();
    const bool tthit = tt_.get(ttentry, ttkey, ply);
    const auto ttmove = ttentry.move;
    const bool ttpv = is_PV || ttentry.was_pv;

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
        else if (!tt_.get_static_eval(ttkey, raw_static_eval)) {
            raw_static_eval = position.evaluate(!params_.datagen);
            tt_.set_static_eval(ttkey, raw_static_eval);
        }

        static_eval = adjust_score(tdata, raw_static_eval);

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
                && !SEE::see(move, board, 1))
            {
                bestscore = max(bestscore, futility);
                continue;
            }

            // qs see pruning
            if (!SEE::see(move, board, QS_SEE_THRESH)) continue;
        }

        tt_.prefetch(board.hash_after<false>(move));
        position.make_move(move);
        move_searched++;
        tm_.inc_nodes(thread_id);

        const i32 score = -quiescence<is_PV>(tdata, ply + 1, -beta, -alpha, mv + 1);
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
    if (!stop_.load(memory_order_relaxed))
        tt_.set(ttkey, bestscore, raw_static_eval, bestmove, 0, ttpv, ttflag, ply);

    return bestscore;
}
