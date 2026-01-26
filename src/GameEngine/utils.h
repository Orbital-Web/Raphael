#pragma once
#include <chess/include.h>

#include <SFML/Graphics.hpp>



namespace cge {
namespace PALETTE {
const sf::Color BG(22, 21, 18);                // background
const sf::Color TIMER_A(56, 71, 34);           // active timer
const sf::Color TIMER_ALOW(115, 49, 44);       // active timer (<60sec)
const sf::Color TIMER(38, 36, 33);             // inactive timer
const sf::Color TIMER_LOW(78, 41, 40);         // inactive timer (<60sec)
const sf::Color TILE_W(240, 217, 181);         // white tile
const sf::Color TILE_B(177, 136, 106);         // black tile
const sf::Color TILE_MOV(170, 162, 58, 200);   // tile moved to&from
const sf::Color TILE_SEL(130, 151, 105, 200);  // tile to move to
const sf::Color TEXT(255, 255, 255);           // text
}  // namespace PALETTE



class Arrow: public sf::Drawable {  // An arrow drawable that goes from squares `from` to `to`
    static constexpr f32 THICKNESS = 40;
    static constexpr f32 ARROWSIZE = 40;

private:
    chess::Square from, to;
    sf::ConvexShape arrowhead;
    sf::RectangleShape arrowbody;

public:
    Arrow(chess::Square from_in, chess::Square to_in);
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    bool operator==(const Arrow& rhs) const;
};



class PieceDrawer {  // Class for drawing a piece on screen
private:
    std::vector<sf::Texture> textures;
    std::vector<sf::Sprite> sprites;

public:
    PieceDrawer();
    void draw(sf::RenderWindow& window, chess::Piece piece, f32 x, f32 y, i32 check);
};



class Timer: public sf::Drawable {  // A timer drawable
private:
    bool top;
    std::string t_disp;
    sf::Text timertext;
    sf::RectangleShape timerbox;

public:
    Timer(bool at_top, const sf::Font& font);
    void update(f32 time, bool active);
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
};



/** Converts x and y coordinates into a Square
 *
 * \param x x coordinate on screen
 * \param y y coordinate on screen
 * \returns the square at that coordinate
 */
chess::Square get_square(i32 x, i32 y);
}  // namespace cge
