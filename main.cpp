#include "src/GameEngine/GameEngine.h"
#include "src/GameEngine/HumanPlayer.h"
#include <array>



int main() {
    cge::GamePlayer* p1 = new cge::HumanPlayer("Adam");
    cge::GamePlayer* p2 = new cge::HumanPlayer("Bob");

    std::array<cge::GamePlayer*, 2> players = {p1, p2};
    cge::GameEngine ge(players);

    ge.run_match(true, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {600, 600}, true);
    ge.run_match(false,"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", {60, 60}, true);
    ge.print_report();
    delete p1;
    delete p2;
    return 0;
}