#include "GameEngine.h"
#include "HumanPlayer.h"
#include "thc.h"
#include <thread>
#include <future>



// Initialize the GameEngine with the respective
// players and time remaining
cge::GameEngine::GameEngine(std::array<cge::GamePlayer*, 2> players_in): players(players_in) {
    t_remain = {600.0, 600.0};
}
cge::GameEngine::GameEngine(std::array<float, 2> t_remain_in,
    std::array<cge::GamePlayer*, 2> players_in): t_remain(t_remain_in), players(players_in) {}


// Run a match from start to end
void cge::GameEngine::run_match() {
    // until game ends or time runs out
    while (!game_ended && t_remain[0]>0 && t_remain[1]>0) {
        auto& cur_player = players[turn];
        float& cur_t_remain = t_remain[turn];

        // ask player for move in seperate thread so that we can keep rendering
        auto movereceiver = std::async(&cge::GamePlayer::get_move, cur_player, manager, cur_t_remain);

        // timings
        auto start = std::chrono::high_resolution_clock::now();
        auto stop = std::chrono::high_resolution_clock::now();
        std::future_status status = std::future_status::timeout;

        // update visuals until a move is returned
        while(status!=std::future_status::ready) {
            start = std::chrono::high_resolution_clock::now();
            status = movereceiver.wait_for(WAITTIME);

            // game loop
            // update_render();
            printf("%f\n", cur_t_remain);

            if (cur_t_remain <= 0) {
                // handle unreturned move
                // set winner
                return;
            }

            // count down timer
            auto stop = std::chrono::high_resolution_clock::now();
            cur_t_remain -= std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count()/1000.0;
        }

        // play move
        move(movereceiver.get());
        turn = !turn;
    }
}


void cge::GameEngine::move(std::string movestr) {
    mv.TerseIn(&manager, movestr.c_str());
    manager.PlayMove(mv);
}