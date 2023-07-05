#include "GameEngine.h"
#include "HumanPlayer.h"
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <thread>
#include <future>
#include <sstream>
#include <iomanip>
#include <algorithm>



// Initialize the GameEngine with the respective
// players and time remaining
cge::GameEngine::GameEngine(std::array<cge::GamePlayer*, 2> players_in): players(players_in) {
    t_remain = {600.0, 600.0};
}
cge::GameEngine::GameEngine(std::array<float, 2> t_remain_in,
    std::array<cge::GamePlayer*, 2> players_in): t_remain(t_remain_in), players(players_in) {}


// Run a match from start to end
void cge::GameEngine::run_match(bool p1_is_white, std::string start_fen) {
    // set white and black
    if (!p1_is_white)
        std::reverse(players.begin(), players.end());
    players[0]->iswhite = true;
    players[1]->iswhite = false;

    // initialize board
    board = chess::Board(start_fen);
    chess::GameResult game_result = chess::GameResult::NONE;
    
    // create window and sprites
    window = new sf::RenderWindow(sf::VideoMode(880, 940), "Chess");
    generate_sprites();
    
    // until game ends or time runs out
    while (game_result==chess::GameResult::NONE && t_remain[0]>0 && t_remain[1]>0) {
        auto& cur_player = players[turn];
        float& cur_t_remain = t_remain[turn];

        // ask player for move in seperate thread so that we can keep rendering
        bool halt = false;
        auto movereceiver = std::async(&cge::GamePlayer::get_move, cur_player,
                                       board, cur_t_remain, std::ref(windowevent), std::ref(halt));
        auto status = std::future_status::timeout;

        // timings
        std::chrono::_V2::system_clock::time_point start, stop;

        // update visuals until a move is returned
        while(status!=std::future_status::ready) {
            start = std::chrono::high_resolution_clock::now();
            status = movereceiver.wait_for(WAITTIME);

            // game loop
            update_window();

            // count down timer
            stop = std::chrono::high_resolution_clock::now();
            cur_t_remain -= std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count()/1000.0;

            // timeout
            if (cur_t_remain <= 0) {
                halt = true;
                results[(turn) ? 0 : 2]++;
                delete window;
                return;
            }
        }

        // play move
        move(movereceiver.get());
        turn = !turn;
        game_result = board.isGameOver().second;
    }

    // score tracking
    if (game_result==chess::GameResult::DRAW)
        results[1]++;
    else
        results[(game_result==chess::GameResult::WIN) ? 2 : 0]++;

    delete window;
}


// Print result of the games
void cge::GameEngine::print_report() {
    int total_matches = results[0] + results[1] + results[2];
    printf("Out of %i total matches:\n", total_matches);
    printf("   %s: \t\x1b[32m%i\x1b[0m\n", players[0]->name.c_str(), results[0]);
    printf("   Draw: \t\x1b[90m%i\x1b[0m\n", results[1]);
    printf("   %s: \t\x1b[31m%i\x1b[0m\n", players[1]->name.c_str(), results[2]);
}


// Generates (mostly) static sprites such as the tiles and timers
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


// Renders the chess pieces
void cge::GameEngine::draw_pieces() {
    for (int rank=0; rank<8; rank++) {
        for (int file=0; file<8; file++) {
            chess::Square sq = chess::utils::fileRankSquare(chess::File(file), chess::Rank(rank));
            int piece = int(board.at(sq));
            if (piece != 12) {
                pieces[(int)piece].setPosition(50 + 100*file, 770 - 100*rank);
                window->draw(pieces[piece]);
            }
        }
    }
}


// Renders the timer and names
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


// Handles window events and rendering
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


// Updates the board with a move
void cge::GameEngine::move(chess::Move move_in) {
    board.makeMove(move_in);
}