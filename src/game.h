#ifndef GAME_H
#define GAME_H

/* How a game ended */
#define GAME_LEFT    0 // The player pressed Escape
#define GAME_CLOSED  1 // The player closed the window
#define GAME_FAILED  2 // Could not connect, or lost the server; see clientStatusMessage

// Join the server at hostname:port as username and play until the game
// ends. Opens the game window if it is not already open.
int gamePlay(const char *hostname, int port, const char *username);

#endif
