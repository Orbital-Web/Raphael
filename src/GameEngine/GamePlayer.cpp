#include <GameEngine/GamePlayer.h>

using namespace cge;
using std::string;



GamePlayer::GamePlayer(const string& name_in): name(name_in) {}
GamePlayer::~GamePlayer() {}

// By default, does nothing
void GamePlayer::ponder(volatile bool&) {}
void GamePlayer::reset() {}
