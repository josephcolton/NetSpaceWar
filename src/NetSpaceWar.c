#include <SDL3/SDL_main.h> // Supplies the platform entry point (WinMain on Windows)

#include "global.h"
#include "cdata.h"
#include "client.h"
#include "gui.h"
#include "game.h"
#include "launcher.h"

/*
 * NetSpaceWar, the game. With no arguments it opens the menu, where the
 * player can host a game (with AI bots) or join one. With a server and name
 * on the command line it joins that game directly, as SpaceWarClient did.
 */
int main(int argc, char *argv[]) {
  int result;

  if (argc == 1) {
    launcherRun();
    SDL_Quit();
    return 0;
  }

  if (argc != 4) {
    printf("Usage:\n\t%s\n\t%s servername serverport username\n\n", argv[0], argv[0]);
    return 0;
  }

  result = gamePlay(argv[1], atoi(argv[2]), argv[3]);
  if (result == GAME_FAILED) printf("%s\n", clientStatusMessage);
  SDL_Quit();
  return result == GAME_FAILED ? 1 : 0;
}
