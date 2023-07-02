#pragma once
#include "GamePlayer.h"
#include <array>



namespace cge { // chess game engine
class GameEngine {
// class variables
private:
    thc::ChessRules manager;
    thc::TERMINAL game_ended = thc::NOT_TERMINAL;
    bool turn = true;                           // white's turn
    std::array<int, 2> t_remain;                // (white, black)
    std::array<cge::GamePlayer*, 2> players;    // (white, black)


// methods
public:
    // Initialize the GameEngine with the respective
    // players and time remaining
    GameEngine();
    GameEngine(std::array<int, 2> t_remain_in, std::array<cge::GamePlayer*, 2> players_in);
    
    // Run a match from start to end
    void run_match();

    //void run_tests(int n_matches);

    void reset();

    //void load_game();

    //void save_game();

private:
    void update_render();

    void move(thc::Move movecode);

    void undo();

    void add_time(std::pair<int, int> extra_time);
};  // GameEngine
}   // namespace cge