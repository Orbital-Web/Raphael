#include <GameEngine/GameEngine.h>

#include <fstream>
#include <iostream>

using namespace cge;
using std::cout;
using std::fixed;
using std::flush;
using std::ios_base;
using std::min;
using std::ofstream;
using std::setprecision;
using std::string;
using std::stringstream;



GameEngine::GameEngine(): tiles(66, sf::RectangleShape({100, 100})), soundbuffers(3), players(2) {
    generate_assets();
}


void GameEngine::run_match(const GameOptions& options) {
    // open window
    window.create(sf::VideoMode({880, 940}), "Chess");
    window.setFramerateLimit(FRAMERATE);

    // initialize
    t_remain[0] = options.w_time;
    t_remain[1] = options.b_time;
    t_inc = options.inc_time;
    position.set_board(chess::Board(options.start_fen));
    chess::Movegen::generate_legals(movelist, position.board());
    GameResult game_result = GameResult::NONE;
    turn = position.board().stm() != chess::Color::WHITE;
    names[0].setString((options.w_is_engine) ? "Raphael" : "Player");
    names[1].setString((options.b_is_engine) ? "Raphael" : "Player");

    // initialize pgn
    bool savepgn = (options.pgn_file != "");
    ofstream pgn;
    stringstream pgn_moves;
    if (savepgn) {
        string fen = options.start_fen;
        fen = fen.substr(0, min(fen.find('\r'), fen.find('\n')) - 1);
        pgn.open(options.pgn_file, ios_base::app);
        pgn << "[White \"" << ((options.w_is_engine) ? "Raphael" : "Player") << "\"]\n"
            << "[Black \"" << ((options.b_is_engine) ? "Raphael" : "Player") << "\"]\n"
            << "[FEN \"" << fen << "\"]\n";
    }

    // start game
    i32 nmoves = 1;
    bool timeout = false;
    players[0].reset();
    players[1].reset();
    sounds[2].play();

    // until game ends or time runs out
    while (game_result == GameResult::NONE && t_remain[0] > 0 && t_remain[1] > 0) {
        const bool cur_is_engine = (turn) ? options.b_is_engine : options.w_is_engine;

        if (cur_is_engine) {
            players[turn].set_position(position);
            players[turn].start_search({.t_remain = t_remain[turn], .t_inc = t_inc});
        } else
            human.set_board(position.board());

        // get move while updating visuals
        chess::Move to_play = chess::Move::NO_MOVE;
        while (!to_play) {
            const auto start = std::chrono::steady_clock::now();

            if (cur_is_engine) {
                if (players[turn].is_search_complete()) {
                    const auto res = players[turn].wait_search();
                    to_play = res.move;
                    if (!to_play) {
                        cout << "error: no move returned. Remaining time of player: " << fixed
                             << setprecision(2) << t_remain[turn] / 1000.0f << "\n"
                             << flush;
                        timeout = true;
                        game_result = GameResult::LOSE;
                        break;
                    }

                    if (res.is_mate)
                        cout << "Score: #" << ((turn) ? -1 : 1) * res.score << "\n" << flush;
                    else
                        cout << "Score: " << fixed << setprecision(2)
                             << ((turn) ? -1 : 1) * res.score / 100.0f << "\n"
                             << flush;
                }
            } else
                to_play = human.try_get_move(mouse);

            update_window();

            // count down timer (only after first white moves)
            if (nmoves != 1) {
                const auto stop = std::chrono::steady_clock::now();
                t_remain[turn]
                    -= std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
            }

            // timeout
            if (t_remain[turn] <= 0 || !window.isOpen()) {
                players[turn].stop_search();
                timeout = true;
                game_result = GameResult::LOSE;
                break;
            }
        }

        if (timeout) break;

        // pgn saving FIXME: output san
        if (savepgn) pgn_moves << nmoves << ". " << chess::uci::from_move(to_play) << " ";

        // play move and update everything
        move(to_play);
        if (nmoves != 1) t_remain[turn] += t_inc;
        turn = !turn;
        nmoves++;

        // get game result (if any)
        game_result = GameResult::NONE;
        if (position.board().is_halfmovedraw()) game_result = GameResult::DRAW;
        if (position.board().is_insufficientmaterial()) game_result = GameResult::DRAW;
        if (position.is_repetition()) game_result = GameResult::DRAW;
        if (movelist.empty())
            game_result = (position.board().in_check()) ? GameResult::LOSE : GameResult::DRAW;
    }

    // print result
    if (game_result == GameResult::DRAW)
        cout << "Result: 1/2-1/2\n" << flush;
    else
        cout << "Result: " << ((turn) ? "1-0\n" : "0-1\n") << flush;

    // wait until window closed
    sounds[2].play();
    while (window.isOpen()) {
        while (const auto event = window.pollEvent())
            if (event->is<sf::Event::Closed>()) window.close();
        update_window();
    }

    sq_from = chess::Square::NONE;
    sq_to = chess::Square::NONE;
    movelist.clear();
    if (savepgn) {
        string pgn_result = "1/2-1/2";
        if (game_result != GameResult::DRAW) pgn_result = (turn) ? "1-0" : "0-1";
        pgn << "[Result \"" << pgn_result << "\"]\n\n" << pgn_moves.str() << "\n\n";
    };
    pgn.close();
}


