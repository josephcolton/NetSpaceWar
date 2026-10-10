#ifndef ASTEROIDS_H
#define ASTEROIDS_H

#include "global.h"

/* Asteroids (server only): they drift in from outside everyone's view,
   pull lightly on ships, missiles and orbs, and can be shot or rammed. */
void asteroidsInitialize();
// Spawn, move, collide and remove asteroids. Runs once per physics tick,
// after the other collisions and before dead ships are detected.
void asteroidsUpdate();
// The change in velocity this tick from the asteroids' pull on something at (xcoord, ycoord)
void asteroidGravity(double xcoord, double ycoord, double *xChange, double *yChange);

#endif
