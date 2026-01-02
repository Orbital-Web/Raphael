#include <GameEngine/UCIPlayer.h>

using namespace cge;
using std::string;



UCIPlayer::UCIPlayer(string name_in): GamePlayer(name_in) {
    load_uci_engine("src/Games/stockfish-windows-x86-64-avx2.exe");
}


UCIPlayer::MoveEval UCIPlayer::get_move(
    chess::Board board,
    const int t_remain,
    const int t_inc,
    volatile MouseInfo& mouse,
    volatile bool& halt
) {
    return {chess::Move::NO_MOVE, 0};
}


void UCIPlayer::load_uci_engine(string uci_filepath) {}
