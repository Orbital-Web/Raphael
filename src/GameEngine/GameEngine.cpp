#include "GameEngine.h"
#include "HumanPlayer.h"
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <thread>
#include <future>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>



// Initialize the GameEngine with the respective players
cge::GameEngine::GameEngine(std::array<cge::GamePlayer*, 2> players_in): players(players_in) {
    generate_assets();
}


// Runs a match from start to end
void cge::GameEngine::run_match(bool p1_is_white, std::string start_fen, std::array<float, 2> t_remain_in, bool is_interactive) {
    // open window
    window = new sf::RenderWindow(sf::VideoMode(880, 940), "Chess");
    
    // initialize board
    board = chess::Board(start_fen);
    chess::movegen::legalmoves(movelist, board);
    chess::GameResult game_result = chess::GameResult::NONE;

    // set time
    t_remain = t_remain_in;

    // manage turns
    players[0]->iswhite = p1_is_white;
    players[1]->iswhite = !p1_is_white;
    names[0].setString(players[!p1_is_white]->name);
    names[1].setString(players[p1_is_white]->name);
    turn = (board.sideToMove()!=chess::Color::WHITE);

    interactive = is_interactive;
    if (interactive)
        sounds[2].play();
    
    // until game ends or time runs out
    while (game_result==chess::GameResult::NONE && t_remain[0]>0 && t_remain[1]>0) {
        auto& cur_player = players[turn^(!p1_is_white)];
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
                game_result = chess::GameResult::WIN;
                goto game_end;
            }
        }

        // play move
        move(movereceiver.get());
        turn = !turn;
        game_result = board.isGameOver().second;
    }

    game_end:
    // score tracking
    if (game_result==chess::GameResult::DRAW)
        results[1]++;
    else
        results[((game_result==chess::GameResult::WIN)^(!p1_is_white)) ? 2 : 0]++;
    
    // wait until window closed if interactive
    if (interactive) {
        sounds[2].play();
        while (window->isOpen()) {
            while (window->pollEvent(windowevent))
                if (windowevent.type == sf::Event::Closed)
                    window->close();
            update_window();
        }
    }

    // close window
    delete window;
}


// Print result of the games
void cge::GameEngine::print_report() {
    int total_matches = results[0] + results[1] + results[2];
    printf("Out of %i total matches:\n", total_matches);
    printf("   %s: \t\x1b[32m%i\x1b[0m\n", players[0]->name.c_str(), results[0]);   // p1 wins
    printf("   Draw: \t\x1b[90m%i\x1b[0m\n", results[1]);                           // draws
    printf("   %s: \t\x1b[31m%i\x1b[0m\n", players[1]->name.c_str(), results[2]);   // p2 wins
}


// Generates sprites, texts, and sounds
void cge::GameEngine::generate_assets() {
    // tiles
    for (int i=0; i<64; i++) {
        int x = i%8;
        int y = i/8;
        tiles[i] = sf::RectangleShape({100, 100});
        tiles[i].setPosition(50 + 100*x, 70 + 100*y);
        tiles[i].setFillColor(((x+y)%2) ? cge::PALETTE::TILE_B : cge::PALETTE::TILE_W);
    }

    // special tiles (e.g. move to&from, selection)
    tilesspecial[0] = sf::RectangleShape({100, 100});
    tilesspecial[0].setFillColor(cge::PALETTE::TILE_MOV);
    tilesspecial[1] = sf::RectangleShape({100, 100});
    tilesspecial[1].setFillColor(cge::PALETTE::TILE_SEL);

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
        names[i] = sf::Text("", font, 40);
        names[i].setFillColor(cge::PALETTE::TEXT);
        names[i].setPosition(50, (i) ? 10 : 880);
    }

    // piece textures and sprites
    for (int i=0; i<12; i++) {
        piecetextures[i].loadFromFile("src/assets/themes/tartiana/" + cge::TEXTURE[i] + ".png");
        piecetextures[i].setSmooth(true);
        pieces[i].setTexture(piecetextures[i]);
    }

    // sounds
    soundbuffers[0].loadFromFile("src/assets/sounds/Move.ogg");
    soundbuffers[1].loadFromFile("src/assets/sounds/Capture.ogg");
    soundbuffers[2].loadFromFile("src/assets/sounds/GenericNotify.ogg");
    for (int i=0; i<3; i++)
        sounds[i].setBuffer(soundbuffers[i]);
}


