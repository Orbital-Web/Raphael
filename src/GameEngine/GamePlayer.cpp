#include <GameEngine/GamePlayer.h>

using namespace cge;


GamePlayer::GamePlayer(string name_in): name(name_in) {}
GamePlayer::~GamePlayer() {}

// By default, does nothing
void GamePlayer::ponder(chess::Board board, bool& halt) {}
void GamePlayer::reset() {}
