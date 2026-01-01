#include <GameEngine/GameEngine.h>
#include <GameEngine/consts.h>

#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace cge;
using std::async, std::future_status;
using std::cout, std::fixed, std::setprecision;
using std::min;
using std::mutex, std::lock_guard;
using std::ofstream, std::stringstream, std::ios_base;
using std::optional;
using std::ref;
using std::string;
using std::vector;

#define whiteturn (board.sideToMove() == chess::Color::WHITE)



GameEngine::GameEngine(const vector<GamePlayer*>& players_in)
    : tiles(66, sf::RectangleShape({100, 100})), soundbuffers(3), players(players_in) {
    generate_assets();
}


void GameEngine::run_match(const GameOptions& options) {
    // open window
    window.create(sf::VideoMode({880, 940}), "Chess");
    window.setFramerateLimit(FRAMERATE);

    // initialize board
    board = chess::Board(options.start_fen);
    chess::movegen::legalmoves(movelist, board);
    chess::GameResult game_result = chess::GameResult::NONE;
    turn = !whiteturn;

    // reset players
    players[0]->reset();
    players[1]->reset();
    bool p1_is_white = options.p1_is_white;
    names[0].setString(players[!p1_is_white]->name);
    names[1].setString(players[p1_is_white]->name);

    // initialize pgn
    bool savepgn = (options.pgn_file != "");
    ofstream pgn;
    stringstream pgn_moves;
    if (savepgn) {
        string fen = options.start_fen;
        fen = fen.substr(0, min(fen.find('\r'), fen.find('\n')) - 1);
        pgn.open(options.pgn_file, ios_base::app);
        pgn << "[White \"" << players[!p1_is_white]->name << "\"]\n"
            << "[Black \"" << players[p1_is_white]->name << "\"]\n"
            << "[FEN \"" << fen << "\"]\n";
    }
    int nmoves = 1;

    // set time
    bool timeout = false;
    t_remain = options.t_remain;
    int t_inc = options.t_inc;

    interactive = options.interactive;
    if (interactive) sounds[2].play();

    // until game ends or time runs out
    while (game_result == chess::GameResult::NONE && t_remain[0] > 0 && t_remain[1] > 0) {
        auto& cur_player = players[turn == p1_is_white];
        auto& oth_player = players[turn != p1_is_white];
        int64_t& cur_t_remain = t_remain[turn];

        // ask player for move in seperate thread so that we can keep rendering
        bool halt = false;
        auto movereceiver = async(
            &GamePlayer::get_move, cur_player, board, cur_t_remain, t_inc, ref(mouse), ref(halt)
        );
        auto status = future_status::timeout;

        // allow other player to ponder
        auto _ = async(&GamePlayer::ponder, oth_player, board, ref(halt));

        // timings
        std::chrono::_V2::system_clock::time_point start, stop;

        // update visuals until a move is returned
        while (status != future_status::ready) {
            start = std::chrono::high_resolution_clock::now();
            status = movereceiver.wait_for(std::chrono::milliseconds(5));

            // game loop
            update_window();

            // count down timer (only after first white moves)
            if (nmoves != 1) {
                stop = std::chrono::high_resolution_clock::now();
                cur_t_remain
                    -= std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
            }

            // timeout
            if (cur_t_remain <= 0 || !window.isOpen()) {
                halt = true;
                timeout = true;
                timeoutwins[(p1_is_white != turn)]++;
                game_result = chess::GameResult::LOSE;
                goto game_end;
            }
        }

        // play move
        auto toPlay = movereceiver.get();
        if (toPlay == chess::Move::NO_MOVE
            || std::find(movelist.begin(), movelist.end(), toPlay) == movelist.end()) {
            if (toPlay == chess::Move::NO_MOVE) {
                lock_guard<mutex> lock(cout_mutex);
                cout << "Warning, no move returned. Remaining time of player: " << fixed
                     << setprecision(2) << cur_t_remain / 1000.0f << "\n";
            } else {
                lock_guard<mutex> lock(cout_mutex);
                cout << "Warning, illegal move " << chess::uci::moveToUci(toPlay)
                     << " played. Remaining time of player: " << fixed << setprecision(2)
                     << cur_t_remain / 1000.0f << "\n";
            }
            timeout = true;
            timeoutwins[(p1_is_white != turn)]++;
            game_result = chess::GameResult::LOSE;
            goto game_end;
        }
        halt = true;  // force stop pondering
        // pgn saving
        if (savepgn) pgn_moves << nmoves << ". " << chess::uci::moveToSan(board, toPlay) << " ";
        // play move and update everything
        move(toPlay);
        turn = !turn;
        game_result = board.isGameOver().second;
        if (nmoves != 1) cur_t_remain += t_inc;
        nmoves++;
    }

game_end:
    // score tracking
    if (game_result == chess::GameResult::DRAW)
        results[1]++;
    else {
        results[(p1_is_white == turn) ? 0 : 2]++;
        if (!timeout && ((p1_is_white == turn) == p1_is_white)) whitewins[(p1_is_white != turn)]++;
    }

    // wait until window closed if interactive
    if (interactive) {
        sounds[2].play();
        while (window.isOpen()) {
            while (const optional<sf::Event> event = window.pollEvent())
                if (event->is<sf::Event::Closed>()) window.close();
            update_window();
        }
    }
    sq_from = chess::Square::NO_SQ;
    sq_to = chess::Square::NO_SQ;
    movelist.clear();
    if (savepgn) {
        string pgn_result = "1/2-1/2";
        if (game_result != chess::GameResult::DRAW) pgn_result = (!whiteturn) ? "1-0" : "0-1";
        pgn << "[Result \"" << pgn_result << "\"]\n\n" << pgn_moves.str() << "\n\n";
    };
    pgn.close();
}


