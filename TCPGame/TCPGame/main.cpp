#include "network.h"
#include "game.h"

int main(void)
{
	Game* game = new Game;
	game->init();
	network_main(game);
	delete game;
}