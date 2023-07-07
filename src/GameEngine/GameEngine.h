#pragma once
#include "GamePlayer.h"
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <array>
#include <chrono>
#include <vector>



namespace cge { // chess game engine
namespace PALETTE {
    const sf::Color BG(22, 21, 18);             // background
    const sf::Color TIMER_A(56, 71, 34);        // active timer
    const sf::Color TIMER_ALOW(115, 49, 44);    // active timer (<60sec)
    const sf::Color TIMER(38, 36, 33);          // inactive timer
    const sf::Color TIMER_LOW(78, 41, 40);      // inactive timer (<60sec)
    const sf::Color TILE_W(240, 217, 181);      // white tile
    const sf::Color TILE_B(177, 136, 106);      // black tile
    const sf::Color TILE_MOV(170, 162, 58);     // tile moved to&from
    const sf::Color TILE_SEL(130, 151, 105);    // tile to move to
    const sf::Color TEXT(255, 255, 255);        // text
}   // cge::PALETTE
const std::string TEXTURE[12] = {
    "wP", "wN", "wB", "wR", "wQ", "wK",
    "bP", "bN", "bB", "bR", "bQ", "bK"
};



class GameEngine {
// class variables
private:
    // visual & sound
    const int FRAMERATE = 60; // actual frate tends to be half of target at higher values
    const std::chrono::milliseconds WAITTIME = std::chrono::milliseconds(1000/FRAMERATE);
    sf::RenderWindow* window;
    sf::Event windowevent;
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
    bool interactive;

    // chess game logic
    chess::Board board;
    bool turn;                                  // current turn (0=white, 1=black)
    std::array<cge::GamePlayer*, 2> players;    // players (p1, p2)
    std::array<float, 2> t_remain;              // time remaining (white, black)
    // visual-related game logic
    chess::Square sq_from = chess::Square::NO_SQ;
    chess::Square sq_to = chess::Square::NO_SQ;
    std::vector<chess::Square> selectedtiles;
    chess::Movelist movelist;

    // score tracking
    std::array<int, 3> results = {0, 0, 0};     // (p1, draw, p2)


// methods
public:
    // Initialize the GameEngine with the respective players
    GameEngine(std::array<cge::GamePlayer*, 2> players_in);
    
    // Runs a match from start to end
    void run_match(
        bool p1_is_white,                   // true
        std::string start_fen,              // "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        std::array<float, 2> t_remain_in,   // {600, 600}
        bool is_interactive                 // true
    );

    // Runs several matches with set starting position to
    // compare the strengths of each player. Best used to
    // compare different engines
    //void compare_players();

    // Print result of the games
    void print_report();

private:
    // Generates sprites, texts, and sounds
    void generate_assets();

    // Renders the chess pieces
    void draw_pieces();

    // Renders the timer and names
    void draw_timer();

    // Draws possible move square and move to/from squares
    void draw_select();

    // Converts x and y coordinates into a Square
    static chess::Square get_square(int x, int y);

    // Adds squares a selectedtiles[0] can move to
    void add_selectedtiles();

    // Handles window events and rendering
    void update_window();

    // Updates the board with a move
    void move(chess::Move move_in);

    //void add_time(std::array<float, 2> t_extra);
};  // GameEngine
}   // namespace cge