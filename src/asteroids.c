#include "global.h"
#include "sdata.h"
#include "server.h"
#include "physics.h"
#include "asteroids.h"

// Seconds between asteroids for each Asteroids setting (0 = none)
static const int asteroidInterval[ASTEROID_LEVELS] = ASTEROID_INTERVALS;
static int asteroidWait; // Physics ticks until the next asteroid

// Shortest signed distance from a to b along an axis of the looping universe
static double wrapDelta(double a, double b) {
  double span = SPACE_MAX_XCOORD - SPACE_MIN_XCOORD;
  double d = b - a;
  if (d > span / 2) d -= span;
  if (d < -span / 2) d += span;
  return d;
}

// Could any player see something of this radius here? A dead player's view
// stays where their ship was.
static int asteroidVisible(double xcoord, double ycoord, double radius) {
  int i;
  for (i=0; i < sdataCountShips; i++) {
    if (sdataArrayShips[i] == NULL) continue;
    if (fabs(wrapDelta(sdataArrayShips[i]->xcoord, xcoord)) < VIEW_HALF_W + radius &&
        fabs(wrapDelta(sdataArrayShips[i]->ycoord, ycoord)) < VIEW_HALF_H + radius) return 1;
  }
  return 0;
}

static int asteroidNextWait() {
  int interval = asteroidInterval[SERVER_OPTION(ASTEROIDS)];
  return ORB_TICKS(interval * (0.75 + 0.5 * randUnit()));
}

void asteroidsInitialize() {
  // The first one comes a little sooner than the rest
  asteroidWait = asteroidNextWait() / 2;
}