// Renders the chess pieces
void cge::GameEngine::draw_pieces() {
    for (int rank=0; rank<8; rank++) {
        for (int file=0; file<8; file++) {
            chess::Square sq = chess::utils::fileRankSquare(chess::File(file), chess::Rank(rank));
            int piece = (int)board.at(sq);
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


// Draws possible move square and move to/from squares
void cge::GameEngine::draw_select() {
    // draw move to/from squares
    if (sq_from!=chess::Square::NO_SQ) {
        int file = (int)chess::utils::squareFile(sq_from);
        int rank = (int)chess::utils::squareRank(sq_from);
        tilesspecial[0].setPosition(50 + 100*file, 770 - 100*rank);
        window->draw(tilesspecial[0]);

        file = (int)chess::utils::squareFile(sq_to);
        rank = (int)chess::utils::squareRank(sq_to);
        tilesspecial[0].setPosition(50 + 100*file, 770 - 100*rank);
        window->draw(tilesspecial[0]);
    }

    // populate selected squares
    if (windowevent.type==sf::Event::MouseButtonPressed && windowevent.mouseButton.button==sf::Mouse::Left) {
        int x = windowevent.mouseButton.x;
        int y = windowevent.mouseButton.y;

        // board clicked
        if (x>50 && x<850 && y>70 && y<870) {
            chess::Square sq = get_square(x, y);
            int piece = (int)board.at(sq);

            // own pieces clicked
            if ((!turn && piece>=0 && piece<6) || (turn && piece>=6 && piece<12)) {
                selectedtiles.clear();
                selectedtiles.push_back(sq);
                add_selectedtiles();
            } else
                selectedtiles.clear();
        }

        // only consider first mouse click
        windowevent.type = sf::Event::MouseMoved;
    }

    //draw selection squares
    for (auto sq : selectedtiles) {
        int file = (int)chess::utils::squareFile(sq);
        int rank = (int)chess::utils::squareRank(sq);
        tilesspecial[1].setPosition(50 + 100*file, 770 - 100*rank);
        window->draw(tilesspecial[1]);
    }
}


// Converts x and y coordinates into a Square
chess::Square cge::GameEngine::get_square(int x, int y) {
    int rank = (870 - y) / 100;
    int file = (x - 50) / 100;
    return chess::utils::fileRankSquare(chess::File(file), chess::Rank(rank));
}


// Adds squares a selectedtiles[0] can move to
void cge::GameEngine::add_selectedtiles() {
    chess::Square sq_from = selectedtiles[0];

    for (auto move : movelist) {
        if (move.from()==sq_from) {
            // modify castling move to target empty tile
            if (move.typeOf()==chess::Move::CASTLING) {
                switch (move.to()) {
                case chess::Square::SQ_H1:  // white king-side
                    selectedtiles.push_back(chess::Square::SQ_G1);
                    break;
                case chess::Square::SQ_A1:  // white queen-side
                    selectedtiles.push_back(chess::Square::SQ_C1);
                    break;
                case chess::Square::SQ_H8:  // black king-side
                    selectedtiles.push_back(chess::Square::SQ_G8);
                    break;
                case chess::Square::SQ_A8:  // black queen-side
                    selectedtiles.push_back(chess::Square::SQ_C8);
                    break;
                }
            } else
                selectedtiles.push_back(move.to());
        }
    }
}


// Handles window events and rendering
void cge::GameEngine::update_window() {
    // event handling
    while (window->pollEvent(windowevent))
        if (windowevent.type == sf::Event::Closed)
            window->close();

    // clear window render
    window->clear(cge::PALETTE::BG);

    // draw tiles
    for (int i=0; i<64; i++)
        window->draw(tiles[i]);

    draw_select();  // draw selected and move tiles
    draw_timer();   // draw timer and names
    draw_pieces();  // draw pieces

    // update window render
    window->display();
}


// Updates the board with a move
void cge::GameEngine::move(chess::Move move_in) {
    sq_from = move_in.from();
    sq_to = move_in.to();
    selectedtiles.clear();

    // play sound
    if (interactive) {
        if (board.at(sq_to)!=chess::Piece::NONE && move_in.typeOf()!=chess::Move::CASTLING)
            sounds[1].play();   // capture
        else
            sounds[0].play();   // move
    }
    
    board.makeMove(move_in);
    chess::movegen::legalmoves(movelist, board);
}