#pragma once
#include <GameEngine/GamePlayer.h>
#include <Raphael/History.h>
#include <Raphael/Transposition.h>
#include <Raphael/nnue.h>
#include <Raphael/tunable.h>

#include <chrono>
#include <string>



namespace raphael {
class Raphael: public cge::GamePlayer {
public:
    static const std::string version;

    struct EngineOptions {
        // uci options
        SpinOption<false> hash;
        SpinOption<false> threads;

        // other options
        CheckOption datagen;
        CheckOption softnodes;
        SpinOption<false> softhardmult;
    };
    static const EngineOptions& default_params();

    struct SearchOptions {
        i64 maxnodes = -1;
        i32 maxdepth = -1;
        i32 movetime = -1;
        i32 movestogo = 0;
        bool infinite = false;
    };

private:
    // search
    EngineOptions params;
    SearchOptions searchopt;  // limit depth, nodes, or movetime
    // storage
    TranspositionTable tt;  // table with position, score, and bestmove
    History history;        // history score for each move
    // nnue
    Nnue net;
    // info
    i64 nodes_;     // number of nodes visited
    i32 seldepth_;  // maximum search depth reached
    // timing
    std::chrono::time_point<std::chrono::high_resolution_clock> start_t_;  // search start time
    i64 search_t_;                                                         // search duration (ms)

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
    MoveStack movestack[MAX_DEPTH];



public:
    /** Initializes Raphael
     *
     * \param name_in player name
     */
    Raphael(const std::string& name_in);


    /** Sets Raphael's engine options
     *
     * \param name name of option to set
     * \param value value to set to
     */
    void set_option(const std::string& name, i32 value);
    void set_option(const std::string& name, bool value);

    /** Sets Raphael's search options
     *
     * \param options options to set to
     */
    void set_searchoptions(SearchOptions options);


    /** Returns the best move found by Raphael. Returns immediately if halt becomes true. Will print
     * out bestmove and search statistics if UCI is true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param mouse unused
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the best move and its score
     */
    MoveScore get_move(
        chess::Board board,
        const i32 t_remain,
        const i32 t_inc,
        volatile cge::MouseInfo& mouse,
        volatile bool& halt
    );

    /** Ponders for best move during opponent's turn. Returns immediately if halt becomes true.
     *
     * \param board current board (for opponent)
     * \param halt bool reference which will turn false to indicate search should stop
     */
    void ponder(chess::Board board, volatile bool& halt);


    /** Resets Raphael */
    void reset();

private:
    /** Estimates the time in ms Raphael should spent on searching a move, and sets search_t_
     * Should be called at the start before using is_time_over
     *
     * \param board current board
     * \param t_remain remaining time in ms
     * \param t_inc increment after move in ms
     */
    void start_search_timer(const chess::Board& board, i32 t_remain, i32 t_inc);

    /** Sets and returns halt = true if search_t_ ms has passed. Will return false indefinetely if
     * search_t_ = 0.
     *
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the new value of halt
     */
    bool is_time_over(volatile bool& halt) const;


    /** Prints out the uci info
     *
     * \param depth current depth
     * \param score score to print
     * \param search stack at current ply
     */
    void print_uci_info(i32 depth, i32 score, const SearchStack* ss) const;

    /** Returns the stringified PV line
     *
     * \param pv the PV to stringify
     * \returns the stringified PV line of the board
     */
    std::string get_pv_line(const PVList& pv) const;


    /** Recursively searches for the best move and score of the current position assuming optimal
     * play by both us and the opponent
     *
     * \tparam is_PV whether the current position is a PV node
     * \param board current board
     * \param depth depth to search for
     * \param ply current distance from root
     * \param mvidx movestack index
     * \param alpha lower bound score of current position
     * \param beta upper bound score of current position
     * \param cutnode whether the current position is a cutnode
     * \param ss search stack at current ply
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns score of current position
     */
    template <bool is_PV>
    i32 negamax(
        chess::Board& board,
        i32 depth,
        const i32 ply,
        const i32 mvidx,
        i32 alpha,
        i32 beta,
        bool cutnode,
        SearchStack* ss,
        volatile bool& halt
    );

    /** Evaluates the board after all noisy moves are played out
     *
     * \tparam is_PV whether the current position is a PV node
     * \param board current board
     * \param ply current distance from root
     * \param mvidx movestack index
     * \param alpha lower bound score of current position
     * \param beta upper bound score of current position
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns score of current board
     */
    template <bool is_PV>
    i32 quiescence(
        chess::Board& board,
        const i32 ply,
        const i32 mvidx,
        i32 alpha,
        i32 beta,
        volatile bool& halt
    );
};
}  // namespace raphael
