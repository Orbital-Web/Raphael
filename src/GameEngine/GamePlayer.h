#pragma once
#include <SFML/Graphics.hpp>
#include <chess.hpp>



namespace cge {  // chess game engine
class GamePlayer {
public:
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
     * \param event used to access events, e.g., mouse moves for human players
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the best move it found
     */
    virtual chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile sf::Event& event,
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
