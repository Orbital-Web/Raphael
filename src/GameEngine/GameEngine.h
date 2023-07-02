#pragma once
#include <string>
#include <utility>

namespace cge { // chess game engine
class GameEngine {
public:
    GameEngine(std::string start_fen, std::pair<int, int> remaining_time);
    void load_game();
    void save_game();
    void start();
    void pause();
    void run_tests(int n_matches);
    void reset();

private:
    void update_render();
    void move(std::string movecode);
    void undo();
    void add_time(std::pair<int, int> extra_time);
};
}   // namespace cge