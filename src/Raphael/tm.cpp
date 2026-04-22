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
using std::vector;
namespace ch = std::chrono;



TimeManager::TimeManager() {}

void TimeManager::set_threads(i32 num_searchers) { thread_tm_ = vector<ThreadTM>(num_searchers); }

void TimeManager::start_timer(
    const SearchOptions& searchopt, i32 thread_id, i32 t_overhead, i32 softhardmult
) {
    // reset
    thread_tm_[thread_id].nodes.store(0, memory_order_relaxed);
    thread_tm_[thread_id].seldepth.store(0, memory_order_relaxed);
    if (thread_id != 0) return;

    hard_t_.reset();
    soft_t_.reset();
    hard_nodes_.reset();
    soft_nodes_.reset();
    max_depth_.reset();

    memset(nodes_per_move_, 0, sizeof(nodes_per_move_));
    prev_bestmove_ = chess::Move::NO_MOVE;
    bestmove_stability_ = 0;
    prev_score_ = 0;
    score_stability_ = 0;

    // non-standard limits
    if (searchopt.movetime.has_value() || searchopt.maxdepth.has_value()
        || searchopt.maxnodes.has_value() || searchopt.infinite) {
        // either use movetime or don't use tm
        if (searchopt.movetime.has_value()) hard_t_ = *searchopt.movetime;

        // set max depth
        if (searchopt.maxdepth.has_value()) max_depth_ = *searchopt.maxdepth;

        // set max nodes (soft and hard)
        if (searchopt.maxnodes.has_value()) {
            if (softhardmult > 0) {
                soft_nodes_ = *searchopt.maxnodes;
                hard_nodes_ = *searchopt.maxnodes * softhardmult;
            } else
                hard_nodes_ = *searchopt.maxnodes;
        }

        start_t_ = ch::steady_clock::now();
        return;
    }

    auto t_remain = searchopt.t_remain;
    auto t_inc = searchopt.t_inc;

    // some guis send negative time, scary...
    if (t_remain < 0) t_remain = 1;
    if (t_inc < 0) t_inc = 0;

    f64 t_base = t_remain * (TIME_FACTOR / 1000.0) + t_inc * (INC_FACTOR / 1000.0);
    hard_t_ = max<i64>(min<i64>(i64(t_base * HARD_TIME_FACTOR / 1000.0), t_remain) - t_overhead, 1);
    soft_t_ = i64(t_base * SOFT_TIME_FACTOR / 1000.0);

    // TODO: do something with searchopt.movestogo

    start_t_ = ch::steady_clock::now();
}

i64 TimeManager::get_time() const {
    const auto now = ch::steady_clock::now();
    return ch::duration_cast<ch::milliseconds>(now - start_t_).count();
}


void TimeManager::inc_nodes(i32 thread_id) {
    // only one thread will write, so this is faster
    auto& nodes = thread_tm_[thread_id].nodes;
    nodes.store(nodes.load(memory_order_relaxed) + 1, memory_order_relaxed);
}

void TimeManager::inc_nodes(i32 thread_id, chess::Move move, u64 count) {
    if (thread_id != 0) return;
    nodes_per_move_[move.from()][move.to()] += count;
}

u64 TimeManager::get_nodes() const {
    u64 total = 0;
    for (const auto& tdata : thread_tm_) total += tdata.nodes.load(memory_order_relaxed);
    return total;
}

u64 TimeManager::get_nodes(i32 thread_id) const {
    return thread_tm_[thread_id].nodes.load(memory_order_relaxed);
}

u64 TimeManager::get_nodes(i32 thread_id, chess::Move move) const {
    if (thread_id != 0) return 0;
    return nodes_per_move_[move.from()][move.to()];
}


void TimeManager::update_seldepth(i32 thread_id, i32 depth) {
    // only one thread will write, so we can get away with this
    auto& seldepth = thread_tm_[thread_id].seldepth;
    seldepth.store(max(seldepth.load(memory_order_relaxed), depth), memory_order_relaxed);
}

i32 TimeManager::get_seldepth() const {
    i32 seldepth = 0;
    for (const auto& tdata : thread_tm_)
        seldepth = max(seldepth, tdata.seldepth.load(memory_order_relaxed));
    return seldepth;
}


bool TimeManager::is_hard_limit_reached(i32 thread_id, atomic<bool>& stop) const {
    // only main thread tracks tm (for now)
    if (thread_id != 0) return stop.load(memory_order_relaxed);

    // if hard nodes is specified, check node count
    if (hard_nodes_.has_value() && get_nodes(thread_id) >= *hard_nodes_) {
        stop.store(true, memory_order_relaxed);
        return true;
    }

    // if hard time is specified, check timeover every 2048 nodes
    if (hard_t_.has_value() && !(get_nodes(thread_id) & 2047) && get_time() >= hard_t_) {
        stop.store(true, memory_order_relaxed);
        return true;
    }

    return stop.load(memory_order_relaxed);
}

bool TimeManager::is_soft_limit_reached(
    i32 thread_id, atomic<bool>& stop, chess::Move bestmove, i32 score, i32 depth
) {
    // only main thread tracks tm (for now)
    if (thread_id != 0) return stop.load(memory_order_relaxed);

    // if soft nodes is specified, check node count
    if (soft_nodes_.has_value() && get_nodes(thread_id) >= *soft_nodes_) {
        stop.store(true, memory_order_relaxed);
        return true;
    }

    // if max depth is specified, check depth (we already finished searching at `depth`)
    if (max_depth_.has_value() && depth >= *max_depth_) {
        stop.store(true, memory_order_relaxed);
        return true;
    }

    // if soft time is specified, check against the adjusted time
    if (soft_t_.has_value()) {
        const auto soft_t_adj = adjust_soft_time(thread_id, bestmove, score, depth);
        if (get_time() >= soft_t_adj) {
            stop.store(true, memory_order_relaxed);
            return true;
        }
    }

    return stop.load(memory_order_relaxed);
}


i64 TimeManager::adjust_soft_time(i32 thread_id, chess::Move bestmove, i32 score, i32 depth) {
    assert(thread_id == 0);
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
            (MV_STAB_TM_BASE / 1000.0) - (MV_STAB_TM_MUL * bestmove_stability_ / 1000.0),
            (MV_STAB_TM_MIN / 1000.0)
        );

    // score stability tm
    if (abs(score - prev_score_) <= SCORE_STAB_MARGIN)
        score_stability_++;
    else
        score_stability_ = 0;
    prev_score_ = score;

    if (depth >= SCORE_STAB_TM_MIN_DEPTH)
        factor *= max(
            (SCORE_STAB_TM_BASE / 1000.0) - (SCORE_STAB_TM_MUL * score_stability_ / 1000.0),
            (SCORE_STAB_TM_MIN / 1000.0)
        );

    // node tm
    if (depth >= NODE_TM_MIN_DEPTH) {
        const auto ratio = f64(get_nodes(thread_id, bestmove)) / f64(get_nodes(thread_id));
        factor *= (NODE_TM_BASE / 1000.0) - (NODE_TM_MUL * ratio / 1000.0);
    }

    return i64(*soft_t_ * factor);
}
