#ifndef CDATA_H
#define CDATA_H

#include "global.h"
#include "options.h"

// Client Information
extern int clientAlive;
extern object_t clientObjectID;
extern struct dataNodeClients *clientNode;
extern struct dataNodeShips *clientShip;
extern char clientUsername[USERNAME_MAX];
extern time_t clientCheckin;
// Why the client stopped (login refused, server unreachable...), for the player
extern char clientStatusMessage[LEN];

// Variables to keep track of the number of each object
extern int cdataCountClients;
extern int cdataCountExplosions;
extern int cdataCountShips;
extern int cdataCountMissiles;
extern int cdataCountPlanets;

// The latest scoreboard from the server, in the server's order
extern struct wireScore cdataScores[MAX_COUNT_CLIENTS];
extern int cdataCountScores;

// The server's game rules: defaults until its msg_gameoptions arrives
extern int cdataOptions[OPTION_COUNT];
extern int cdataOptionsReceived;
#define CLIENT_OPTION(x) (cdataOptions[OPTION_##x])

// We need an array for each type of data.
extern struct dataNodeClients *cdataArrayClients[MAX_COUNT_CLIENTS];
extern struct dataNodeExplosions *cdataArrayExplosions[MAX_COUNT_EXPLOSIONS];
extern struct dataNodeShips *cdataArrayShips[MAX_COUNT_SHIPS];
extern struct dataNodeMissiles *cdataArrayMissiles[MAX_COUNT_MISSILES];
extern struct dataNodePlanets *cdataArrayPlanets[MAX_COUNT_PLANETS];

/* Client Functions */
void cdataInitialize();

// Existance Functions
int cdataExistsClient(object_t objectID);

// Creation Functions
int cdataCreateClient(struct sockaddr_in *client_addr, char *username, object_t objectID);
int cdataCreateExplosion(double xcoord, double ycoord);
int cdataCreateShip(object_t objectID, double xcoord, double ycoord);
int cdataCreateMissile(object_t objectID, double xcoord, double ycoord, double orientation, double xVelocity, double yVelocity);
int cdataCreatePlanet(object_t objectID, double xcoord, double ycoord);
// Deletion Functions
void cdataDeleteShip(object_t objectID);
void cdataDeleteMissile(object_t objectID);
void cdataDeletePlanet(object_t objectID);
void cdataDeleteExplosion(int pos); // Explosions don't have an objectID so this is just an array position
// Update Functions
int cdataUpdateShip(object_t objectID, double xcoord, double ycoord);
int cdataUpdateMissile(object_t objectID, double xcoord, double ycoord);
int cdataUpdatePlanet(object_t objectID, double xcoord, double ycoord);
void cdataUpdateAll();
// Lookup Functions
struct dataNodeClients *cdataLookupClient(object_t objectID);
struct dataNodeShips *cdataLookupShips(object_t objectID);
struct dataNodeMissiles *cdataLookupMissiles(object_t objectID);
struct dataNodePlanets *cdataLookupPlanets(object_t objectID);

#endif
