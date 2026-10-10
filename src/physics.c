#include "global.h"
#include "sdata.h"
#include "server.h"
#include "physics.h"

/* Global variable definitions (declared extern in physics.h) */
int restoreCount;

void deadShipDetect() {
  int i;
  double xcoord, ycoord;

  for(i=0; i < sdataCountShips; i++) {
    // We do not want empty nodes
    if (sdataArrayShips[i] == NULL) continue;
    if (sdataArrayShips[i]->status == 0) continue;
    // See if the ship should die
    if (sdataArrayShips[i]->shields < 0) {
      sdataArrayShips[i]->energy = 0;
      sdataArrayShips[i]->shields = 0;
      sdataArrayShips[i]->status = 0; // ship is marked as dead
      // Every death counts, whatever caused it; the owner of a fatal
      // missile already got credit for the kill
      scoreCountDeath(sdataArrayShips[i]->objectID);
      scoreAdd(sdataArrayShips[i]->objectID, POINTS_DIE);
      // Get the ship location
      xcoord = sdataArrayShips[i]->xcoord;
      ycoord = sdataArrayShips[i]->ycoord;
      // Create explosions
      sdataCreateExplosion(xcoord, ycoord);
      sdataCreateExplosion(xcoord + 2, ycoord);
      sdataCreateExplosion(xcoord - 2, ycoord);
      sdataCreateExplosion(xcoord, ycoord + 2);
      sdataCreateExplosion(xcoord, ycoord - 2);
    }
  }
}

void shieldRestore() {
  int i;

  restoreCount++;
  if (restoreCount > PHYSICS_PER_REST) {
    restoreCount = 0;
    // Give shields to ships
    for(i=0; i < sdataCountShips; i++) {
      // We do not want empty nodes
      if (sdataArrayShips[i] == NULL) continue;
      if (sdataArrayShips[i]->status == 0) continue;
      // Increment shields by SHIP_REST_SHIELD
      sdataArrayShips[i]->shields += SHIP_REST_SHIELD;
      // Make sure the ship does not have more shields than max
      if (sdataArrayShips[i]->shields > SHIP_MAX_SHIELDS) {
	sdataArrayShips[i]->shields = SHIP_MAX_SHIELDS;
      }
    }
  }
}

void physicsUpdatePositions() {
  int i;
  // Update ship positions
  for(i=0; i < sdataCountShips; i++) {
    // We do not want empty nodes
    if (sdataArrayShips[i] == NULL) continue;
    if (sdataArrayShips[i]->status == 0) continue;
    sdataArrayShips[i]->xcoord += sdataArrayShips[i]->xVelocity*PHYS_SECS_PER_UPDATE;
    sdataArrayShips[i]->ycoord += sdataArrayShips[i]->yVelocity*PHYS_SECS_PER_UPDATE;
    // Update for looping around the universe
    sdataArrayShips[i]->xcoord = xcoordLoop(sdataArrayShips[i]->xcoord);
    sdataArrayShips[i]->ycoord = ycoordLoop(sdataArrayShips[i]->ycoord);
  }

  // Update missile positions
  for(i=0; i < sdataCountMissiles; i++) {
    // We do not want empty nodes
    if (sdataArrayMissiles[i] == NULL) continue;
    sdataArrayMissiles[i]->xcoord += sdataArrayMissiles[i]->xVelocity*PHYS_SECS_PER_UPDATE;
    sdataArrayMissiles[i]->ycoord += sdataArrayMissiles[i]->yVelocity*PHYS_SECS_PER_UPDATE;
    // Update for looping around the universe
    sdataArrayMissiles[i]->xcoord = xcoordLoop(sdataArrayMissiles[i]->xcoord);
    sdataArrayMissiles[i]->ycoord = ycoordLoop(sdataArrayMissiles[i]->ycoord);
  }

}

double xcoordLoop(double xcoord) {
  if (xcoord > SPACE_MAX_XCOORD) {
    xcoord -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  }
  if (xcoord < SPACE_MIN_XCOORD) {
    xcoord += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  }
  return xcoord;
}

double ycoordLoop(double ycoord) {
  if (ycoord > SPACE_MAX_YCOORD) {
    ycoord -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
  }
  if (ycoord < SPACE_MIN_YCOORD) {
    ycoord += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
  }
  return ycoord;
}

