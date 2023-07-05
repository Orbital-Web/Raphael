#pragma once
#include "GamePlayer.h"
#include <SFML/Graphics.hpp>
#include <string>
#include <array>
#include <chrono>
#include <map>



namespace cge { // chess game engine
namespace PALETTE {
    const sf::Color BG(22, 21, 18);
    const sf::Color TIMER_A(56, 71, 34);
    const sf::Color TIMER_ALOW(115, 49, 44);
    const sf::Color TIMER(38, 36, 33);
    const sf::Color TIMER_LOW(78, 41, 40);
    const sf::Color TILE_W(240, 217, 181);
    const sf::Color TILE_B(177, 136, 106);
    const sf::Color TILE_SEL(191, 197, 85, 160);
    const sf::Color TEXT(255, 255, 255);
}   // cge::PALETTE
const std::string TEXTURE[12] = {
    "wP", "wK", "wQ", "wN", "wB", "wR",
    "bP", "bK", "bQ", "bN", "bB", "bR"
};
const std::map<char, int> PIECENAME = {
    {'P', 0}, {'K', 1}, {'Q', 2}, {'N', 3}, {'B', 4}, {'R', 5},
    {'p', 6}, {'k', 7}, {'q', 8}, {'n', 9}, {'b',10}, {'r',11}
};



class GameEngine {
// class variables
private:
    // visual
    const int FRAMERATE = 60; // actual frate tends to be half of target at higher values
    const std::chrono::milliseconds WAITTIME = std::chrono::milliseconds(1000/FRAMERATE);
    sf::RenderWindow* window;
    sf::Event windowevent;
    std::array<sf::RectangleShape, 64> tiles;
    std::array<sf::RectangleShape, 2> timers;
    std::array<sf::Text, 2> names;
    std::array<sf::Text, 2> timertexts;
    std::array<sf::Texture, 12> piecetextures;
    std::array<sf::Sprite, 12> pieces;
    sf::Font font;

    // chess game manager
    thc::ChessRules manager;
    thc::Move mv;
    thc::TERMINAL game_ended = thc::NOT_TERMINAL;
    bool turn = false;                          // black's turn
    std::array<float, 2> t_remain;              // (white, black)
    std::array<cge::GamePlayer*, 2> players;    // (white, black)

    // score tracking
    std::array<int, 3> results = {0, 0, 0};     // (white, draw, black)


// methods
public:
    // Initialize the GameEngine with the respective
    // players and time remaining
    GameEngine(std::array<cge::GamePlayer*, 2> players_in);
    GameEngine(std::array<float, 2> t_remain_in, std::array<cge::GamePlayer*, 2> players_in);
    
    // Run a match from start to end
    void run_match();

    //void run_tests(int n_matches);

    // Print result of the games
    void print_report();

    //void restart();

    //void load_game();

    //void save_game();

private:
    // Generates (mostly) static sprites such as the tiles and timers
    void generate_sprites();

    // Renders the chess pieces
    void draw_pieces();

    // Renders the timer and names
    void draw_timer();

    // Handles window events and rendering
    void update_window();

    // Updates the manager with a move
    void move(std::string movestr);

    //void undo();

    //void add_time(std::array<float, 2> t_extra);
};  // GameEngine
}   // namespace cge