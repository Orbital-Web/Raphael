#include <GameEngine/consts.h>
#include <GameEngine/utils.h>

#include <cmath>

using namespace cge;
using std::cout;
using std::fixed;
using std::lock_guard;
using std::mutex;
using std::setfill;
using std::setprecision;
using std::setw;
using std::string;
using std::stringstream;
using std::to_string;



Arrow::Arrow(chess::Square from_in, chess::Square to_in): from(from_in), to(to_in) {
    sf::Vector2f from_coord = {100.0f + 100 * (int)from.file(), 820.0f - 100 * (int)from.rank()};
    sf::Vector2f to_coord = {100.0f + 100 * (int)to.file(), 820.0f - 100 * (int)to.rank()};

    float dx = to_coord.x - from_coord.x;
    float dy = from_coord.y - to_coord.y;
    float headlen = ARROWSIZE * 1.5f;
    float arrowlen = sqrt(dx * dx + dy * dy);
    auto arrowang = sf::radians(atan2(dx, dy));

    arrowbody.setSize({THICKNESS, arrowlen - headlen});
    arrowbody.setOrigin({0.5f * THICKNESS, arrowlen - headlen});
    arrowbody.setPosition(from_coord);
    arrowbody.setRotation(arrowang);
    arrowbody.setFillColor(PALETTE::TILE_SEL);

    arrowhead.setPointCount(3);
    arrowhead.setPoint(0, {0.0f, 0.0f});
    arrowhead.setPoint(1, {-0.9f * ARROWSIZE, headlen});
    arrowhead.setPoint(2, {0.9f * ARROWSIZE, headlen});
    arrowhead.setOrigin({0, arrowlen});
    arrowhead.setPosition(from_coord);
    arrowhead.setRotation(arrowang);
    arrowhead.setFillColor(PALETTE::TILE_SEL);
}

void Arrow::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    target.draw(arrowhead);
    target.draw(arrowbody);
}

bool Arrow::operator==(const Arrow& rhs) const { return ((to == rhs.to) && (from == rhs.from)); }



PieceDrawer::PieceDrawer(): textures(13) {
    const string TEXTURE[12] = {
        "wP",
        "wN",
        "wB",
        "wR",
        "wQ",
        "wK",
        "bP",
        "bN",
        "bB",
        "bR",
        "bQ",
        "bK",
    };
    bool loaded = true;
    for (int i = 0; i < 12; i++) {
        loaded &= textures[i].loadFromFile("src/assets/themes/tartiana/" + TEXTURE[i] + ".png");
        textures[i].setSmooth(true);
    }
    // check texture
    loaded &= textures[12].loadFromFile("src/assets/themes/check.png");
    textures[12].setSmooth(true);

    if (!loaded) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "Warning, could not load 1 or more texture files\n";
    }
    sprites.reserve(13);
    for (int i = 0; i < 13; i++) sprites.emplace_back(textures[i]);
}

void PieceDrawer::draw(sf::RenderWindow& window, chess::Piece piece, float x, float y, int check) {
    int i = (int)piece;
    assert((i != 12));
    // draw check overlay
    if ((piece == chess::Piece::WHITEKING && check == 1)
        || (piece == chess::Piece::BLACKKING && check == -1)) {
        sprites[12].setPosition({x, y});
        window.draw(sprites[12]);
    }
    // draw piece
    sprites[i].setPosition({x, y});
    window.draw(sprites[i]);
}



Timer::Timer(bool at_top, const sf::Font& font)
    : top(at_top), timertext(font, "", 40), timerbox({180.0f, 50.0f}) {
    timerbox.setPosition({660.0f, (top) ? 10.0f : 880.0f});
    timertext.setFillColor(PALETTE::TEXT);
}

void Timer::update(float time, bool active) {
    // accomodate different format
    stringstream formatter;

    if (time >= 60) {  // mm : ss
        timerbox.setFillColor((active) ? PALETTE::TIMER_A : PALETTE::TIMER);
        int min = (int)time / 60;
        int sec = (int)time % 60;
        formatter << setw(2) << setfill('0') << to_string(sec);
        t_disp = to_string(min) + " : " + formatter.str();
    } else {  // ss.mm
        timerbox.setFillColor((active) ? PALETTE::TIMER_ALOW : PALETTE::TIMER_LOW);
        formatter << fixed << setprecision(1) << time;
        t_disp = formatter.str();
    }

    // manage positioning of string
    timertext.setString(t_disp);
    auto textbounds = timertext.getLocalBounds();
    timertext.setPosition({820.0f - textbounds.size.x, (top) ? 10.0f : 880.0f});
}

void Timer::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    target.draw(timerbox);
    target.draw(timertext);
}



chess::Square cge::get_square(int x, int y) {
    int rank = (870 - y) / 100;
    int file = (x - 50) / 100;
    return chess::Square(chess::File(file), chess::Rank(rank));
}
