#pragma once
#include <chess.hpp>
#include <SFML/Graphics.hpp>
#include <math.h>
#include <iomanip>



// mouse buttons
#define rmbdown (event.type==sf::Event::MouseButtonPressed && event.mouseButton.button==sf::Mouse::Right)
#define rmbup (event.type==sf::Event::MouseButtonReleased && event.mouseButton.button==sf::Mouse::Right)
#define lmbdown (event.type==sf::Event::MouseButtonPressed && event.mouseButton.button==sf::Mouse::Left)
#define lmbup (event.type==sf::Event::MouseButtonReleased && event.mouseButton.button==sf::Mouse::Left)

// [bool] curently white's turn
#define whiteturn (board.sideToMove() == chess::Color::WHITE)

// [bool] square is a light tile
#define lighttile(sqi) (((sq >> 3) ^ sq) & 1)



namespace cge {
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



class Arrow : public sf::Drawable {     // An arrow drawable that goes from squares `from` to `to`
static constexpr float THICKNESS = 40;
static constexpr float ARROWSIZE = 40;

// Arrow vars
private:
    chess::Square to, from;
    sf::ConvexShape arrowhead;
    sf::RectangleShape arrowbody;


// Arrow methods
public:
    Arrow(const chess::Square from_in, const chess::Square to_in): from(from_in), to(to_in) {
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
        float headlen = ARROWSIZE*1.5f;
        float arrowlen = sqrt(dx*dx + dy*dy);
        float arrowang = 180*atan2(dx, dy)/M_PI;

        arrowbody.setSize({THICKNESS, arrowlen - headlen});
        arrowbody.setOrigin(0.5f*THICKNESS, arrowlen - headlen);
        arrowbody.setPosition(from_coord);
        arrowbody.setRotation(arrowang);
        arrowbody.setFillColor(PALETTE::TILE_SEL);

        arrowhead.setPointCount(3);
        arrowhead.setPoint(0, sf::Vector2f(0, 0));
        arrowhead.setPoint(1, sf::Vector2f(-0.9f*ARROWSIZE, headlen));
        arrowhead.setPoint(2, sf::Vector2f(0.9f*ARROWSIZE, headlen));
        arrowhead.setOrigin(0, arrowlen);
        arrowhead.setPosition(from_coord);
        arrowhead.setRotation(arrowang);
        arrowhead.setFillColor(PALETTE::TILE_SEL);
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        target.draw(arrowhead);
        target.draw(arrowbody);
    }

    bool operator==(const Arrow& rhs) const {
        return ((to==rhs.to) && (from==rhs.from));
    }
};  // Arrow



class PieceDrawer {     // Class for drawing a piece on screen
// PieceDrawer vars
private:
    std::vector<sf::Texture> textures;
    std::vector<sf::Sprite> sprites;


// PieceDrawer methods
public:
    PieceDrawer(): textures(12), sprites(12) {
        const std::string TEXTURE[12] = {
            "wP", "wN", "wB", "wR", "wQ", "wK",
            "bP", "bN", "bB", "bR", "bQ", "bK"
        };
        for (int i=0; i<12; i++) {
            textures[i].loadFromFile("src/assets/themes/tartiana/" + TEXTURE[i] + ".png");
            textures[i].setSmooth(true);
            sprites[i].setTexture(textures[i]);
        }
    }

    void draw(sf::RenderWindow& window, const chess::Piece piece, const float x, const float y) {
        int i = (int)piece;
        assert((i != 12));
        sprites[i].setPosition(x, y);
        window.draw(sprites[i]);
    }
};  // PieceDrawer



class Timer : public sf::Drawable {     // A timer drawable
// Timer vars
private:
    bool top;
    std::string t_disp;
    sf::Text timertext;
    sf::RectangleShape timerbox;


// Timer methods
public:
    Timer(const bool at_top, const sf::Font& font): top(at_top), timertext("", font, 40), timerbox({180, 50}) {
        timerbox.setPosition(660, (top) ? 10 : 880);
        timertext.setFillColor(PALETTE::TEXT);
    }

    void update(const float time, const bool active) {
        // accomodate different format
        std::stringstream formatter;

        if (time >= 60) {    // mm : ss
            timerbox.setFillColor((active) ? PALETTE::TIMER_A : PALETTE::TIMER);
            int min = (int)time / 60;
            int sec = (int)time % 60;
            formatter << std::setw(2) << std::setfill('0') << std::to_string(sec);
            t_disp = std::to_string(min) + " : " + formatter.str();
        } else {                    // ss.mm
            timerbox.setFillColor((active) ? PALETTE::TIMER_ALOW : PALETTE::TIMER_LOW);
            formatter << std::fixed << std::setprecision(1) << time;
            t_disp = formatter.str();
        }

        // manage positioning of string
        timertext.setString(t_disp);
        auto textbounds = timertext.getLocalBounds();
        timertext.setPosition(820 - textbounds.width, (top) ? 10 : 880);
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        target.draw(timerbox);
        target.draw(timertext);
    }
};  // Timer



// Converts x and y coordinates into a Square
chess::Square get_square(int x, int y) {
    int rank = (870 - y) / 100;
    int file = (x - 50) / 100;
    return chess::utils::fileRankSquare(chess::File(file), chess::Rank(rank));
}
}   // namespace cge