void GameEngine::print_report() const {
    lock_guard<mutex> lock(cout_mutex);
    int total_matches = results[0] + results[1] + results[2];
    cout << "Out of " << total_matches << " total matches:\n";
    cout << "   " << players[0]->name << "\t\x1b[32m" << results[0] << " (white: " << whitewins[0]
         << ", black: " << results[0] - whitewins[0] - timeoutwins[0]
         << ", timeout: " << timeoutwins[0] << ")\x1b[0m\n";
    cout << "   Draw: \t\x1b[90m" << results[1] << "\x1b[0m\n";
    cout << "   " << players[1]->name << "\t\x1b[31m" << results[2] << " (white: " << whitewins[1]
         << ", black: " << results[2] - whitewins[1] - timeoutwins[1]
         << ", timeout: " << timeoutwins[1] << ")\x1b[0m\n";
}


void GameEngine::generate_assets() {
    // tiles
    for (int i = 0; i < 64; i++) {
        int y = i / 8;
        int x = i % 8;
        tiles[i].setPosition({50.0f + 100 * x, 70.0f + 100 * y});
        tiles[i].setFillColor(((x + y) % 2) ? PALETTE::TILE_B : PALETTE::TILE_W);
    }

    // special tiles
    tiles[64].setFillColor(PALETTE::TILE_MOV);  // move to/from
    tiles[65].setFillColor(PALETTE::TILE_SEL);  // selection squares

    // timer and player names
    bool font_loaded = true;
    font_loaded &= font.openFromFile("src/assets/fonts/Roboto-Regular.ttf");
    if (!font_loaded) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "Warning, could not load font\n";
    }
    timers.reserve(2);
    names.reserve(2);
    for (int i = 0; i < 2; i++) {
        timers.emplace_back(i, font);
        names.emplace_back(font, "", 40);
        names[i].setFillColor(PALETTE::TEXT);
        names[i].setPosition({50.0f, (i) ? 10.0f : 880.0f});
    }

    // sounds
    bool sound_loaded = true;
    sound_loaded &= soundbuffers[0].loadFromFile("src/assets/sounds/Move.ogg");
    sound_loaded &= soundbuffers[1].loadFromFile("src/assets/sounds/Capture.ogg");
    sound_loaded &= soundbuffers[2].loadFromFile("src/assets/sounds/GenericNotify.ogg");
    if (!sound_loaded) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "Warning, could not load 1 or more sound files\n";
    }
    sounds.reserve(3);
    for (int i = 0; i < 3; i++) sounds.emplace_back(soundbuffers[i]);
}


void GameEngine::update_select() {
    // populate selected squares
    if (mouse.event == MouseEvent::LMBDOWN) {
        arrows.clear();
        int x = mouse.x;
        int y = mouse.y;

        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) {
            auto sq = get_square(x, y);
            int piece = (int)board.at(sq);

            // own pieces clicked
            if (piece != 12 && whiteturn == (piece < 6)) {
                selectedtiles.clear();
                selectedtiles.push_back(sq);
                add_selectedtiles();
            } else
                selectedtiles.clear();
        }
    }
    if (mouse.event == MouseEvent::RMBDOWN) selectedtiles.clear();
}


