#include <Raphael/tm.h>
#include <Raphael/tunable.h>

#include <cstring>

using namespace raphael;
using std::abs;
using std::atomic;
using std::max;
using std::memory_order_relaxed;
using std::memset;
using std::min;
namespace ch = std::chrono;



TimeManager::TimeManager() { reset(); }

void TimeManager::start_timer(
    const SearchOptions& searchopt, i32 t_remain, i32 t_inc, i32 t_overhead, i32 softhardmult
) {
    reset();

    // non-standard limits
    if (searchopt.movetime.has_value() || searchopt.maxdepth.has_value()
        || searchopt.maxnodes.has_value() || searchopt.infinite) {
        // either use movetime or don't use tm
        if (searchopt.movetime.has_value()) {
            hard_t_ = *searchopt.movetime;
            soft_t_.reset();
        } else {
            hard_t_.reset();
            soft_t_.reset();
        }

        // set max depth
        if (searchopt.maxdepth.has_value()) max_depth_ = *searchopt.maxdepth;

        // set max nodes (soft and hard)
        if (searchopt.maxnodes.has_value()) {
            if (softhardmult > 0) {
                soft_nodes_ = *searchopt.maxnodes;
                hard_nodes_ = *searchopt.maxnodes * softhardmult;
            } else {
                soft_nodes_.reset();
                hard_nodes_ = *searchopt.maxnodes;
            }
        }

        start_t_ = ch::steady_clock::now();
        return;
    }

    // some guis send negative time, scary...
    if (t_remain < 0) t_remain = 1;
    if (t_inc < 0) t_inc = 0;

    f64 t_base = t_remain * (TIME_FACTOR / 100.0) + t_inc * (INC_FACTOR / 100.0);
    hard_t_ = max<i64>(min<i64>(i64(t_base * HARD_TIME_FACTOR / 100.0), t_remain) - t_overhead, 1);
    soft_t_ = i64(t_base * SOFT_TIME_FACTOR / 100.0);

    // TODO: do something with searchopt.movestogo

    start_t_ = ch::steady_clock::now();
}

i64 TimeManager::get_time() const {
    const auto now = ch::steady_clock::now();
    return ch::duration_cast<ch::milliseconds>(now - start_t_).count();
}


void TimeManager::inc_nodes() { nodes_++; }

void TimeManager::inc_nodes(chess::Move move, u64 count) {
    nodes_per_move_[move.from()][move.to()] += count;
}

u64 TimeManager::get_nodes() const { return nodes_; }

u64 TimeManager::get_nodes(chess::Move move) const {
    return nodes_per_move_[move.from()][move.to()];
}


bool TimeManager::is_hard_limit_reached(atomic<bool>& halt) const {
    // if hard nodes is specified, check node count
    if (hard_nodes_.has_value() && nodes_ >= *hard_nodes_) {
        halt.store(true, memory_order_relaxed);
        return true;
    }

    // if hard time is specified, check timeover every 2048 nodes
    if (hard_t_.has_value() && !(nodes_ & 2047) && get_time() >= hard_t_) {
        halt.store(true, memory_order_relaxed);
        return true;
    }

    return halt.load(memory_order_relaxed);
}

bool TimeManager::is_soft_limit_reached(
    atomic<bool>& halt, chess::Move bestmove, i32 score, i32 depth
) {
    // if soft nodes is specified, check node count
    if (soft_nodes_.has_value() && nodes_ >= *soft_nodes_) {
        halt.store(true, memory_order_relaxed);
        return true;
    }

    // if max depth is specified, check depth (we already finished searching at `depth`)
    if (max_depth_.has_value() && depth >= *max_depth_) {
        halt.store(true, memory_order_relaxed);
        return true;
    }

    // if soft time is specified, check against the adjusted time
    if (soft_t_.has_value()) {
        const auto soft_t_adj = adjust_soft_time(bestmove, score, depth);
        if (get_time() >= soft_t_adj) {
            halt.store(true, memory_order_relaxed);
            return true;
        }
    }

    return halt.load(memory_order_relaxed);
}


void TimeManager::reset() {
    nodes_ = 0;
    memset(nodes_per_move_, 0, sizeof(nodes_per_move_));
    hard_t_.reset();
    soft_t_.reset();
    hard_nodes_.reset();
    soft_nodes_.reset();
    max_depth_.reset();
    prev_bestmove_ = chess::Move::NO_MOVE;
    bestmove_stability_ = 0;
    prev_score_ = 0;
    score_stability_ = 0;
}


i64 TimeManager::adjust_soft_time(chess::Move bestmove, i32 score, i32 depth) {
    assert(soft_t_.has_value());
    f64 factor = 1.0;

    // move stability tm
    if (bestmove == prev_bestmove_)
        bestmove_stability_++;
    else
        bestmove_stability_ = 0;
    prev_bestmove_ = bestmove;

    if (depth >= MV_STAB_TM_MIN_DEPTH)
        factor *= max(
            (MV_STAB_TM_BASE / 100.0) - (MV_STAB_TM_MUL * bestmove_stability_ / 100.0),
            (MV_STAB_TM_MIN / 100.0)
        );

    // score stability tm
    if (abs(score - prev_score_) <= SCORE_STAB_MARGIN)
        score_stability_++;
    else
        score_stability_ = 0;
    prev_score_ = score;

    if (depth >= SCORE_STAB_TM_MIN_DEPTH)
        factor *= max(
            (SCORE_STAB_TM_BASE / 100.0) - (SCORE_STAB_TM_MUL * score_stability_ / 100.0),
            (SCORE_STAB_TM_MIN / 100.0)
        );

    // node tm
    if (depth >= NODE_TM_MIN_DEPTH) {
        const auto ratio = f64(get_nodes(bestmove)) / f64(get_nodes());
        factor *= (NODE_TM_BASE / 100.0) - (NODE_TM_MUL * ratio / 100.0);
    }

    return i64(*soft_t_ * factor);
}
