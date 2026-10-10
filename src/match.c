#include "global.h"
#include "sdata.h"
#include "server.h"
#include "physics.h"
#include "match.h"

// The score that wins at each Match length (0 = Endless)
static const int matchScores[MATCH_LENGTHS] = MATCH_SCORES;

int matchOver;
object_t matchWinner;
static int matchTicksLeft;

int matchSecondsLeft() {
  return (matchTicksLeft + ORB_TICKS(1) - 1) / ORB_TICKS(1);
}

// A fresh match: scores to zero, every ship back at full power somewhere
// safe, and the leftovers of the last match cleared away
static void matchStart() {
  int i;
  double xcoord, ycoord;
  struct dataNodeShips *ship;

  while (sdataCountMissiles > 0) sdataDeleteMissile(sdataArrayMissiles[0]->objectID);
  while (sdataCountOrbs > 0) orbRemove(0);
  while (sdataCountAsteroids > 0) sdataDeleteAsteroid(sdataArrayAsteroids[0]->objectID);

  for (i=0; i < sdataCountClients; i++) {
    sdataArrayClients[i]->score = 0;
    sdataArrayClients[i]->kills = 0;
    sdataArrayClients[i]->deaths = 0;
  }
  for (i=0; i < sdataCountShips; i++) {
    ship = sdataArrayShips[i];
    if (ship == NULL) continue;
    serverSpawnPoint(ship->objectID, &xcoord, &ycoord);
    ship->xcoord = xcoord;
    ship->ycoord = ycoord;
    ship->xVelocity = ship->yVelocity = 0.0;
    ship->status = 1;
    ship->energy = SHIP_MAX_ENERGY;
    ship->shields = SHIP_MAX_SHIELDS;
  }
  matchOver = 0;
  matchWinner = 0;
  scoreChanged = 1;
  serverEvent("A new match has begun");
}

void matchUpdate() {
  int i, target = matchScores[SERVER_OPTION(MATCH)];
  struct dataNodeClients *best = NULL;

  if (matchOver) {
    if (--matchTicksLeft <= 0) matchStart();
    // Resend the scoreboard each second, so the countdown stays current
    else if (matchTicksLeft % ORB_TICKS(1) == 0) scoreChanged = 1;
    return;
  }

  if (target == 0) return;
  for (i=0; i < sdataCountClients; i++) {
    if (sdataArrayClients[i]->score < target) continue;
    if (best == NULL || sdataArrayClients[i]->score > best->score) best = sdataArrayClients[i];
  }
  if (best == NULL) return;

  matchOver = 1;
  matchWinner = best->objectID;
  matchTicksLeft = ORB_TICKS(MATCH_RESULTS_TIME);
  scoreChanged = 1;
  serverEvent("%s won the match with %d points", best->username, best->score);
}
