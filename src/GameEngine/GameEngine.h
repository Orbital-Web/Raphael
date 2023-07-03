#pragma once
#include "GamePlayer.h"
#include <SFML/Graphics.hpp>
#include <string>
#include <array>
#include <chrono>



namespace cge { // chess game engine
namespace Palette {
    const sf::Color BG(49, 46, 43);
    const sf::Color TIMER_W(152, 151, 149);
    const sf::Color TIMER_B(43, 39, 34);
    const sf::Color TILE_W(240, 217, 181);
    const sf::Color TILE_B(177, 136, 106);
    const sf::Color TILE_SEL(205, 210, 106);
    const sf::Color TEXT(255, 255, 255);
}   // cge::Palette



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
    sf::Font font;

    // chess game manager
    thc::ChessRules manager;
    thc::Move mv;
    thc::TERMINAL game_ended = thc::NOT_TERMINAL;
    bool turn = false;                          // black's turn
    std::array<float, 2> t_remain;              // (white, black)
    std::array<cge::GamePlayer*, 2> players;    // (white, black)


// methods
public:
    // Initialize the GameEngine with the respective
    // players and time remaining
    GameEngine(std::array<cge::GamePlayer*, 2> players_in);
    GameEngine(std::array<float, 2> t_remain_in, std::array<cge::GamePlayer*, 2> players_in);
    
    // Run a match from start to end
    void run_match();

    //void run_tests(int n_matches);

    //void reset();

    //void load_game();

    //void save_game();

private:
    // generates (mostly) static sprites such as the tiles and timers
    void generate_sprites();

    // handles window events and rendering
    void update_window();

    // updates the manager with a move
    void move(std::string movestr);

    void undo();

    void add_time(std::array<float, 2> t_extra);
};  // GameEngine
}   // namespace cge