#pragma once
#include "HumanPlayer.hpp"
#include "utils.hpp"
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <future>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>



namespace cge { // chess game engine
namespace PALETTE {
    const sf::Color BG(22, 21, 18);                 // background
    const sf::Color TIMER_A(56, 71, 34);            // active timer
    const sf::Color TIMER_ALOW(115, 49, 44);        // active timer (<60sec)
    const sf::Color TIMER(38, 36, 33);              // inactive timer
    const sf::Color TIMER_LOW(78, 41, 40);          // inactive timer (<60sec)
    const sf::Color TILE_W(240, 217, 181);          // white tile
    const sf::Color TILE_B(177, 136, 106);          // black tile
    const sf::Color TILE_MOV(170, 162, 58, 200);    // tile moved to&from
    const sf::Color TILE_SEL(130, 151, 105, 200);   // tile to move to
    const sf::Color TEXT(255, 255, 255);            // text
}
const std::string TEXTURE[12] = {
    "wP", "wN", "wB", "wR", "wQ", "wK",
    "bP", "bN", "bB", "bR", "bQ", "bK"
};



class GameEngine {
// class variables
private:
    // visual & sound
    const int FRAMERATE = 60;
    sf::RenderWindow window;
    sf::Event event;
    std::array<sf::RectangleShape, 64> tiles;
    std::array<sf::RectangleShape, 2> tilesspecial;
    std::array<sf::RectangleShape, 2> timers;
    std::array<sf::Text, 2> timertexts;
    std::array<sf::Text, 2> names;
    sf::Font font;
    std::array<sf::Texture, 12> piecetextures;
    std::array<sf::Sprite, 12> pieces;
    std::array<sf::SoundBuffer, 3> soundbuffers;
    std::array<sf::Sound, 3> sounds;
    bool interactive;       // play sound and keep window open after game end
    // for draw_select()
    chess::Square sq_from = chess::NO_SQ;
    chess::Square sq_to = chess::NO_SQ;
    std::vector<chess::Square> selectedtiles;
    chess::Movelist movelist;
    // arrows
    chess::Square arrow_from = chess::NO_SQ;
    std::vector<Arrow> arrows;

    // chess game logic
    chess::Board board;
    bool turn;                                  // current turn (0=white, 1=black)
    std::array<GamePlayer*, 2> players;         // players (p1, p2)
    std::array<float, 2> t_remain;              // time remaining (white, black)

    // score tracking
    std::array<int, 3> results = {0, 0, 0};     // (p1, draw, p2)
    std::array<int, 2> timeoutwins = {0, 0};    // (p1, p2)
    std::array<int, 2> whitewins = {0, 0};      // (p1, p2)


// methods
public:
    // Initialize the GameEngine with the respective players
    GameEngine(std::array<GamePlayer*, 2> players_in): players(players_in) {
        generate_assets();
    }
    

    // Runs a match from start to end
    void run_match(bool p1_is_white, std::string start_fen, std::array<float, 2> t_remain_in, bool is_interactive) {
        // open window
        window.create(sf::VideoMode(880, 940), "Chess");
        window.setFramerateLimit(FRAMERATE);
        
        // initialize board
        board = chess::Board(start_fen);
        chess::movegen::legalmoves(movelist, board);
        chess::GameResult game_result = chess::GameResult::NONE;
        event.type = sf::Event::MouseMoved; // in case run_match is called consecutively
        bool timeout = false;

        // reset players
        players[0]->reset();
        players[1]->reset();

        // set time
        t_remain = t_remain_in;

        // manage turns
        names[0].setString(players[!p1_is_white]->name);
        names[1].setString(players[p1_is_white]->name);
        turn = !whiteturn;

        interactive = is_interactive;
        if (interactive)
            sounds[2].play();
        
        // until game ends or time runs out
        while (game_result==chess::GameResult::NONE && t_remain[0]>0 && t_remain[1]>0) {
            auto& cur_player = players[turn==p1_is_white];
            auto& oth_player = players[turn!=p1_is_white];
            float& cur_t_remain = t_remain[turn];

            // ask player for move in seperate thread so that we can keep rendering
            bool halt = false;
            auto movereceiver = std::async(&cge::GamePlayer::get_move, cur_player,
                                        board, cur_t_remain, std::ref(event), std::ref(halt));
            auto status = std::future_status::timeout;

            // allow other player to ponder
            auto _ = std::async(&cge::GamePlayer::ponder, oth_player, board, std::ref(halt));

            // timings
            std::chrono::_V2::system_clock::time_point start, stop;

            // update visuals until a move is returned
            while(status!=std::future_status::ready) {
                start = std::chrono::high_resolution_clock::now();
                status = movereceiver.wait_for(std::chrono::milliseconds(0));

                // game loop
                update_window();

                // count down timer
                stop = std::chrono::high_resolution_clock::now();
                cur_t_remain -= std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count()/1000.0;

                // timeout
                if (cur_t_remain<=0 || event.type==sf::Event::Closed) {
                    timeoutwins[(p1_is_white!=turn)]++;
                    timeout = true;
                    game_result = chess::GameResult::LOSE;
                    halt = true;
                    goto game_end;
                }
            }

            // play move
            auto toPlay = movereceiver.get();
            if (toPlay == chess::Move::NO_MOVE) {
                timeout = true;
                timeoutwins[(p1_is_white!=turn)]++;
                game_result = chess::GameResult::LOSE;
                goto game_end;
            }
            halt = true;    // force stop pondering
            move(toPlay);
            turn = !turn;
            game_result = board.isGameOver().second;
        }

        game_end:
        // score tracking
        if (game_result==chess::GameResult::DRAW)
            results[1]++;
        else {
            results[(p1_is_white==turn) ? 0 : 2]++;
            if (!timeout && ((p1_is_white==turn)==p1_is_white))
                whitewins[(p1_is_white!=turn)]++;
        }
        
        // wait until window closed if interactive
        if (interactive) {
            sounds[2].play();
            while (window.isOpen()) {
                while (window.pollEvent(event))
                    if (event.type == sf::Event::Closed)
                        window.close();
                update_window();
            }
        }
        sq_from = chess::NO_SQ;
        sq_to = chess::NO_SQ;
        movelist.clear();
    }


