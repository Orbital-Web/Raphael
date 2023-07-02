#include "GameEngine.h"
#include "HumanPlayer.h"
#include "thc.h"
#include <thread>
#include <future>



// Initialize the GameEngine with the respective
// players and time remaining
cge::GameEngine::GameEngine(std::array<cge::GamePlayer*, 2> players_in): players(players_in) {
    t_remain = {600, 600};
}
cge::GameEngine::GameEngine(std::array<int, 2> t_remain_in,
    std::array<cge::GamePlayer*, 2> players_in): t_remain(t_remain_in), players(players_in) {}


// Run a match from start to end
void cge::GameEngine::run_match() {
    // until game ends or time runs out
    while (!game_ended && t_remain[0]>0 && t_remain[1]>0) {
        auto cur_player = players[turn];
        int& cur_t_remain = t_remain[turn];


        /*
        std::string fen = manager.ForsythPublish();
        std::string s = manager.ToDebugStr();
        printf( "FEN (Forsyth Edwards Notation) = %s\n", fen.c_str() );
        printf( "Position = %s\n", s.c_str() );
        */
        
        // ask player for move in seperate thread so that we can keep rendering
        auto movereceiver = std::async(&cge::GamePlayer::get_move, cur_player, manager, cur_t_remain);

        // update visuals until a move is returned
        while(cur_t_remain>0 && 
            movereceiver.wait_for(std::chrono::milliseconds(WAITTIME))!=std::future_status::ready) {
            // count down timer
            // update_render();
            //printf("A");

            if (cur_t_remain <= 0) {
                // handle unreturned move
                // set winner
                return;
            }
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