#include "GameEngine.h"
#include "HumanPlayer.h"
#include "thc.h"
#include <SFML/Graphics.hpp>
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
    // create window
    window = new sf::RenderWindow(sf::VideoMode(880, 940), "Chess");
    generate_sprites();
    
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
            update_window();
            //printf("%f\n", cur_t_remain);

            if (cur_t_remain <= 0) {
                // handle unreturned move
                // set winner
                // the thread returning the move will still be running
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


// generates (mostly) static sprites such as the tiles and timers
void cge::GameEngine::generate_sprites() {
    // tiles
    for (int i=0; i<64; i++) {
        int x = i%8;
        int y = i/8;
        tiles[i] = sf::RectangleShape({100, 100});
        tiles[i].setPosition(50 + 100*x, 70 + 100*y);
        tiles[i].setFillColor(((x+y)%2) ? cge::Palette::TILE_B : cge::Palette::TILE_W);
    }

    // timer boxes
    timers[0] = sf::RectangleShape({180, 50});
    timers[0].setPosition(660, 880);
    timers[0].setFillColor(cge::Palette::TIMER_W);
    timers[1] = sf::RectangleShape({180, 50});
    timers[1].setPosition(660, 10);
    timers[1].setFillColor(cge::Palette::TIMER_B);

    // player names
    font.loadFromFile("src/assets/fonts/Roboto-Regular.ttf");
    for (int i=0; i<2; i++) {
        printf("AA%s\n", players[i]->name.c_str());
        names[i] = sf::Text(players[i]->name, font, 40);
        names[i].setFillColor(cge::Palette::TEXT);
    }
    names[0].setPosition(50, 880);
    names[1].setPosition(50, 10);
}


// handles window events and rendering
void cge::GameEngine::update_window() {
    while (window->pollEvent(windowevent))
    {
        if (windowevent.type == sf::Event::Closed)
            window->close();
    }

    // clear window
    window->clear(cge::Palette::BG);

    // draw tiles
    for (int i=0; i<64; i++)
        window->draw(tiles[i]);
    
    // draw timer boxes and names
    for (int i=0; i<2; i++) {
        window->draw(timers[i]);
        window->draw(names[i]);
    }
    
    // display window
    window->display();
}


// updates the manager with a move
void cge::GameEngine::move(std::string movestr) {
    mv.TerseIn(&manager, movestr.c_str());
    manager.PlayMove(mv);

    printf("%s\n", manager.ToDebugStr());
}