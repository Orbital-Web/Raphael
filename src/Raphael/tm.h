#pragma once
#include <Raphael/consts.h>
#include <chess/include.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <vector>



namespace raphael {
class TimeManager {
public:
    struct SearchOptions {
        i32 t_remain = 0;
        i32 t_inc = 0;
        std::optional<u64> maxnodes = std::nullopt;
        std::optional<i32> maxdepth = std::nullopt;
        std::optional<i32> movetime = std::nullopt;
        std::optional<i32> movestogo = std::nullopt;
        bool infinite = false;
    };

private:
    std::chrono::time_point<std::chrono::steady_clock> start_t_;  // search start time

    struct alignas(CACHE_SIZE) ThreadTM {
        std::atomic<u64> nodes{0};
        std::atomic<i32> seldepth{0};
    };
    std::vector<ThreadTM> thread_tm_;

    // limits
    std::optional<i64> hard_t_;  // hard time limit, checked every few nodes
    std::optional<i64> soft_t_;  // soft time limit, checked after each iterative deepening

    std::optional<u64> hard_nodes_;  // hard node limit, checked every few nodes
    std::optional<u64> soft_nodes_;  // soft node limit, checked after each iterative deepening

    std::optional<i32> max_depth_;  // max depth

    // heuristics (should only be accessed by main thread)
    u64 nodes_per_move_[64][64] = {};

    chess::Move prev_bestmove_ = chess::Move::NO_MOVE;
    i32 bestmove_stability_ = 0;

    i32 prev_score_ = 0;
    i32 score_stability_ = 0;


public:
    TimeManager();


    /** Sets the number of threads searching
     *
     * \param num_searchers number of threads searching
     */
    void set_threads(i32 num_searchers);


    /** Sets the limits and starts the search timer
     *
     * \param searchopt options such as movetime and maxnodes
     * \param t_overhead overhead in ms
     * \param softhardmult ratio between hard and soft nodes, or 0 if not using softnodes
     */
    void start_timer(const SearchOptions& searchopt, i32 t_overhead, i32 softhardmult);

    /** Returns the time elapsed (in ms) since the start of search */
    i64 get_time() const;


    /** Increments the node counter
     *
     * \param thread_id thread id
     */
    void inc_nodes(i32 thread_id);

    /** Increments the node counter for a certain move
     *
     * \param thread_id thread id
     * \param move move to increment for
     * \param count amount to increment by
     */
    void inc_nodes(i32 thread_id, chess::Move move, u64 count);

    /** Returns the total number of nodes visited
     *
     * \returns number of nodes visited
     */
    u64 get_nodes() const;

    /** Returns the total number of nodes visited by a thread
     *
     * \param thread_id thread id
     * \returns node count of that thread
     */
    u64 get_nodes(i32 thread_id) const;

    /** Returns the number of nodes visited by a thread for a certain move
     *
     * \param thread_id thread id
     * \param move move to get node count for
     * \returns number of nodes visited for that move
     */
    u64 get_nodes(i32 thread_id, chess::Move move) const;


    /** Updates the seldepth of a thread
     *
     * \param thread_id thread id
     * \param depth new depth
     */
    void update_seldepth(i32 thread_id, i32 depth);

    /** Returns the global seldepth
     *
     * \returns max seldepth of any thread
     */
    i32 get_seldepth() const;


    /** Sets stop and returns its value if the hard limit is reached for this thread
     *
     * \param thread_id thread id
     * \param stop bool reference which will turn false to indicate search should stop
     * \returns the new value of stop
     */
    bool is_hard_limit_reached(i32 thread_id, std::atomic<bool>& stop) const;

    /** Sets stop and returns its value if the soft limit for this thread is reached
     * Checked at the end of a search at `depth`
     *
     * \param thread_id thread id
     * \param stop bool reference which will turn false to indicate search should stop
     * \param bestmove current bestmove
     * \param score current score
     * \param depth current search depth
     * \returns the new value of stop
     */
    bool is_soft_limit_reached(
        i32 thread_id, std::atomic<bool>& stop, chess::Move bestmove, i32 score, i32 depth
    );

private:
    /** Resets the time manager */
    void reset();


    /** Computes the adjusted soft time limit for this thread
     *
     * \param thread_id thread id
     * \param bestmove current bestmove
     * \param score current score
     * \param depth current search depth
     * \returns the new soft time limit
     */
    i64 adjust_soft_time(i32 thread_id, chess::Move bestmove, i32 score, i32 depth);
};
}  // namespace raphael
