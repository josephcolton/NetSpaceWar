#include "global.h"
#include "discover.h"

static int discoverSocket = -1;
static double discoverStarted, discoverLastSent;
static struct netInterface interfaces[16];
static int interfaceCount;

// Broadcast the search. It goes out a few times while listening, in case
// one is lost.
static void discoverSend() {
  struct sockaddr_in to;
  struct msg_discover packet;
  int i;

  memset(&to, 0, sizeof(to));
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  to.sin_port = htons(DISCOVERY_PORT);
  packet.mtype = MSG_DISCOVER;
  sendto(discoverSocket, (const char *)&packet, sizeof(packet), 0, (struct sockaddr *)&to, sizeof(to));
  // Windows sends 255.255.255.255 out of only one network adapter, so also
  // send to each adapter's own broadcast address
  for (i=0; i < interfaceCount; i++) {
    to.sin_addr = interfaces[i].broadcast;
    sendto(discoverSocket, (const char *)&packet, sizeof(packet), 0, (struct sockaddr *)&to, sizeof(to));
  }
  discoverLastSent = platformNow();
}

void discoverStart() {
  int on = 1;
  discoverStop();
  if (netInitialize() != 0) return;
  discoverSocket = (int)socket(AF_INET, SOCK_DGRAM, 0);
  if (discoverSocket < 0) return;
  setsockopt(discoverSocket, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof(on));
  netIgnoreConnReset(discoverSocket);
  interfaceCount = netInterfaces(interfaces, 16);
  discoverStarted = platformNow();
  discoverSend();
}

void discoverStop() {
  if (discoverSocket >= 0) netCloseSocket(discoverSocket);
  discoverSocket = -1;
}

int discoverPoll(struct foundGame *games, int *count) {
  char buffer[256];
  struct sockaddr_in from;
  socklen_t fromLength;
  struct msg_serverinfo *info = (struct msg_serverinfo *)buffer;
  struct timeval wait = {0, 0};
  fd_set readfds;
  struct foundGame *game;
  int bytes, i;

  if (discoverSocket < 0) return 0;
  for (;;) {
    FD_ZERO(&readfds);
    FD_SET(discoverSocket, &readfds);
    if (select(discoverSocket + 1, &readfds, NULL, NULL, &wait) <= 0) break;
    fromLength = sizeof(from);
    bytes = recvfrom(discoverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&from, &fromLength);
    if (bytes < (int)sizeof(struct msg_serverinfo) || info->mtype != MSG_SERVERINFO) continue;

    // A game on this computer answers from each of its addresses; call
    // them all 127.0.0.1, so it is listed once
    for (i=0; i < interfaceCount; i++) {
      if (interfaces[i].address.s_addr == from.sin_addr.s_addr) from.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    // A server may answer more than one of our broadcasts
    for (i=0; i < *count; i++) {
      if (strcmp(games[i].address, inet_ntoa(from.sin_addr)) == 0 && games[i].port == info->port) break;
    }
    if (i == *count) {
      if (*count >= DISCOVER_MAX) continue;
      (*count)++;
    }
    game = &games[i];
    snprintf(game->address, sizeof(game->address), "%s", inet_ntoa(from.sin_addr));
    game->port = info->port;
    game->players = info->players;
    game->maxPlayers = info->maxPlayers;
    memcpy(game->version, info->version, VERSION_MAX);
    game->version[VERSION_MAX - 1] = 0;
    memcpy(game->host, info->host, USERNAME_MAX);
    game->host[USERNAME_MAX - 1] = 0;
    game->compatible = versionCompatible(game->version);
  }

  if (platformNow() - discoverStarted > DISCOVER_SECONDS) {
    discoverStop();
    return 0;
  }
  if (platformNow() - discoverLastSent > 0.4) discoverSend();
  return 1;
}
