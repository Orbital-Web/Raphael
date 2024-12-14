#include <GameEngine/GameEngine.h>
#include <GameEngine/consts.h>

#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>

using namespace cge;
using std::async;
using std::cout;
using std::fixed;
using std::future_status;
using std::ofstream;
using std::ref;
using std::setprecision;



GameEngine::GameEngine(const vector<GamePlayer*>& players_in)
    : tiles(66, sf::RectangleShape({100, 100})), soundbuffers(3), sounds(3), players(players_in) {
    generate_assets();
}


void GameEngine::run_match(const GameOptions& options) {
    // open window
    window.create(sf::VideoMode(880, 940), "Chess");
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
    if (savepgn) {
        pgn.open(options.pgn_file, std::ios_base::app);
        pgn << "[White \"" << players[!p1_is_white]->name << "\"]\n"
            << "[Black \"" << players[p1_is_white]->name << "\"]\n"
            << "[FEN \"" << options.start_fen << "\"]\n\n";
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
            &GamePlayer::get_move, cur_player, board, cur_t_remain, t_inc, ref(event), ref(halt)
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
            if (cur_t_remain <= 0 || event.type == sf::Event::Closed) {
                halt = true;
                timeout = true;
                timeoutwins[(p1_is_white != turn)]++;
                game_result = chess::GameResult::LOSE;
                goto game_end;
            }
        }

        // play move
        auto toPlay = movereceiver.get();
        if (toPlay == chess::Move::NO_MOVE) {
            {
                lock_guard<mutex> lock(cout_mutex);
                cout << "Warning, no move returned. Remaining time of player: " << fixed
                     << setprecision(2) << cur_t_remain / 1000.0f << "\n";
            }
            timeout = true;
            timeoutwins[(p1_is_white != turn)]++;
            game_result = chess::GameResult::LOSE;
            goto game_end;
        }
        halt = true;  // force stop pondering
        // pgn saving
        if (savepgn) pgn << nmoves << ". " << chess::uci::moveToSan(board, toPlay) << " ";
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
            while (window.pollEvent(event))
                if (event.type == sf::Event::Closed) window.close();
            update_window();
        }
    }
    sq_from = chess::NO_SQ;
    sq_to = chess::NO_SQ;
    movelist.clear();
    event.type = sf::Event::MouseMoved;
    if (savepgn) pgn << "\n\n";
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
        tiles[i].setPosition(50 + 100 * x, 70 + 100 * y);
        tiles[i].setFillColor(((x + y) % 2) ? PALETTE::TILE_B : PALETTE::TILE_W);
    }

    // special tiles
    tiles[64].setFillColor(PALETTE::TILE_MOV);  // move to/from
    tiles[65].setFillColor(PALETTE::TILE_SEL);  // selection squares

    // timer and player names
    font.loadFromFile("src/assets/fonts/Roboto-Regular.ttf");
    for (int i = 0; i < 2; i++) {
        timers.emplace_back(i, font);
        names.emplace_back("", font, 40);
        names[i].setFillColor(PALETTE::TEXT);
        names[i].setPosition(50, (i) ? 10 : 880);
    }

    // sounds
    soundbuffers[0].loadFromFile("src/assets/sounds/Move.ogg");
    soundbuffers[1].loadFromFile("src/assets/sounds/Capture.ogg");
    soundbuffers[2].loadFromFile("src/assets/sounds/GenericNotify.ogg");
    for (int i = 0; i < 3; i++) sounds[i].setBuffer(soundbuffers[i]);
}


void GameEngine::update_select() {
    // populate selected squares
    if (lmbdown) {
        arrows.clear();
        int x = event.mouseButton.x;
        int y = event.mouseButton.y;

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
    if (rmbdown) selectedtiles.clear();
}


void GameEngine::add_selectedtiles() {
    auto sq_from = selectedtiles[0];

    for (auto& move : movelist) {
        if (move.from() == sq_from) {
            // modify castling move to target empty tile
            if (move.typeOf() == chess::Move::CASTLING) {
                switch (move.to()) {
                    case chess::SQ_H1:  // white king-side
                        selectedtiles.push_back(chess::SQ_G1);
                        break;
                    case chess::SQ_A1:  // white queen-side
                        selectedtiles.push_back(chess::SQ_C1);
                        break;
                    case chess::SQ_H8:  // black king-side
                        selectedtiles.push_back(chess::SQ_G8);
                        break;
                    case chess::SQ_A8:  // black queen-side
                        selectedtiles.push_back(chess::SQ_C8);
                        break;
                    default: break;
                }
            } else
                selectedtiles.push_back(move.to());
        }
    }
}


void GameEngine::update_arrows() {
    // from arrow
    if (rmbdown) {
        int x = event.mouseButton.x;
        int y = event.mouseButton.y;
        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) arrow_from = get_square(x, y);
    }

    // to arrow
    else if (rmbup) {
        int x = event.mouseButton.x;
        int y = event.mouseButton.y;

        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) {
            auto arrow_to = get_square(x, y);
            if (arrow_to != arrow_from && arrow_from != chess::NO_SQ) {
                Arrow newarrow(arrow_from, arrow_to);

                // check arrow does not exist
                bool arrow_exists = false;
                for (size_t i = 0; i < arrows.size(); i++) {
                    if (newarrow == arrows[i]) {
                        arrows.erase(arrows.begin() + i);
                        arrow_from = chess::NO_SQ;
                        arrow_exists = true;
                        break;
                    }
                }

                // add arrow
                if (!arrow_exists) {
                    arrows.push_back(newarrow);
                    arrow_from = chess::NO_SQ;
                }
            }
        }
    }
}


void GameEngine::update_window() {
    // event handling
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window.close();
        if (event.type == sf::Event::MouseButtonPressed
            || event.type == sf::Event::MouseButtonReleased) {
            update_arrows();  // handle arrow placing
            update_select();  // handle move selection
        }
    }

    // clear window render
    window.clear(PALETTE::BG);

    // draw tiles
    for (int i = 0; i < 64; i++) window.draw(tiles[i]);

    // draw move to/from squares
    if (sq_from != chess::NO_SQ) {
        int file = (int)chess::utils::squareFile(sq_from);
        int rank = (int)chess::utils::squareRank(sq_from);
        tiles[64].setPosition(50 + 100 * file, 770 - 100 * rank);
        window.draw(tiles[64]);

        file = (int)chess::utils::squareFile(sq_to);
        rank = (int)chess::utils::squareRank(sq_to);
        tiles[64].setPosition(50 + 100 * file, 770 - 100 * rank);
        window.draw(tiles[64]);
    }

    // draw selection squares
    for (auto& sq : selectedtiles) {
        int file = (int)chess::utils::squareFile(sq);
        int rank = (int)chess::utils::squareRank(sq);
        tiles[65].setPosition(50 + 100 * file, 770 - 100 * rank);
        window.draw(tiles[65]);
    }

    // draw pieces
    auto pieces = board.occ();
    int check = 0;
    if (board.inCheck()) check = (whiteturn) ? 1 : -1;
    while (pieces) {
        auto sq = chess::builtin::poplsb(pieces);
        int file = (int)chess::utils::squareFile(sq);
        int rank = (int)chess::utils::squareRank(sq);
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