void GameEngine::add_selectedtiles() {
    auto sq_from = selectedtiles[0];

    for (auto& move : movelist) {
        if (move.from() == sq_from) {
            // modify castling move to target empty tile
            if (move.typeOf() == chess::Move::CASTLING) {
                if (move.to() == chess::Square::SQ_H1)  // white king-side
                    selectedtiles.push_back(chess::Square::SQ_G1);
                else if (move.to() == chess::Square::SQ_A1)  // white queen-side
                    selectedtiles.push_back(chess::Square::SQ_C1);
                else if (move.to() == chess::Square::SQ_H8)  // black king-side
                    selectedtiles.push_back(chess::Square::SQ_G8);
                else if (move.to() == chess::Square::SQ_A8)  // black queen-side
                    selectedtiles.push_back(chess::Square::SQ_C8);
            } else
                selectedtiles.push_back(move.to());
        }
    }
}


void GameEngine::update_arrows() {
    // from arrow
    if (mouse.event == MouseEvent::RMBDOWN) {
        int x = mouse.x;
        int y = mouse.y;
        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) arrow_from = get_square(x, y);
    }

    // to arrow
    else if (mouse.event == MouseEvent::RMBUP) {
        int x = mouse.x;
        int y = mouse.y;

        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) {
            auto arrow_to = get_square(x, y);
            if (arrow_to != arrow_from && arrow_from != chess::Square::NO_SQ) {
                Arrow newarrow(arrow_from, arrow_to);

                // check arrow does not exist
                bool arrow_exists = false;
                for (size_t i = 0; i < arrows.size(); i++) {
                    if (newarrow == arrows[i]) {
                        arrows.erase(arrows.begin() + i);
                        arrow_from = chess::Square::NO_SQ;
                        arrow_exists = true;
                        break;
                    }
                }

                // add arrow
                if (!arrow_exists) {
                    arrows.push_back(newarrow);
                    arrow_from = chess::Square::NO_SQ;
                }
            }
        }
    }
}


void GameEngine::update_window() {
    // event handling
    while (const optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>())
            window.close();
        else if (const auto* mouseevent = event->getIf<sf::Event::MouseButtonPressed>()) {
            mouse.event = (mouseevent->button == sf::Mouse::Button::Right) ? MouseEvent::RMBDOWN
                                                                           : MouseEvent::LMBDOWN;
            mouse.x = mouseevent->position.x;
            mouse.y = mouseevent->position.y;
            update_arrows();
            update_select();
        } else if (const auto* mouseevent = event->getIf<sf::Event::MouseButtonReleased>()) {
            mouse.event = (mouseevent->button == sf::Mouse::Button::Right) ? MouseEvent::RMBUP
                                                                           : MouseEvent::LMBUP;
            mouse.x = mouseevent->position.x;
            mouse.y = mouseevent->position.y;
            update_arrows();
            update_select();
        } else
            mouse.event = MouseEvent::NONE;
    }

    // clear window render
    window.clear(PALETTE::BG);

    // draw tiles
    for (int i = 0; i < 64; i++) window.draw(tiles[i]);

    // draw move to/from squares
    if (sq_from != chess::Square::NO_SQ) {
        int file = (int)sq_from.file();
        int rank = (int)sq_from.rank();
        tiles[64].setPosition({50.0f + 100 * file, 770.0f - 100 * rank});
        window.draw(tiles[64]);

        file = (int)sq_to.file();
        rank = (int)sq_to.rank();
        tiles[64].setPosition({50.0f + 100 * file, 770.0f - 100 * rank});
        window.draw(tiles[64]);
    }

    // draw selection squares
    for (auto& sq : selectedtiles) {
        int file = (int)sq.file();
        int rank = (int)sq.rank();
        tiles[65].setPosition({50.0f + 100 * file, 770.0f - 100 * rank});
        window.draw(tiles[65]);
    }

    // draw pieces
    auto pieces = board.occ();
    int check = 0;
    if (board.inCheck()) check = (whiteturn) ? 1 : -1;
    while (pieces) {
        chess::Square sq = pieces.pop();
        int file = (int)sq.file();
        int rank = (int)sq.rank();
        auto piece = board.at(sq);
        piecedrawer.draw(window, piece, 50 + 100 * file, 770 - 100 * rank, check);
    }

    // draw timers and names
    for (int i = 0; i < 2; i++) {
        timers[i].update(t_remain[i] / 1000.0f, i == turn);
        window.draw(timers[i]);
        window.draw(names[i]);
    }

    // draw arrows
    for (auto& arrow : arrows) window.draw(arrow);

    // update window render
    window.display();
}


void GameEngine::move(chess::Move move_in) {
    sq_from = move_in.from();
    sq_to = move_in.to();
    selectedtiles.clear();

    // play sound
    if (interactive) {
        if (board.isCapture(move_in))
            sounds[1].play();  // capture
        else
            sounds[0].play();  // move
    }

    board.makeMove(move_in);
    chess::movegen::legalmoves(movelist, board);
}
