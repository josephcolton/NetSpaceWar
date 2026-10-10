#ifndef SDATA_H
#define SDATA_H

#include "global.h"

// Global variables
extern int serverSocket;
extern object_t sdataNextObjectID;
extern int sdataCountClients;
extern int sdataCountExplosions;
extern int sdataCountShips;
extern int sdataCountMissiles;
extern int sdataCountPlanets;

// Global general functions
object_t sdataNewObjectID();
void sdataInitialize();
double randRange(double minValue, double maxValue);

// Client Information
object_t sdataGetClientIDbyUsername(char *username);
object_t sdataGetClientIDbyAddress(struct sockaddr_in client_addr);

int sdataCreateClient(char *username, object_t objectID, struct sockaddr_in client_addr);

// Keep track of system clock
extern double lastTime;
int lastTimeLeft(int usec);

// We need an array for each type of data.
extern struct dataNodeClients *sdataArrayClients[MAX_COUNT_CLIENTS];
extern struct dataNodeExplosions *sdataArrayExplosions[MAX_COUNT_EXPLOSIONS];
extern struct dataNodeShips *sdataArrayShips[MAX_COUNT_SHIPS];
extern struct dataNodeMissiles *sdataArrayMissiles[MAX_COUNT_MISSILES];
extern struct dataNodePlanets *sdataArrayPlanets[MAX_COUNT_PLANETS];
extern int scoreChanged; // Set when the scoreboard needs sending

// Keep track of scores
void scoreAdd(object_t objectID, int score);
int scoreGet(object_t objectID);
void scoreCountKill(object_t objectID);
void scoreCountDeath(object_t objectID);

/* Server Data Access Functions */
// Existance Functions
int sdataExistsClient(object_t objectID);
// Creation Functions
int sdataCreateExplosion(double xcoord, double ycoord);
int sdataCreateShip(object_t objectID, double xcoord, double ycoord);
int sdataCreateMissile(object_t objectID, double xcoord, double ycoord, double orientation, double xVelocity, double yVelocity, object_t owner);
int sdataCreatePlanet(object_t objectID, double xcoord, double ycoord);
// Deletion Functions
void sdataDeleteClient(object_t objectID);
void sdataDeleteShip(object_t objectID);
void sdataDeleteMissile(object_t objectID);
void sdataDeletePlanet(object_t objectID);
// Update Functions
void sdataUpdateAll();
int sdataUpdateShip(object_t objectID, double xcoord, double ycoord);
int sdataUpdateMissile(object_t objectID, double xcoord, double ycoord);
int sdataUpdatePlanet(object_t objectID, double xcoord, double ycoord);
// Lookup Functions
struct dataNodeClients *sdataLookupClient(object_t objectID);
struct dataNodeShips *sdataLookupShips(object_t objectID);
struct dataNodeMissiles *sdataLookupMissiles(object_t objectID);
struct dataNodePlanets *sdataLookupPlanets(object_t objectID);

#endif
