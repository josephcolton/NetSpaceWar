#ifndef PHYSICS_H
#define PHYSICS_H

#include "global.h"
#include "sdata.h"
#include "server.h"

extern int restoreCount;

/* What hit a ship, for the death message */
#define HIT_PLANET   1
#define HIT_SHIP     2
#define HIT_MISSILE  3
#define HIT_ASTEROID 4
void noteHit(object_t ship, int cause, object_t by);
double randUnit(); // Random number in [0, 1)
void shieldRestore();
void physicsUpdatePositions();
void physicsUpdateCollisions();
void physicsUpdate();
// Pick the starting volcanoes and time their first eruptions (after the planets exist)
void volcanoInitialize();
void volcanoUpdate();
void volcanoToggle(int planet);  // Index in sdataArrayPlanets
void orbRemove(int i);           // Index in sdataArrayOrbs; restarts its volcano's wait
int inRange(double x1, double y1, double x2, double y2, double range);
double xcoordLoop(double xcoord);
double ycoordLoop(double ycoord);
double xcoordTowards(double x1, double y1, double x2, double y2);
double ycoordTowards(double x1, double y1, double x2, double y2);

#endif