// Start an asteroid just out of everyone's sight, clear of the planets,
// heading roughly for a random ship so it drifts into someone's view.
// Returns 0 if no good spot turned up this time.
static int asteroidSpawn() {
  int tries, j;
  double xcoord, ycoord, angle, speed;
  struct dataNodeShips *ship;

  for (tries=0; tries < 30; tries++) {
    xcoord = SPACE_MIN_XCOORD + randUnit() * (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    ycoord = SPACE_MIN_YCOORD + randUnit() * (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    if (asteroidVisible(xcoord, ycoord, ASTEROID_RADIUS + 2)) continue;
    for (j=0; j < sdataCountPlanets; j++) {
      if (inRange(xcoord, ycoord, sdataArrayPlanets[j]->xcoord, sdataArrayPlanets[j]->ycoord,
                  P_GUI_DEFAULT_PLANET_SIZE + ASTEROID_RADIUS + 8)) break;
    }
    if (j < sdataCountPlanets) continue;

    ship = sdataArrayShips[rand() % sdataCountShips];
    if (ship == NULL) angle = randUnit() * 2 * M_PI;
    else angle = atan2(wrapDelta(ycoord, ship->ycoord), wrapDelta(xcoord, ship->xcoord)) + (randUnit() - 0.5) * 0.6;
    speed = ASTEROID_SPEED_MIN + (ASTEROID_SPEED_MAX - ASTEROID_SPEED_MIN) * randUnit();
    return sdataCreateAsteroid(sdataNewObjectID(), xcoord, ycoord, cos(angle) * speed, sin(angle) * speed,
                               ORB_TICKS(ASTEROID_TRIP / speed));
  }
  return 0;
}

static void asteroidExplode(struct dataNodeAsteroids *asteroid) {
  sdataCreateExplosion(asteroid->xcoord, asteroid->ycoord);
  sdataCreateExplosion(xcoordLoop(asteroid->xcoord + ASTEROID_RADIUS), asteroid->ycoord);
  sdataCreateExplosion(xcoordLoop(asteroid->xcoord - ASTEROID_RADIUS), asteroid->ycoord);
  sdataCreateExplosion(asteroid->xcoord, ycoordLoop(asteroid->ycoord + ASTEROID_RADIUS));
  sdataCreateExplosion(asteroid->xcoord, ycoordLoop(asteroid->ycoord - ASTEROID_RADIUS));
}

// A ship hit the asteroid: both take planet-strength crash damage and the
// ship bounces off. Returns 1 if that destroyed the asteroid.
static int asteroidRam(struct dataNodeAsteroids *asteroid, struct dataNodeShips *ship) {
  int damage = SERVER_OPTION(CRASH_DMG) * 5;
  double nx = wrapDelta(asteroid->xcoord, ship->xcoord);
  double ny = wrapDelta(asteroid->ycoord, ship->ycoord);
  double dist = sqrt(nx * nx + ny * ny);
  double closing;

  ship->shields -= damage;
  asteroid->hp -= damage;
  noteHit(ship->objectID, HIT_ASTEROID, 0);

  // Reflect the ship's velocity (relative to the asteroid) off the point of
  // contact, then move it clear so it isn't hit again next tick
  if (dist < 0.001) { nx = 1; ny = 0; dist = 1; }
  nx /= dist;
  ny /= dist;
  closing = (ship->xVelocity - asteroid->xVelocity) * nx + (ship->yVelocity - asteroid->yVelocity) * ny;
  if (closing < 0) {
    ship->xVelocity -= 2 * closing * nx;
    ship->yVelocity -= 2 * closing * ny;
  }
  ship->xcoord = xcoordLoop(asteroid->xcoord + nx * (ASTEROID_RADIUS + P_GUI_SHIP_RADIUS_INTERNAL + 0.1));
  ship->ycoord = ycoordLoop(asteroid->ycoord + ny * (ASTEROID_RADIUS + P_GUI_SHIP_RADIUS_INTERNAL + 0.1));

  if (asteroid->hp > 0) return 0;
  // Ramming it to pieces scores like shooting it, even if the ship dies too
  scoreAdd(ship->objectID, POINTS_ASTEROID_KILL);
  serverLog("%s rammed an asteroid to pieces", serverClientName(ship->objectID));
  return 1;
}

// Missiles that hit the asteroid explode and damage it. Returns 1 if it was destroyed.
static int asteroidShot(struct dataNodeAsteroids *asteroid) {
  int k;
  object_t owner;
  struct dataNodeMissiles *missile;

  for (k=0; k < sdataCountMissiles; k++) {
    missile = sdataArrayMissiles[k];
    if (missile == NULL) continue;
    if (!inRange(missile->xcoord, missile->ycoord, asteroid->xcoord, asteroid->ycoord,
                 P_GUI_SHOT_RADIUS_INTERNAL + ASTEROID_RADIUS)) continue;
    owner = missile->owner;
    sdataCreateExplosion(missile->xcoord, missile->ycoord);
    sdataDeleteMissile(missile->objectID); // Also tells the clients
    k--; // The last missile moved into this slot

    asteroid->hp -= SERVER_OPTION(MISSILE_DMG);
    if (asteroid->hp > 0) {
      scoreAdd(owner, POINTS_ASTEROID_HIT);
      continue;
    }
    scoreAdd(owner, POINTS_ASTEROID_KILL);
    serverLog("%s destroyed an asteroid", serverClientName(owner));
    return 1;
  }
  return 0;
}

void asteroidsUpdate() {
  int i, j, destroyed;
  struct dataNodeAsteroids *asteroid;
  struct dataNodeShips *ship;

  // A new one every so often, while someone is playing
  if (SERVER_OPTION(ASTEROIDS) > 0 && sdataCountShips > 0 && sdataCountAsteroids < MAX_COUNT_ASTEROIDS) {
    if (asteroidWait > 0) asteroidWait--;
    else if (asteroidSpawn()) asteroidWait = asteroidNextWait();
  }

  for (i=0; i < sdataCountAsteroids; i++) {
    asteroid = sdataArrayAsteroids[i];
    destroyed = 0;

    // Asteroids drift in straight lines; nothing pulls on them
    asteroid->xcoord = xcoordLoop(asteroid->xcoord + asteroid->xVelocity * PHYS_SECS_PER_UPDATE);
    asteroid->ycoord = ycoordLoop(asteroid->ycoord + asteroid->yVelocity * PHYS_SECS_PER_UPDATE);

    // After its trip it leaves, but only where nobody sees it vanish
    if (asteroid->life > 0) asteroid->life--;
    if (asteroid->life <= 0 && !asteroidVisible(asteroid->xcoord, asteroid->ycoord, ASTEROID_RADIUS)) {
      sdataDeleteAsteroid(asteroid->objectID);
      i--;
      continue;
    }

    // Crashing into a planet destroys it, and may wake or put out the planet's volcano
    for (j=0; j < sdataCountPlanets; j++) {
      if (!inRange(asteroid->xcoord, asteroid->ycoord, sdataArrayPlanets[j]->xcoord, sdataArrayPlanets[j]->ycoord,
                   P_GUI_DEFAULT_PLANET_SIZE + ASTEROID_RADIUS)) continue;
      serverLog("An asteroid crashed into a planet");
      if (randUnit() < ASTEROID_TOGGLE_CHANCE) volcanoToggle(j);
      destroyed = 1;
      break;
    }

    // It destroys recharge orbs it runs into
    for (j=0; j < sdataCountOrbs && !destroyed; j++) {
      if (!inRange(asteroid->xcoord, asteroid->ycoord, sdataArrayOrbs[j]->xcoord, sdataArrayOrbs[j]->ycoord,
                   ASTEROID_RADIUS + ORB_RADIUS)) continue;
      sdataCreateExplosion(sdataArrayOrbs[j]->xcoord, sdataArrayOrbs[j]->ycoord);
      orbRemove(j);
      j--;
    }

    if (!destroyed) destroyed = asteroidShot(asteroid);

    for (j=0; j < sdataCountShips && !destroyed; j++) {
      ship = sdataArrayShips[j];
      if (ship == NULL || ship->status == 0) continue;
      if (inRange(ship->xcoord, ship->ycoord, asteroid->xcoord, asteroid->ycoord,
                  P_GUI_SHIP_RADIUS_INTERNAL + ASTEROID_RADIUS)) {
        destroyed = asteroidRam(asteroid, ship);
      }
    }

    if (destroyed) {
      asteroidExplode(asteroid);
      sdataDeleteAsteroid(asteroid->objectID);
      i--;
    }
  }
}

void asteroidGravity(double xcoord, double ycoord, double *xChange, double *yChange) {
  int i;
  *xChange = *yChange = 0.0;
  for (i=0; i < sdataCountAsteroids; i++) {
    double x2 = sdataArrayAsteroids[i]->xcoord, y2 = sdataArrayAsteroids[i]->ycoord;
    if (!inRange(xcoord, ycoord, x2, y2, ASTEROID_GRAVITY_RANGE)) continue;
    *xChange += xcoordTowards(xcoord, ycoord, x2, y2) * ASTEROID_GRAVITY;
    *yChange += ycoordTowards(xcoord, ycoord, x2, y2) * ASTEROID_GRAVITY;
  }
}
