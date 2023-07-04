#include "GameEngine.h"
#include "HumanPlayer.h"
#include "thc.h"
#include <SFML/Graphics.hpp>
#include <thread>
#include <future>
#include <sstream>
#include <iomanip>



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
        tiles[i].setFillColor(((x+y)%2) ? cge::PALETTE::TILE_B : cge::PALETTE::TILE_W);
    }

    // timer and player names
    font.loadFromFile("src/assets/fonts/Roboto-Regular.ttf");
    for (int i=0; i<2; i++) {
        // timer boxes
        timers[i] = sf::RectangleShape({180, 50});
        timers[i].setPosition(660, (i) ? 10 : 880);
        // time remaining
        timertexts[i] = sf::Text("", font, 40);
        timertexts[i].setFillColor(cge::PALETTE::TEXT);
        // names
        names[i] = sf::Text(players[i]->name, font, 40);
        names[i].setFillColor(cge::PALETTE::TEXT);
        names[i].setPosition(50, (i) ? 10 : 880);
    }

    // piece textures and sprites
    for (int i=0; i<12; i++) {
        piecetextures[i].loadFromFile("src/assets/themes/tartiana/" + cge::TEXTURE[i] + ".png");
        piecetextures[i].setSmooth(true);
        pieces[i].setTexture(piecetextures[i]);
    }
}


// renders the chess pieces
void cge::GameEngine::draw_pieces() {
    char* board = manager.squares;

    for (int sq=0; sq<64; sq++) {
        // coordinates on the board
        int x = sq%8;
        int y = sq/8;

        // render piece
        if (board[sq] != ' ') {
            int piece = cge::PIECENAME.at(board[sq]);
            pieces[piece].setPosition(50 + 100*x, 70 + 100*y);
            window->draw(pieces[piece]);
        }
    }
}


// renders the timer and names
void cge::GameEngine::draw_timer() {
    // set timer display properties
    for (int i=0; i<2; i++) {
        // accomodate different format
        std::stringstream formatter;
        std::string t_disp;

        if (t_remain[i] >= 60) {    // mm : ss
            timers[i].setFillColor((i!=turn) ? cge::PALETTE::TIMER : cge::PALETTE::TIMER_A);
            int min = (int)t_remain[i] / 60;
            int sec = (int)t_remain[i] % 60;
            formatter << std::setw(2) << std::setfill('0') << std::to_string(sec);
            t_disp = std::to_string(min) + " : " + formatter.str();
        } else {                    // ss.mm
            timers[i].setFillColor((i!=turn) ? cge::PALETTE::TIMER_LOW : cge::PALETTE::TIMER_ALOW);
            formatter << std::fixed << std::setprecision(1) << t_remain[i];
            t_disp = formatter.str();
        }

        // manage positioning of string
        timertexts[i].setString(t_disp);
        sf::FloatRect textbounds = timertexts[i].getLocalBounds();
        timertexts[i].setPosition(820 - textbounds.width, (i) ? 10 : 880);
    }

    // draw timer, remaining time, and names
    for (int i=0; i<2; i++) {
        window->draw(timers[i]);
        window->draw(timertexts[i]);
        window->draw(names[i]);
    }
}


// handles window events and rendering
void cge::GameEngine::update_window() {
    // event handling
    while (window->pollEvent(windowevent))
    {
        if (windowevent.type == sf::Event::Closed)
            window->close();
    }

    // clear window render
    window->clear(cge::PALETTE::BG);

    // draw tiles
    for (int i=0; i<64; i++)
        window->draw(tiles[i]);
    
    // selected tiles overlay
    
    draw_timer();   // draw timer and names
    draw_pieces();  // draw pieces

    // update window render
    window->display();
}


// updates the manager with a move
void cge::GameEngine::move(std::string movestr) {
    mv.TerseIn(&manager, movestr.c_str());
    manager.PlayMove(mv);
}