double xcoordTowards(double x1, double y1, double x2, double y2) {
  double xDist = x2 - x1;
  double yDist = y2 - y1;

  if (xDist < SPACE_MIN_XCOORD) xDist += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  if (xDist > SPACE_MAX_XCOORD) xDist -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  if (yDist < SPACE_MIN_YCOORD) yDist += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
  if (yDist > SPACE_MAX_YCOORD) yDist -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);

  double distSquare = (xDist * xDist) + (yDist * yDist);
  return xDist / distSquare * PHYS_SECS_PER_UPDATE * GRAVITY;
}

double ycoordTowards(double x1, double y1, double x2, double y2) {
  double xDist = x2 - x1;
  double yDist = y2 - y1;

  if (xDist < SPACE_MIN_XCOORD) xDist += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  if (xDist > SPACE_MAX_XCOORD) xDist -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  if (yDist < SPACE_MIN_YCOORD) yDist += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
  if (yDist > SPACE_MAX_YCOORD) yDist -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);

  double distSquare = (xDist * xDist) + (yDist * yDist);
  return yDist / distSquare * PHYS_SECS_PER_UPDATE * GRAVITY;
}

int inRange(double x1, double y1, double x2, double y2, double range) {
  // See if the objects are in range.  In range is defined as
  // within range distance of each other.
  double rangeSquare = range * range;
  double xDist = x2 - x1;
  double yDist = y2 - y1;
  if (xDist < SPACE_MIN_XCOORD) xDist += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  if (xDist > SPACE_MAX_XCOORD) xDist -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
  if (yDist < SPACE_MIN_YCOORD) yDist += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
  if (yDist > SPACE_MAX_YCOORD) yDist -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
  double distSquare = (xDist * xDist) + (yDist * yDist);
  if (distSquare < rangeSquare) return 1;
  return 0;
}

