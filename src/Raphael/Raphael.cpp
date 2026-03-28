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
using std::atomic;
using std::copy;
using std::cout;
using std::flush;
using std::lock_guard;
using std::max;
using std::memory_order_relaxed;
using std::min;
using std::mutex;
using std::string;
using std::swap;



const string Raphael::version = "3.3.0-dev";

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
    : name(name_in), params_(default_params()), tt_(params_.hash) {
    params_.hash.set_callback([this]() { tt_.resize(params_.hash); });
    init_tunables();
}


void Raphael::set_option(const std::string& name, i32 value) {
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
    for (CheckOption* p : {&params_.datagen, &params_.softnodes}) {
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

void Raphael::set_searchoptions(TimeManager::SearchOptions options) { searchopt_ = options; }

void Raphael::set_uciinfolevel(UciInfoLevel level) { ucilevel_ = level; }


void Raphael::set_position(const Position<false>& position) { position_.set_position(position); }

void Raphael::set_board(const chess::Board& board) { position_.set_board(board); }


Raphael::MoveScore Raphael::get_move(const i32 t_remain, const i32 t_inc, atomic<bool>& halt) {
    seldepth_ = 0;

    i32 score = -INF_SCORE;
    chess::Move bestmove = chess::Move::NO_MOVE;

    SearchStack stack[MAX_DEPTH + 3];
    SearchStack* ss = &stack[2];

    // stop search after an appropriate duration
    tm_.start_timer(
        searchopt_,
        t_remain,
        t_inc,
        params_.moveoverhead,
        (params_.softnodes) ? params_.softhardmult : 0
    );

    // begin iterative deepening
    i32 depth = 1;
    for (; depth <= MAX_DEPTH; depth++) {
        // stop if search stopped
        if (halt.load(memory_order_relaxed)) break;

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
        while (!halt.load(memory_order_relaxed)) {
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

        if (halt.load(memory_order_relaxed)) break;  // don't use results if timeout

        score = iterscore;
        bestmove = ss->pv.moves[0];

        // print info
        if (ucilevel_ == UciInfoLevel::ALL) print_uci_info(depth, score, ss);

        // soft limit
        if (tm_.is_soft_limit_reached(halt, bestmove, score, depth)) break;
    }

    // last attempt to get bestmove
    if (!bestmove) bestmove = ss->pv.moves[0];

    // print last info
    if (ucilevel_ == UciInfoLevel::MINIMAL) print_uci_info(depth, score, ss);

    // age tt and record score for time management
    tt_.do_age();
    tm_.record_score(score);

    // return result
    if (utils::is_mate(score))
        return {bestmove, utils::mate_distance(score), true, tm_.get_nodes()};
    return {bestmove, score, false, tm_.get_nodes()};
}

void Raphael::ponder(atomic<bool>& halt) {
    // just get move with infinite time to fill up the transposition table
    const auto prev_searchopt = searchopt_;
    searchopt_ = {.infinite = true};
    get_move(0, 0, halt);
    searchopt_ = prev_searchopt;
}


i32 Raphael::static_eval(bool corrected) {
    const auto raw_score = position_.evaluate();
    return (corrected) ? history_.correct(position_.board(), raw_score) : raw_score;
}


void Raphael::reset() {
    seldepth_ = 0;
    tt_.clear();
    history_.clear();
    tm_.hard_reset();
    searchopt_ = {};
}


void Raphael::print_uci_info(i32 depth, i32 score, const SearchStack* ss) const {
    const auto dtime = tm_.get_time();
    const auto nps = (dtime) ? tm_.get_nodes() * 1000 / dtime : 0;

    lock_guard<mutex> lock(cout_mutex);
    cout << "info depth " << depth << " seldepth " << seldepth_ << " time " << dtime << " nodes "
         << tm_.get_nodes() << " nps " << nps;

    if (utils::is_mate(score))
        cout << " score mate " << utils::mate_distance(score);
    else
        cout << " score cp " << score;

    cout << " hashfull " << tt_.hashfull() << " pv " << get_pv_line(ss->pv) << "\n" << flush;
}

string Raphael::get_pv_line(const PVList& pv) const {
    const bool chess960 = position_.board().chess960();
    string pvline = "";
    for (i32 i = 0; i < pv.length; i++)
        pvline += chess::uci::from_move(pv.moves[i], chess960) + " ";
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
    atomic<bool>& halt
) {
    const bool is_root = (ply == 0);
    assert(!is_root || is_PV);
    assert(!is_PV || !cutnode);
    assert(!is_root || !ss->excluded);

    // timeout
    if (tm_.is_hard_limit_reached(halt)) return 0;

    if constexpr (is_PV) ss->pv.length = 0;

    const auto& board = position_.board();

    if (!is_root) {
        // prevent draw in winning positions
        // technically this ignores checkmate on the 50th move
        if (position_.is_repetition(1) || board.is_halfmovedraw()) return 0;

        // mate distance pruning
        alpha = max(alpha, -MATE_SCORE + ply);
        beta = min(beta, MATE_SCORE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    // terminal depth or max ply
    if (depth <= 0 || ply >= MAX_DEPTH - 1) return quiescence<is_PV>(ply, mvidx, alpha, beta, halt);

    // probe transposition table
    const auto ttkey = board.hash();
    auto ttentry = TranspositionTable::ProbedEntry();

    if (!ss->excluded) {
        const bool tthit = tt_.get(ttentry, ttkey, ply);

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
    if (depth >= IIR_DEPTH && !ss->excluded && (is_PV || cutnode) && !ttmove) depth--;

    const bool in_check = board.in_check();
    if (!ss->excluded)
        ss->static_eval = (in_check) ? NONE_SCORE : history_.correct(board, position_.evaluate());
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
            && (ss - 1)->move != chess::Move::NULL_MOVE && !board.is_kingpawn(board.stm())) {
            tt_.prefetch(board.hash_after<true>(chess::Move::NULL_MOVE));
            position_.make_nullmove();
            ss->move = chess::Move::NULL_MOVE;

            const i32 red_depth = depth - NMP_REDUCTION;
            const i32 score = -negamax<false>(
                red_depth, ply + 1, mvidx, -beta, -beta + 1, !cutnode, ss + 1, halt
            );

            position_.unmake_nullmove();

            if (score >= beta) return (utils::is_win(score)) ? beta : score;
        }
    }

    // draw analysis
    if (board.is_insufficientmaterial()) return 0;

    // initialize move generator
    assert(mvidx < 2 * MAX_DEPTH);
    auto& mvstack = movestack_[mvidx];
    mvstack.quietlist.clear();
    mvstack.noisylist.clear();
    auto generator
        = MoveGenerator::negamax(&mvstack.movelist, &position_, &history_, ttmove, ss->killer);

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
        const auto history = (is_quiet) ? history_.get_quietscore(move, position_)
                                        : history_.get_noisyscore(move, board.get_captured(move));

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
                const i32 futility = ss->static_eval + FP_MARGIN_BASE + FP_DEPTH_SCALE * lmr_depth;
                if (!in_check && lmr_depth <= FP_DEPTH && futility <= alpha
                    && !board.gives_direct_check(move)) {
                    generator.skip_quiets();
                    continue;
                }
            }

            // SEE pruning
            const i32 see_thresh = (is_quiet) ? SEE_QUIET_DEPTH_SCALE * lmr_depth * lmr_depth
                                              : SEE_NOISY_DEPTH_SCALE * depth;
            if (!SEE::see(move, board, see_thresh)) continue;
        }

        // extensions
        i32 extension = 0;
        if (!is_root && depth >= SE_DEPTH && move == ttmove && !ss->excluded
            && ttentry.depth >= depth - SE_TT_DEPTH && ttentry.flag != tt_.UPPER) {
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
            } else if (s_beta >= beta)
                return s_beta;  // multicut
            else if (ttentry.score >= beta) {
                extension = -1;  // negative extensions
            } else if (cutnode) {
                extension = -1;  // cutnode negative extensions
            }
        }

        const u64 old_nodes = tm_.get_nodes();

        tt_.prefetch(board.hash_after<false>(move));
        position_.make_move(move);
        ss->move = move;
        move_searched++;
        tm_.inc_nodes();

        const auto& new_board = position_.board();
        const bool gives_check = new_board.in_check();

        // principle variation search
        i32 score = INT32_MIN;
        const i32 new_depth = depth - 1 + extension;
        if (depth >= LMR_DEPTH && move_searched > LMR_FROMMOVE) {
            // late move reduction
            i32 red_factor = LMR_TABLE[is_quiet][depth][move_searched];
            red_factor += !is_PV * LMR_NONPV;
            red_factor += cutnode * LMR_CUTNODE;
            red_factor -= improving * LMR_IMPROVING;
            red_factor -= gives_check * LMR_CHECK;
            red_factor
                -= history * 128 / ((is_quiet) ? LMR_QUIET_HIST_DIVISOR : LMR_NOISY_HIST_DIVISOR);

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

        position_.unmake_move();

        if (is_root) tm_.inc_nodes(move, tm_.get_nodes() - old_nodes);

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

                        const auto quiet_bonus = history_.quiet_bonus(history_depth);
                        const auto quiet_penalty = history_.quiet_penalty(history_depth);

                        history_.update_quiet(bestmove, position_, quiet_bonus);
                        for (const auto quietmove : mvstack.quietlist)
                            history_.update_quiet(quietmove, position_, quiet_penalty);
                    } else {
                        // apply capthist bonus
                        const auto noisy_bonus = history_.noisy_bonus(history_depth);
                        history_.update_noisy(bestmove, board.get_captured(bestmove), noisy_bonus);
                    }

                    // always apply capthist penalty
                    const auto noisy_penalty = history_.noisy_penalty(history_depth);
                    for (const auto noisymove : mvstack.noisylist)
                        history_.update_noisy(
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
            history_.update_corrections(board, depth, bestscore, ss->static_eval);
        tt_.set(ttkey, bestscore, bestmove, depth, ttflag, ply);
    }

    return bestscore;
}

template <bool is_PV>
i32 Raphael::quiescence(const i32 ply, const i32 mvidx, i32 alpha, i32 beta, atomic<bool>& halt) {
    // timeout
    if (tm_.is_hard_limit_reached(halt)) return 0;

    if constexpr (is_PV) seldepth_ = max(seldepth_, ply);

    const auto& board = position_.board();

    // max ply
    const bool in_check = board.in_check();
    if (ply >= MAX_DEPTH - 1) return (in_check) ? 0 : history_.correct(board, position_.evaluate());

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
    i32 static_eval;
    if (in_check)
        static_eval = -MATE_SCORE + ply;
    else {
        static_eval = history_.correct(board, position_.evaluate());

        if (static_eval >= beta) return static_eval;

        alpha = max(alpha, static_eval);
    }

    // initialize move generator
    assert(mvidx < 2 * MAX_DEPTH);
    auto& mvstack = movestack_[mvidx];
    mvstack.quietlist.clear();
    mvstack.noisylist.clear();
    auto generator = MoveGenerator::quiescence(&mvstack.movelist, &position_, &history_, ttmove);

    // search
    i32 bestscore = static_eval;
    chess::Move bestmove = chess::Move::NO_MOVE;
    auto ttflag = tt_.UPPER;

    const i32 futility = bestscore + QS_FUTILITY_MARGIN;

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
        position_.make_move(move);
        move_searched++;
        tm_.inc_nodes();

        const i32 score = -quiescence<is_PV>(ply + 1, mvidx + 1, -beta, -alpha, halt);
        position_.unmake_move();

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
    if (!halt.load(memory_order_relaxed)) tt_.set(ttkey, bestscore, bestmove, 0, ttflag, ply);

    return bestscore;
}
