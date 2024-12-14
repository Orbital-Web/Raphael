#include <GameEngine/UCIPlayer.h>

using namespace cge;



UCIPlayer::UCIPlayer(string name_in): GamePlayer(name_in) {
    load_uci_engine("src/Games/stockfish-windows-x86-64-avx2.exe");
}


chess::Move UCIPlayer::get_move(
    chess::Board board, const int t_remain, const int t_inc, sf::Event& event, bool& halt
) {
    return chess::Move::NO_MOVE;
}


void UCIPlayer::load_uci_engine(string uci_filepath) {}