void physicsUpdateCollisions() {
  int i, j;
  double x1, x2, y1, y2;
  double xTemp, yTemp;
  double range;
  object_t objectID;

  // Detect ship/planet collisions
  range = P_GUI_DEFAULT_PLANET_SIZE + P_GUI_SHIP_RADIUS_INTERNAL;
  for(i=0; i < sdataCountShips; i++) {
    // We do not want empty nodes
    if (sdataArrayShips[i] == NULL) continue;
    if (sdataArrayShips[i]->status == 0) continue;
    x1 = sdataArrayShips[i]->xcoord;
    y1 = sdataArrayShips[i]->ycoord;
    for (j=0; j < sdataCountPlanets; j++) {
      // Check the planets
      if (sdataArrayPlanets[j] == NULL) continue;
      x2 = sdataArrayPlanets[j]->xcoord;
      y2 = sdataArrayPlanets[j]->ycoord;
      if (inRange(x1, y1, x2, y2, range)) {
	// Take a bit of damage
	sdataArrayShips[i]->shields -= (COLLISION_DMG * 5);
	if (sdataArrayShips[i]->shields>=0)
	{
	  // The ship is in range of the planet, so switch direction
	  sdataArrayShips[i]->xVelocity = 0 - sdataArrayShips[i]->xVelocity;
	  sdataArrayShips[i]->yVelocity = 0 - sdataArrayShips[i]->yVelocity;
	  // Bounce
	  sdataArrayShips[i]->xcoord += sdataArrayShips[i]->xVelocity*PHYS_SECS_PER_UPDATE;
	  sdataArrayShips[i]->ycoord += sdataArrayShips[i]->yVelocity*PHYS_SECS_PER_UPDATE;
	  // Update for looping around the universe
	  sdataArrayShips[i]->xcoord = xcoordLoop(sdataArrayShips[i]->xcoord);
	  sdataArrayShips[i]->ycoord = ycoordLoop(sdataArrayShips[i]->ycoord);
	}
      }
    }
  }

  // Detect ship/ship collisions
  range = P_GUI_SHIP_RADIUS_INTERNAL + P_GUI_SHIP_RADIUS_INTERNAL;
  for(i=0; i < sdataCountShips; i++) {
    // We do not want empty nodes
    if (sdataArrayShips[i] == NULL) continue;
    if (sdataArrayShips[i]->status == 0) continue;
    x1 = sdataArrayShips[i]->xcoord;
    y1 = sdataArrayShips[i]->ycoord;
    for (j=i + 1; j < sdataCountShips; j++) {
      // Check the ships
      if (sdataArrayShips[j] == NULL) continue;
      if (sdataArrayShips[j]->status == 0) continue;
      x2 = sdataArrayShips[j]->xcoord;
      y2 = sdataArrayShips[j]->ycoord;
      if (inRange(x1, y1, x2, y2, range)) {
	// Both take a bit of damage
	sdataArrayShips[i]->shields -= COLLISION_DMG;
	sdataArrayShips[j]->shields -= COLLISION_DMG;
	// Trade directions
	xTemp = sdataArrayShips[i]->xVelocity;
	yTemp = sdataArrayShips[i]->yVelocity;
	sdataArrayShips[i]->xVelocity = sdataArrayShips[j]->xVelocity;
	sdataArrayShips[i]->yVelocity = sdataArrayShips[j]->yVelocity;
	sdataArrayShips[j]->xVelocity = xTemp;
	sdataArrayShips[j]->yVelocity = yTemp;
	// Bounce
	sdataArrayShips[i]->xcoord += sdataArrayShips[i]->xVelocity*PHYS_SECS_PER_UPDATE;
	sdataArrayShips[i]->ycoord += sdataArrayShips[i]->yVelocity*PHYS_SECS_PER_UPDATE;
	sdataArrayShips[j]->xcoord += sdataArrayShips[j]->xVelocity*PHYS_SECS_PER_UPDATE;
	sdataArrayShips[j]->ycoord += sdataArrayShips[j]->yVelocity*PHYS_SECS_PER_UPDATE;
	// Update for looping around the universe
	sdataArrayShips[i]->xcoord = xcoordLoop(sdataArrayShips[i]->xcoord);
	sdataArrayShips[i]->ycoord = ycoordLoop(sdataArrayShips[i]->ycoord);
	sdataArrayShips[j]->xcoord = xcoordLoop(sdataArrayShips[j]->xcoord);
	sdataArrayShips[j]->ycoord = ycoordLoop(sdataArrayShips[j]->ycoord);
      }
    }
  }

  // Detect ship/missile collisions
  range = P_GUI_SHIP_RADIUS_INTERNAL + P_GUI_SHOT_RADIUS_INTERNAL;
  for(i=0; i < sdataCountShips; i++) {
    // We do not want empty nodes
    if (sdataArrayShips[i] == NULL) continue;
    if (sdataArrayShips[i]->status == 0) continue;
    x1 = sdataArrayShips[i]->xcoord;
    y1 = sdataArrayShips[i]->ycoord;
    for (j=0; j < sdataCountMissiles; j++) {
      // Check the missiles
      if (sdataArrayMissiles[j] == NULL) continue;
      x2 = sdataArrayMissiles[j]->xcoord;
      y2 = sdataArrayMissiles[j]->ycoord;
      if (inRange(x1, y1, x2, y2, range)) {
	// Take missile damage
	sdataArrayShips[i]->shields -= COLLISION_EXP;
	// Missile owner gets points, but not for hitting themselves
	if (sdataArrayMissiles[j]->owner != sdataArrayShips[i]->objectID) {
	  if (sdataArrayShips[i]->shields < 0) {
	    // Points for killing the ship
	    scoreAdd(sdataArrayMissiles[j]->owner, POINTS_KILL);
	    scoreCountKill(sdataArrayMissiles[j]->owner);
	  } else {
	    // Points for a regular hit
	    scoreAdd(sdataArrayMissiles[j]->owner, POINTS_HIT);
	  }
	}
	// Missile explodes
	objectID = sdataArrayMissiles[j]->objectID;
	sdataCreateExplosion(x2, y2);
	// Delete the missile
	broadcastDeleteMissile(objectID);
	sdataDeleteMissile(objectID);
      }
    }
  }

  // Detect missile/missile collisions
  range = P_GUI_SHOT_RADIUS_INTERNAL + P_GUI_SHOT_RADIUS_INTERNAL;
  for(i=0; i < sdataCountMissiles; i++) {
    // We do not want empty nodes
    if (sdataArrayMissiles[i] == NULL) continue;
    x1 = sdataArrayMissiles[i]->xcoord;
    y1 = sdataArrayMissiles[i]->ycoord;
    for (j=i+1; j < sdataCountMissiles; j++) {
      // Check the missiles
      if (sdataArrayMissiles[j] == NULL) continue;
      x2 = sdataArrayMissiles[j]->xcoord;
      y2 = sdataArrayMissiles[j]->ycoord;
      if (inRange(x1, y1, x2, y2, range)) {
	// Missile j explodes first
	objectID = sdataArrayMissiles[j]->objectID;
	sdataCreateExplosion(x2, y2);
	// Delete the missile
	broadcastDeleteMissile(objectID);
	sdataDeleteMissile(objectID);

	// Missile i explodes second
	objectID = sdataArrayMissiles[i]->objectID;
	sdataCreateExplosion(x1, y1);
	// Delete the missile
	broadcastDeleteMissile(objectID);
	sdataDeleteMissile(objectID);

	// Need to break
	i--;
	break;
      }
    }
  }

  // Detect missile/planet collisions
  range = P_GUI_DEFAULT_PLANET_SIZE + P_GUI_SHOT_RADIUS_INTERNAL;
  for(i=0; i < sdataCountPlanets; i++) {
    // We do not want empty nodes
    if (sdataArrayPlanets[i] == NULL) continue;
    x1 = sdataArrayPlanets[i]->xcoord;
    y1 = sdataArrayPlanets[i]->ycoord;
    for (j=0; j < sdataCountMissiles; j++) {
      // Check the missiles
      if (sdataArrayMissiles[j] == NULL) continue;
      x2 = sdataArrayMissiles[j]->xcoord;
      y2 = sdataArrayMissiles[j]->ycoord;
      if (inRange(x1, y1, x2, y2, range)) {
	// Missile explodes
	objectID = sdataArrayMissiles[j]->objectID;
	sdataCreateExplosion(x2, y2);
	// Delete the missile
	broadcastDeleteMissile(objectID);
	sdataDeleteMissile(objectID);
      }
    }
  }

}

