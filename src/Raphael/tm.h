#pragma once
#include <chess/include.h>

#include <chrono>
#include <optional>



namespace raphael {
class TimeManager {
public:
    struct SearchOptions {
        std::optional<u64> maxnodes = std::nullopt;
        std::optional<i32> maxdepth = std::nullopt;
        std::optional<i32> movetime = std::nullopt;
        std::optional<i32> movestogo = std::nullopt;
        bool infinite = false;
    };

private:
    std::chrono::time_point<std::chrono::steady_clock> start_t_;  // search start time
    u64 nodes_;

    std::optional<i64> hard_t_;  // hard time limit, checked every few nodes
    std::optional<i64> soft_t_;  // soft time limit, checked after each iterative deepening

    std::optional<u64> hard_nodes_;  // hard node limit, checked every few nodes
    std::optional<u64> soft_nodes_;  // soft node limit, checked after each iterative deepening

    std::optional<i32> max_depth_;  // max depth


public:
    TimeManager();

    /** Sets the limits and starts the search timer
     *
     * \param searchopt options such as movetime and maxnodes
     * \param t_remain remaining time in ms
     * \param t_inc increment in ms
     * \param t_overhead overhead in ms
     * \param softhardmult ratio between hard and soft nodes, or 0 if not using softnodes
     */
    void start_timer(
        const SearchOptions& searchopt, i32 t_remain, i32 t_inc, i32 t_overhead, i32 softhardmult
    );

    /** Returns the time elapsed (in ms) since the start of search */
    i64 get_time() const;


    /** Increments the node counter */
    void inc_nodes();

    /** Returns the number of nodes visited */
    u64 get_nodes() const;


    /** Sets halt and returns its value if the hard limit is reached
     *
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the new value of halt
     */
    bool is_hard_limit_reached(volatile bool& halt) const;

    /** Sets halt and returns its value if the soft limit is reached
     * Checked at the end of a search at `depth`
     *
     * \param halt bool reference which will turn false to indicate search should stop
     * \param depth the current search depth
     * \returns the new value of halt
     */
    bool is_soft_limit_reached(volatile bool& halt, i32 depth) const;

private:
    /** Resets the time manager */
    void reset();


    /** Computes the adjusted soft time limit */
    i64 adjust_soft_time() const;
};
}  // namespace raphael
