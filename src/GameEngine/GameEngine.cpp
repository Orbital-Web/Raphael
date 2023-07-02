#include "GameEngine.h"
#include "HumanPlayer.h"
#include "thc.h"
#include <thread>
#include <future>



// Initialize the GameEngine with the respective
// players and time remaining
cge::GameEngine::GameEngine() {
    t_remain = {600, 600};
    players = {&cge::HumanPlayer(), &cge::HumanPlayer()};
}
cge::GameEngine::GameEngine(std::array<int, 2> t_remain_in,
    std::array<cge::GamePlayer*, 2> players_in): t_remain(t_remain_in), players(players_in) {}


// Run a match from start to end
void cge::GameEngine::run_match() {
    // until game ends or time runs out
    while (!game_ended && t_remain[0]>0 && t_remain[1]>0) {
        auto& cur_player = *players[turn];
        int& cur_t_remain = t_remain[turn];
        bool ready = false;
        
        // ask player for move in seperate thread so that we can keep rendering
        auto movereceiver = std::async(&cge::GamePlayer::get_move, cur_player, std::ref(ready), manager, cur_t_remain);

        // update visuals until a move is returned
        while (!ready && cur_t_remain > 0) {
            // count down timer
            update_render();

            if (cur_t_remain <= 0) {
                // handle unreturned move
            }
        }

        // play move
        move(movereceiver.get());
        turn = !turn;
    }
}