#ifndef AI_H
#define AI_H

#include "global.h"
#include "client.h"
#include "cdata.h"

/* AI tuning values, in internal units and seconds */
#define AI_CRUISE_SPEED      8.0  // Speed used to close on a target, below SHIP_MAX_VELOCITY
#define AI_VELOCITY_SLOP     2.0  // Velocity error tolerated before correcting course
#define AI_ENGAGE_RANGE      30.0 // Stop closing in and hold position inside this range
#define AI_FIRE_RANGE        60.0 // Do not fire at targets farther than this
#define AI_PLANET_DANGER     (P_GUI_DEFAULT_PLANET_SIZE + 15.0) // Steer away from planets inside this range
#define AI_PLANET_LOOKAHEAD  3.0  // Seconds ahead to project our course when checking planets
#define AI_THRUST_CONE       (M_PI / 8) // Only thrust when pointed within this angle of the wanted heading
#define AI_TARGET_SWITCH     0.8  // Switch targets only if a new one is this fraction of the distance
#define AI_ENERGY_RESERVE    4    // Energy kept back instead of spent on missiles
#define AI_ENERGY_LOW        30   // Move shields to energy below this energy...
#define AI_SHIELDS_SPARE     50   // ...if shields are above this
#define AI_SHIELDS_LOW       40   // Move energy to shields below this shield level...
#define AI_ENERGY_SPARE      40   // ...if energy is above this

// AI Client needs to select a target and work to eliminate it
extern object_t clientAItargetID;

// Functions for hunting and destroying targets
void clientAIinitAI();
object_t clientAIselectTarget();
double clientAIobjectDirection(object_t objectID);
void clientAIupdate(int mysocket, struct sockaddr_in server_addr);

#endif
