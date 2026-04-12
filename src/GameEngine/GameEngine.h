#pragma once
#include <GameEngine/HumanPlayer.h>
#include <GameEngine/utils.h>
#include <Raphael/Raphael.h>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <vector>



namespace cge {     // chess game engine
class GameEngine {  // Class for managing games
private:
    // visual & sound
    static constexpr i32 FRAMERATE = 60;
    sf::RenderWindow window;
    sf::Font font;
    std::vector<sf::RectangleShape> tiles;
    std::vector<Timer> timers;
    std::vector<sf::Text> names;
    PieceDrawer piecedrawer;
    std::vector<sf::SoundBuffer> soundbuffers;
    std::vector<sf::Sound> sounds;
    MouseInfo mouse{.x = 0, .y = 0, .event = cge::MouseEvent::NONE};

    // for draw_select()
    chess::Square sq_from = chess::Square::NONE;
    chess::Square sq_to = chess::Square::NONE;
    std::vector<chess::Square> selectedtiles;
    chess::MoveList<chess::ScoredMove> movelist;

    // arrows
    chess::Square arrow_from = chess::Square::NONE;
    std::vector<Arrow> arrows;

    // players
    std::vector<raphael::Raphael> players;
    HumanPlayer human;

    // chess logic and time
    raphael::Position<false> position;
    bool turn;                           // current turn (0=white, 1=black)
    std::vector<i32> t_remain = {0, 0};  // (w, b)
    i32 t_inc;

    enum class GameResult { NONE, LOSE, WIN, DRAW };

public:
    struct GameOptions {
        bool w_is_engine = false;
        bool b_is_engine = true;
        i32 w_time = 600000;
        i32 b_time = 600000;
        i32 inc_time = 1000;
        std::string start_fen = chess::Board::STARTPOS;
        std::string pgn_file = "";  // the pgn file to save the game to
    };



public:
    /** Initialize the GameEngine */
    GameEngine();

    /** Runs a match from start to end
     *
     * \param options options to run the game with
     */
    void run_match(const GameOptions& options);

private:
    /** Initializes sprites, texts, and sounds */
    void generate_assets();

    /** Updates the selected squares list */
    void update_select();

    /** Adds squares that selectedtiles[0] can move to */
    void add_selectedtiles();

    /** Updated drawn arrows */
    void update_arrows();

    /** Handles window events and renderings */
    void update_window();

    /** Updates the position with a move
     *
     * \param move_in the move to make
     */
    void move(chess::Move move_in);
};
}  // namespace cge
