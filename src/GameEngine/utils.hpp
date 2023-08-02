#pragma once
#include <chess.hpp>
#include <SFML/Graphics.hpp>
#include <math.h>



// mouse buttons
#define rmbdown (event.type==sf::Event::MouseButtonPressed && event.mouseButton.button==sf::Mouse::Right)
#define rmbup (event.type==sf::Event::MouseButtonReleased && event.mouseButton.button==sf::Mouse::Right)
#define lmbdown (event.type==sf::Event::MouseButtonPressed && event.mouseButton.button==sf::Mouse::Left)
#define lmbup (event.type==sf::Event::MouseButtonReleased && event.mouseButton.button==sf::Mouse::Left)


// [bool] curently white's turn
#define whiteturn (board.sideToMove() == chess::Color::WHITE)


// [bool] square is a light tile
#define lighttile(sqi) (((sq >> 3) ^ sq) & 1)


#define EMPTY_MOVE (chess::Move(chess::Move::NO_MOVE))


namespace cge {

// An arrow drawable that goes from squares `from` to `to`
class Arrow : public sf::Drawable {
private:
    chess::Square to, from;
    sf::ConvexShape arrowhead;
    sf::RectangleShape arrowbody;


public:
    Arrow(const chess::Square from_in, chess::Square to_in, const float thickness,
    const float arrowsize, const sf::Color color): from(from_in), to(to_in) {
        sf::Vector2f from_coord = {
            100.0f + 100*(int)chess::utils::squareFile(from),
            820.0f - 100*(int)chess::utils::squareRank(from)
        };
        sf::Vector2f to_coord = {
            100.0f + 100*(int)chess::utils::squareFile(to),
            820.0f - 100*(int)chess::utils::squareRank(to)
        };

        float dx = to_coord.x - from_coord.x;
        float dy = from_coord.y - to_coord.y;
        float headlen = arrowsize*1.5f;
        float arrowlen = sqrt(dx*dx + dy*dy);
        float arrowang = 180*atan2(dx, dy)/M_PI;

        arrowbody.setSize({thickness, arrowlen - headlen});
        arrowbody.setOrigin(0.5f*thickness, arrowlen - headlen);
        arrowbody.setPosition(from_coord);
        arrowbody.setRotation(arrowang);
        arrowbody.setFillColor(color);

        arrowhead.setPointCount(3);
        arrowhead.setPoint(0, sf::Vector2f(0, 0));
        arrowhead.setPoint(1, sf::Vector2f(-0.9f*arrowsize, headlen));
        arrowhead.setPoint(2, sf::Vector2f(0.9f*arrowsize, headlen));
        arrowhead.setOrigin(0, arrowlen);
        arrowhead.setPosition(from_coord);
        arrowhead.setRotation(arrowang);
        arrowhead.setFillColor(color);
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        target.draw(arrowhead);
        target.draw(arrowbody);
    }

    bool operator==(const Arrow& rhs) {
        return ((to==rhs.to) && (from==rhs.from));
    }
};


// Converts x and y coordinates into a Square
chess::Square get_square(int x, int y) {
    int rank = (870 - y) / 100;
    int file = (x - 50) / 100;
    return chess::utils::fileRankSquare(chess::File(file), chess::Rank(rank));
}
}   // namespace cge