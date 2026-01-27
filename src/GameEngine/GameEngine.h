#pragma once
#include <GameEngine/GamePlayer.h>
#include <GameEngine/utils.h>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>



namespace cge {     // chess game engine
class GameEngine {  // Class for managing games
    static constexpr i32 FRAMERATE = 60;

private:
    // visual & sound
    sf::RenderWindow window;
    sf::Font font;
    std::vector<sf::RectangleShape> tiles;
    std::vector<Timer> timers;
    std::vector<sf::Text> names;
    PieceDrawer piecedrawer;
    std::vector<sf::SoundBuffer> soundbuffers;
    std::vector<sf::Sound> sounds;
    MouseInfo mouse = {.x = 0, .y = 0, .event = cge::MouseEvent::NONE};
    bool interactive;  // play sound and keep window open after game end

    // for draw_select()
    chess::Square sq_from = chess::Square::NONE;
    chess::Square sq_to = chess::Square::NONE;
    std::vector<chess::Square> selectedtiles;
    chess::MoveList<chess::ScoredMove> movelist;

    // arrows
    chess::Square arrow_from = chess::Square::NONE;
    std::vector<Arrow> arrows;

    // chess game logic
    chess::Board board;
    bool turn;                         // current turn (0=white, 1=black)
    std::vector<GamePlayer*> players;  // players (p1, p2)
    std::vector<i64> t_remain;         // time remaining ms (white, black)

    // score tracking
    std::vector<i32> results = {0, 0, 0};   // (p1, draw, p2)
    std::vector<i32> timeoutwins = {0, 0};  // (p1, p2)
    std::vector<i32> whitewins = {0, 0};    // (p1, p2)

    enum class GameResult { NONE, LOSE, WIN, DRAW };

public:
    struct GameOptions {
        bool p1_is_white = true;  // whether p1 or p2 is white
        bool interactive = true;  // whether we play sound and keep window open after game end
        i32 t_inc = 0;            // ms incremented per move
        std::vector<i64> t_remain = {600000, 600000};    // ms remaining for p1 and p2
        std::string start_fen = chess::Board::STARTPOS;  // the starting chess position in FEN
        std::string pgn_file = "";                       // the pgn file to save the game to
    };



public:
    /** Initialize the GameEngine with the respective players
     *
     * \param players_in a vectors of player 1 and player 2
     */
    GameEngine(const std::vector<GamePlayer*>& players_in);

    /** Runs a match from start to end
     *
     * \param options options to run the game with
     */
    void run_match(const GameOptions& options);

    /** Prints the results of the games to stdout */
    void print_report() const;

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

    /** Updates the board with a move
     *
     * \param move_in the move to make
     */
    void move(chess::Move move_in);
};
}  // namespace cge
