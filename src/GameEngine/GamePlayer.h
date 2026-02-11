#pragma once
#include <chess/include.h>



namespace cge {  // chess game engine
enum MouseEvent { NONE, LMBDOWN, RMBDOWN, LMBUP, RMBUP };
struct MouseInfo {
    i32 x;
    i32 y;
    MouseEvent event;
};


class GamePlayer {
public:
    inline static const std::string version = "0.0.0.0";
    std::string name;

    struct MoveScore {
        chess::Move move;
        i32 score;
        bool is_mate;
        i64 nodes = 0;
    };


public:
    /** Initializes a GamePlayer
     *
     * \param name_in name of player
     */
    GamePlayer(const std::string& name_in);

    /** Destructor of GamePlayer */
    virtual ~GamePlayer();


    /** Sets the position to search on
     *
     * \param board current board
     */
    virtual void set_board(const chess::Board& board) = 0;

    /** Returns a move and its score from the set position
     * Should return immediately if halt becomes true
     *
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param mouse contains mouse movement info for human players
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the found MoveScore
     */
    virtual MoveScore get_move(
        const i32 t_remain, const i32 t_inc, volatile MouseInfo& mouse, volatile bool& halt
    ) = 0;

    /** Think during opponent's turn. Should return immediately if halt becomes true
     *
     * \param halt bool reference which will turn false to indicate search should stop
     */
    virtual void ponder(volatile bool& halt);


    /** Resets the player */
    virtual void reset();
};
}  // namespace cge