    // Print result of the games
    void print_report() const {
        int total_matches = results[0] + results[1] + results[2];
        printf("Out of %d total matches:\n", total_matches);
        // white wins
        printf("   %s: \t\x1b[32m%d (white: %d, ", players[0]->name.c_str(), results[0], whitewins[0]);
        printf("black: %d, timeout: %d)\x1b[0m\n", results[0]-whitewins[0]-timeoutwins[0], timeoutwins[0]);
        // draws
        printf("   Draw: \t\x1b[90m%d\x1b[0m\n", results[1]);
        // black wins
        printf("   %s: \t\x1b[31m%d (white: %d, ", players[1]->name.c_str(), results[2], whitewins[1]);
        printf("black: %d, timeout: %d)\x1b[0m\n", results[2]-whitewins[1]-timeoutwins[1], timeoutwins[1]);
    }

private:
    // Generates sprites, texts, and sounds
    void generate_assets() {
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
    void draw_pieces() {
        for (int rank=0; rank<8; rank++) {
            for (int file=0; file<8; file++) {
                auto sq = chess::utils::fileRankSquare(chess::File(file), chess::Rank(rank));
                int piece = (int)board.at(sq);
                if (piece != 12) {
                    pieces[(int)piece].setPosition(50 + 100*file, 770 - 100*rank);
                    window.draw(pieces[piece]);
                }
            }
        }
    }


    // Renders the timer and names
    void draw_timer() {
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
            window.draw(timers[i]);
            window.draw(timertexts[i]);
            window.draw(names[i]);
        }
    }


    // Update selected squares list
    void update_select() {
        // populate selected squares
        if (lmbdown) {
            arrows.clear();
            int x = event.mouseButton.x;
            int y = event.mouseButton.y;

            // board clicked
            if (x>50 && x<850 && y>70 && y<870) {
                auto sq = get_square(x, y);
                int piece = (int)board.at(sq);

                // own pieces clicked
                if (piece!=12 && whiteturn!=(piece/6)) {
                    selectedtiles.clear();
                    selectedtiles.push_back(sq);
                    add_selectedtiles();
                } else
                    selectedtiles.clear();
            }
        }
    }


    // Adds squares that selectedtiles[0] can move to (draw_select)
    void add_selectedtiles() {
        auto sq_from = selectedtiles[0];

        for (auto& move : movelist) {
            if (move.from()==sq_from) {
                // modify castling move to target empty tile
                if (move.typeOf()==chess::Move::CASTLING) {
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
                    }
                } else
                    selectedtiles.push_back(move.to());
            }
        }
    }


    // Update arrows
    void update_arrows() {
        // from arrow
        if (rmbdown) {
            int x = event.mouseButton.x;
            int y = event.mouseButton.y;
            // board clicked
            if (x>50 && x<850 && y>70 && y<870)
                arrow_from = get_square(x, y);
        }

        // to arrow
        else if (rmbup) {
            int x = event.mouseButton.x;
            int y = event.mouseButton.y;

            // board clicked
            if (x>50 && x<850 && y>70 && y<870) {
                auto arrow_to = get_square(x, y);
                if (arrow_to!=arrow_from && arrow_from!=chess::NO_SQ) {
                    Arrow newarrow(arrow_from, arrow_to, 40, 40, PALETTE::TILE_SEL);

                    // check arrow does not exist
                    bool arrow_exists = false;
                    for (int i=0; i<arrows.size(); i++) {
                        if (newarrow==arrows[i]) {
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


    // Handles window events and rendering
    void update_window() {
        // event handling
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::MouseButtonPressed || event.type==sf::Event::MouseButtonReleased)
                update_arrows();  // handle arrow placing
                update_select();  // handle move selection
        }

        // clear window render
        window.clear(cge::PALETTE::BG);

        // draw tiles
        for (int i=0; i<64; i++)
            window.draw(tiles[i]);
        
        // draw move to/from squares
        if (sq_from!=chess::NO_SQ) {
            int file = (int)chess::utils::squareFile(sq_from);
            int rank = (int)chess::utils::squareRank(sq_from);
            tilesspecial[0].setPosition(50 + 100*file, 770 - 100*rank);
            window.draw(tilesspecial[0]);

            file = (int)chess::utils::squareFile(sq_to);
            rank = (int)chess::utils::squareRank(sq_to);
            tilesspecial[0].setPosition(50 + 100*file, 770 - 100*rank);
            window.draw(tilesspecial[0]);
        }

        //draw selection squares
        for (auto& sq : selectedtiles) {
            int file = (int)chess::utils::squareFile(sq);
            int rank = (int)chess::utils::squareRank(sq);
            tilesspecial[1].setPosition(50 + 100*file, 770 - 100*rank);
            window.draw(tilesspecial[1]);
        }

        draw_pieces();  // draw pieces
        draw_timer();   // draw timer and names

        // draw arrows
        for (auto& arrow : arrows)
            window.draw(arrow);

        // update window render
        window.display();
    }


    // Updates the board with a move
    void move(chess::Move move_in) {
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
};  // GameEngine
}   // namespace cge