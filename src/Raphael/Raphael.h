#pragma once
#include <Raphael/History.h>
#include <Raphael/Transposition.h>
#include <Raphael/position.h>
#include <Raphael/tm.h>

#include <atomic>
#include <barrier>
#include <memory>
#include <string>
#include <thread>
#include <vector>



namespace raphael {
class Raphael {
public:
    static const std::string version;

    struct EngineOptions {
        // uci options
        SpinOption<false> hash;
        SpinOption<false> threads;
        SpinOption<false> moveoverhead;
        CheckOption chess960;

        // other options
        CheckOption datagen;
        CheckOption softnodes;
        SpinOption<false> softhardmult;
    };
    static const EngineOptions& default_params();

    enum class UciInfoLevel : u8 {
        NONE = 0,
        MINIMAL = 1,
        ALL = 2,
    };

    struct MoveScore {
        chess::Move move;
        i32 score;
        bool is_mate;
        u64 nodes = 0;
    };


private:
    struct PVList {
        chess::Move moves[MAX_DEPTH] = {chess::Move::NO_MOVE};
        i32 length = 0;

        /** Updates the PV
         *
         * \param move move to add
         * \param child child PV to append
         */
        void update(const chess::Move move, const PVList& child);
    };

    struct SearchStack {
        PVList pv;
        i32 static_eval = 0;
        chess::Move move = chess::Move::NO_MOVE;
        chess::Move killer = chess::Move::NO_MOVE;
        chess::Move excluded = chess::Move::NO_MOVE;
    };

    struct MoveStack {
        chess::MoveList<chess::ScoredMove> movelist;
        chess::MoveList<chess::Move> quietlist;
        chess::MoveList<chess::Move> noisylist;
    };

    struct alignas(CACHE_SIZE) ThreadData {
        SearchStack search_stack[MAX_DEPTH + 3];
        MoveStack move_stack[MAX_DEPTH * 2];

        Position<true> position_;
        History history;
        i32 thread_id;
    };

    // shared data
    EngineOptions params_;
    UciInfoLevel ucilevel_ = UciInfoLevel::NONE;

    TranspositionTable tt_;
    TimeManager tm_;

    // thread helpers
    std::atomic<bool> is_searching{false};
    TimeManager::SearchOptions search_opt;
    MoveScore search_result;

    std::atomic<bool> stop{false};
    std::atomic<bool> quit{false};

    std::unique_ptr<std::barrier<>> idle_barrier;
    std::unique_ptr<std::barrier<>> search_end_barrier;

    std::vector<std::thread> searchers;
    std::vector<ThreadData> thread_data;



public:
    /** Initializes Raphael */
    Raphael();

    /** Quits any ongoing search and cleans up */
    ~Raphael();


    /** Sets Raphael's engine options
     *
     * \param name name of option to set
     * \param value value to set to
     */
    void set_option(const std::string& name, i32 value);
    void set_option(const std::string& name, bool value);

    /** Sets Raphael's UCI info level
     *
     * \param level level to set to
     */
    void set_uciinfolevel(UciInfoLevel level);


    /** Sets the position to search on
     *
     * \param position position to set to
     */
    void set_position(const Position<false>& position);

    /** Sets the board to search on
     *
     * \param board board to set to
     */
    void set_board(const chess::Board& board);


    /** Kills existing threads (if any) and spawns num_searcher threads
     *
     * \param num_searchers number of threads to spawn
     */
    void set_threads(i32 num_searchers);


    /** Starts a search without blocking, does nothing if there is an ongoing search
     *
     * \param options search options
     */
    void start_search(const TimeManager::SearchOptions& options);

    /** Returns whether the search is complete (or isn't running)
     *
     * \returns whether the search completed
     */
    bool is_search_complete();

    /** Blocks until the current search finishes and returns its search result.
     * The search result is only valid if start_search has been called previously.
     * You can call this to retrieve the search result after is_search_complete returns true.
     *
     * \returns the completed search result
     */
    MoveScore wait_search();

    /** Starts search and blocks until it completes
     *
     * \param options search options
     * \returns the completed search result
     */
    MoveScore search(const TimeManager::SearchOptions& options);

    /** Stops any ongoing search and waits */
    void stop_search();


    /** Returns the static eval of the set position
     *
     * \param corrected whether to return the corrected or raw static eval
     * \returns the static eval
     */
    i32 static_eval(bool corrected);


    /** Resets Raphael */
    void reset();

private:
    /** Stops and kills all threads */
    void kill_search();


    /** Persistent search thread to handle search commands
     *
     * \param thread_id this thread's id (0 == main thread)
     */
    void t_search_function(i32 thread_id);


    /** Prints out the uci info
     *
     * \param depth current depth
     * \param score score to print
     * \param board current board
     * \param ss search stack at root
     */
    void print_uci_info(
        i32 depth, i32 score, const chess::Board& board, const SearchStack* ss
    ) const;

    /** Returns the stringified PV line
     *
     * \param pv the PV to stringify
     * \returns the stringified PV line of the board
     */
    std::string get_pv_line(const PVList& pv) const;


    /** Adjusts the raw static eval using scaling and corrhists
     *
     * \param tdata this thread's data
     * \param raw_static_eval raw eval to adjust
     * \returns the adjusted eval
     */
    i32 adjust_score(const ThreadData& tdata, i32 raw_static_eval) const;


    /** Does the actual search logic, calling negamax with increasing depth.
     * May set stop to true
     *
     * \param tdata this thread's data
     * \returns the bestmove and score of this thread
     */
    MoveScore iterative_deepen(ThreadData& tdata);

    /** Recursively searches for the best move and score of the current position assuming optimal
     * play by both us and the opponent
     *
     * \tparam is_PV whether the current position is a PV node
     * \param tdata this thread's data
     * \param depth depth to search for
     * \param ply current distance from root
     * \param alpha lower bound score of current position
     * \param beta upper bound score of current position
     * \param cutnode whether the current position is a cutnode
     * \param ss search stack at current ply
     * \param mv move stack at current node
     * \returns score of current position
     */
    template <bool is_PV>
    i32 negamax(
        ThreadData& tdata,
        i32 depth,
        const i32 ply,
        i32 alpha,
        i32 beta,
        bool cutnode,
        SearchStack* ss,
        MoveStack* mv
    );

    /** Evaluates the board after all noisy moves are played out
     *
     * \tparam is_PV whether the current position is a PV node
     * \param tdata this thread's data
     * \param ply current distance from root
     * \param alpha lower bound score of current position
     * \param beta upper bound score of current position
     * \param mv move stack at current node
     * \returns score of current board
     */
    template <bool is_PV>
    i32 quiescence(ThreadData& tdata, const i32 ply, i32 alpha, i32 beta, MoveStack* mv);
};
}  // namespace raphael