void physicsUpdateGravity() {
  int i, j;
  double x1, x2, y1, y2;
  double range;

  range = P_GUI_DEFAULT_PLANET_SIZE * P_GUI_DEFAULT_PLANET_SIZE * 2;
  // Detect planets pull on ships
  for(i=0; i < sdataCountShips; i++) {
    // We do not want empty nodes
    if (sdataArrayShips[i] == NULL) continue;
    if (sdataArrayShips[i]->status == 0) continue;
    x1 = sdataArrayShips[i]->xcoord;
    y1 = sdataArrayShips[i]->ycoord;
    for (j=0; j < sdataCountPlanets; j++) {
      // Check the planets
      if (sdataArrayPlanets[j] == NULL) continue;
      x2 = sdataArrayPlanets[j]->xcoord;
      y2 = sdataArrayPlanets[j]->ycoord;
      if (inRange(x1, y1, x2, y2, range)) {
	// The ship is in range of the planet, pull him in
	sdataArrayShips[i]->xVelocity += xcoordTowards(x1, y1, x2, y2);
	sdataArrayShips[i]->yVelocity += ycoordTowards(x1, y1, x2, y2);
      }
    }
  }

  range = P_GUI_DEFAULT_PLANET_SIZE * P_GUI_DEFAULT_PLANET_SIZE * 2;
  // Detect planets pull on missiles
  for(i=0; i < sdataCountMissiles; i++) {
    // We do not want empty nodes
    if (sdataArrayMissiles[i] == NULL) continue;
    x1 = sdataArrayMissiles[i]->xcoord;
    y1 = sdataArrayMissiles[i]->ycoord;
    for (j=0; j < sdataCountPlanets; j++) {
      // Check the planets
      if (sdataArrayPlanets[j] == NULL) continue;
      x2 = sdataArrayPlanets[j]->xcoord;
      y2 = sdataArrayPlanets[j]->ycoord;
      if (inRange(x1, y1, x2, y2, range)) {
	// The ship is in range of the planet, pull him in
	sdataArrayMissiles[i]->xVelocity += xcoordTowards(x1, y1, x2, y2);
	sdataArrayMissiles[i]->yVelocity += ycoordTowards(x1, y1, x2, y2);
      }
    }
  }

}

void physicsUpdate() {
  shieldRestore();
  physicsUpdateGravity();
  physicsUpdatePositions();
  physicsUpdateCollisions();
  // Must run after collisions: if a ship were left with negative shields
  // until the next tick, a power transfer or shield restore in between
  // would clamp them back to 0 and the ship would survive.
  deadShipDetect();
}
