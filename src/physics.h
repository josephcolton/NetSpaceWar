#ifndef PHYSICS_H
#define PHYSICS_H

#include "global.h"
#include "sdata.h"
#include "server.h"

extern int restoreCount;
void shieldRestore();
void physicsUpdatePositions();
void physicsUpdateCollisions();
void physicsUpdate();
int inRange(double x1, double y1, double x2, double y2, double range);
double xcoordLoop(double xcoord);
double ycoordLoop(double ycoord);
double xcoordTowards(double x1, double y1, double x2, double y2);
double ycoordTowards(double x1, double y1, double x2, double y2);

#endif
