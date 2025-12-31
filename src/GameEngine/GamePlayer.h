#pragma once
#include <chess.hpp>



namespace cge {  // chess game engine
enum MouseEvent { NONE, LMBDOWN, RMBDOWN, LMBUP, RMBUP };
struct MouseInfo {
    int x;
    int y;
    MouseEvent event;
};


class GamePlayer {
public:
    inline static const std::string version = "0.0.0.0";
    std::string name;


public:
    /** Initializes a GamePlayer
     *
     * \param name_in name of player
     */
    GamePlayer(std::string name_in);

    /** Destructor of GamePlayer */
    virtual ~GamePlayer();

    /** Returns a valid move string. Should return immediately if halt becomes true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param mouse contains mouse movement info for human players
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the best move it found
     */
    virtual chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile MouseInfo& mouse,
        volatile bool& halt
    ) = 0;

    /** Think during opponent's turn. Should return immediately if halt becomes true
     *
     * \param board current board
     * \param halt bool reference which will turn false to indicate search should stop
     */
    virtual void ponder(chess::Board board, volatile bool& halt);

    /** Resets the player */
    virtual void reset();
};
}  // namespace cge
