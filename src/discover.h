#ifndef DISCOVER_H
#define DISCOVER_H

#include "global.h"

/* Finding games on the local network (the launcher's Join screen): a
   search is broadcast to DISCOVERY_PORT, and servers listening on every
   interface answer with a msg_serverinfo. */

#define DISCOVER_MAX     6   // Games listed at once
#define DISCOVER_SECONDS 1.5 // How long a search listens for answers

struct foundGame {
  char address[64];          // Where the answer came from
  int port;
  int players, maxPlayers;
  char version[VERSION_MAX];
  char host[USERNAME_MAX];   // Empty for a standalone server
  int compatible;            // Same major.minor version as us
};

// Start a search, replacing any earlier one
void discoverStart();
// Collect answers that have arrived into games/count. Returns 1 while the
// search is still listening, 0 once it is over.
int discoverPoll(struct foundGame *games, int *count);
void discoverStop();

#endif