void GameEngine::generate_assets() {
    // tiles
    for (i32 i = 0; i < 64; i++) {
        i32 y = i / 8;
        i32 x = i % 8;
        tiles[i].setPosition({50.0f + 100 * x, 70.0f + 100 * y});
        tiles[i].setFillColor(((x + y) % 2) ? PALETTE::TILE_B : PALETTE::TILE_W);
    }

    // special tiles
    tiles[64].setFillColor(PALETTE::TILE_MOV);  // move to/from
    tiles[65].setFillColor(PALETTE::TILE_SEL);  // selection squares

    // timer and player names
    bool font_loaded = true;
    font_loaded &= font.openFromFile("src/assets/fonts/Roboto-Regular.ttf");
    if (!font_loaded) cout << "Warning, could not load font\n" << flush;
    timers.reserve(2);
    names.reserve(2);
    for (i32 i = 0; i < 2; i++) {
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
    if (!sound_loaded) cout << "Warning, could not load 1 or more sound files\n" << flush;
    sounds.reserve(3);
    for (i32 i = 0; i < 3; i++) sounds.emplace_back(soundbuffers[i]);
}


void GameEngine::update_select() {
    // populate selected squares
    if (mouse.event == MouseEvent::LMBDOWN) {
        arrows.clear();
        const i32 x = mouse.x;
        const i32 y = mouse.y;

        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) {
            const auto sq = get_square(x, y);
            const auto piece = position.board().at(sq);

            // own pieces clicked
            if (piece != chess::Piece::NONE && piece.color() == position.board().stm()) {
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
    const auto sq_from = selectedtiles[0];

    for (const auto& move : movelist) {
        if (move.move.from() == sq_from) {
            // modify castling move to target empty tile
            if (move.move.type() == chess::Move::CASTLING) {
                if (move.move.to() == chess::Square::H1)  // white king-side
                    selectedtiles.push_back(chess::Square::G1);
                else if (move.move.to() == chess::Square::A1)  // white queen-side
                    selectedtiles.push_back(chess::Square::C1);
                else if (move.move.to() == chess::Square::H8)  // black king-side
                    selectedtiles.push_back(chess::Square::G8);
                else if (move.move.to() == chess::Square::A8)  // black queen-side
                    selectedtiles.push_back(chess::Square::C8);
            } else
                selectedtiles.push_back(move.move.to());
        }
    }
}


void GameEngine::update_arrows() {
    if (mouse.event == MouseEvent::RMBDOWN) {
        // from arrow
        const i32 x = mouse.x;
        const i32 y = mouse.y;
        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) arrow_from = get_square(x, y);

    } else if (mouse.event == MouseEvent::RMBUP) {
        // to arrow
        const i32 x = mouse.x;
        const i32 y = mouse.y;

        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) {
            auto arrow_to = get_square(x, y);
            if (arrow_to != arrow_from && arrow_from != chess::Square::NONE) {
                Arrow newarrow(arrow_from, arrow_to);

                // check arrow does not exist
                bool arrow_exists = false;
                for (usize i = 0; i < arrows.size(); i++) {
                    if (newarrow == arrows[i]) {
                        arrows.erase(arrows.begin() + i);
                        arrow_from = chess::Square::NONE;
                        arrow_exists = true;
                        break;
                    }
                }

                // add arrow
                if (!arrow_exists) {
                    arrows.push_back(newarrow);
                    arrow_from = chess::Square::NONE;
                }
            }
        }
    }
}


void GameEngine::update_window() {
    // event handling
    while (const auto event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            players[0].stop_search();
            players[1].stop_search();
            window.close();
        } else if (const auto* mouseevent = event->getIf<sf::Event::MouseButtonPressed>()) {
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
    for (i32 i = 0; i < 64; i++) window.draw(tiles[i]);

    // draw move to/from squares
    if (sq_from != chess::Square::NONE) {
        auto file = sq_from.file();
        auto rank = sq_from.rank();
        tiles[64].setPosition({50.0f + 100 * file, 770.0f - 100 * rank});
        window.draw(tiles[64]);

        file = sq_to.file();
        rank = sq_to.rank();
        tiles[64].setPosition({50.0f + 100 * file, 770.0f - 100 * rank});
        window.draw(tiles[64]);
    }

    // draw selection squares
    for (const auto& sq : selectedtiles) {
        const auto file = sq.file();
        const auto rank = sq.rank();
        tiles[65].setPosition({50.0f + 100 * file, 770.0f - 100 * rank});
        window.draw(tiles[65]);
    }

    // draw pieces
    auto pieces = position.board().occ();
    i32 check = 0;
    if (position.board().in_check()) check = (turn) ? -1 : 1;
    while (pieces) {
        const auto sq = static_cast<chess::Square>(pieces.poplsb());
        const auto file = sq.file();
        const auto rank = sq.rank();
        const auto piece = position.board().at(sq);
        piecedrawer.draw(window, piece, 50 + 100 * file, 770 - 100 * rank, check);
    }

    // draw timers and names
    for (i32 i = 0; i < 2; i++) {
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
    if (position.board().is_capture(move_in))
        sounds[1].play();  // capture
    else
        sounds[0].play();  // move

    position.make_move(move_in);
    movelist.clear();
    chess::Movegen::generate_legals(movelist, position.board());
}
