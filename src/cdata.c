#include "global.h"
#include "cdata.h"

/* Global variable definitions (declared extern in cdata.h) */
int clientAlive;
object_t clientObjectID;
struct dataNodeClients *clientNode;
struct dataNodeShips *clientShip;
char clientUsername[USERNAME_MAX];
time_t clientCheckin;
char clientStatusMessage[LEN];
int cdataCountClients;
int cdataCountExplosions;
int cdataCountShips;
int cdataCountMissiles;
int cdataCountPlanets;
struct dataNodeOrbs cdataOrbs[MAX_COUNT_ORBS];
int cdataCountOrbs;
struct dataNodeAsteroids cdataAsteroids[MAX_COUNT_ASTEROIDS];
int cdataCountAsteroids;
double cdataPlanetChanged[MAX_COUNT_PLANETS];
void (*clientSoundHook)(int sound, double xcoord, double ycoord);
struct clientEvent cdataEvents[CLIENT_EVENTS];
int cdataCountEvents;

void cdataAddEvent(const char *text) {
  if (cdataCountEvents == CLIENT_EVENTS) {
    memmove(&cdataEvents[0], &cdataEvents[1], sizeof(struct clientEvent) * (CLIENT_EVENTS - 1));
    cdataCountEvents--;
  }
  strncpy(cdataEvents[cdataCountEvents].text, text, EVENT_TEXT_MAX - 1);
  cdataEvents[cdataCountEvents].text[EVENT_TEXT_MAX - 1] = 0;
  cdataEvents[cdataCountEvents].time = platformNow();
  cdataCountEvents++;
}
struct wireScore cdataScores[MAX_COUNT_CLIENTS];
int cdataCountScores;
int cdataMatchOver;
object_t cdataMatchWinner;
int cdataNextMatch;
int cdataOptions[OPTION_COUNT];
int cdataOptionsReceived;
struct dataNodeClients *cdataArrayClients[MAX_COUNT_CLIENTS];
struct dataNodeExplosions *cdataArrayExplosions[MAX_COUNT_EXPLOSIONS];
struct dataNodeShips *cdataArrayShips[MAX_COUNT_SHIPS];
struct dataNodeMissiles *cdataArrayMissiles[MAX_COUNT_MISSILES];
struct dataNodePlanets *cdataArrayPlanets[MAX_COUNT_PLANETS];

/* Client Data Initialization */
void cdataInitialize() {
  int i;

  // Free objects left over from a previous game
  for (i=0; i < MAX_COUNT_CLIENTS; i++) free(cdataArrayClients[i]);
  for (i=0; i < MAX_COUNT_EXPLOSIONS; i++) free(cdataArrayExplosions[i]);
  for (i=0; i < MAX_COUNT_SHIPS; i++) free(cdataArrayShips[i]);
  for (i=0; i < MAX_COUNT_MISSILES; i++) free(cdataArrayMissiles[i]);
  for (i=0; i < MAX_COUNT_PLANETS; i++) free(cdataArrayPlanets[i]);

  // Client Information
  clientObjectID = 0;
  clientStatusMessage[0] = 0;
  clientNode = NULL;
  clientUsername[0] = 0;
  clientAlive = 1;

  // Initialize all of the arrays
  for (i=0; i < MAX_COUNT_CLIENTS; i++) cdataArrayClients[i] = NULL;
  for (i=0; i < MAX_COUNT_EXPLOSIONS; i++) cdataArrayExplosions[i] = NULL;
  for (i=0; i < MAX_COUNT_SHIPS; i++) cdataArrayShips[i] = NULL;
  for (i=0; i < MAX_COUNT_MISSILES; i++) cdataArrayMissiles[i] = NULL;
  for (i=0; i < MAX_COUNT_PLANETS; i++) cdataArrayPlanets[i] = NULL;

  // Initialize all of the object counts
  cdataCountClients = 0;
  cdataCountExplosions = 0;
  cdataCountShips = 0;
  cdataCountMissiles = 0;
  cdataCountPlanets = 0;
  cdataCountOrbs = 0;
  cdataCountAsteroids = 0;
  cdataCountEvents = 0;
  for (i=0; i < MAX_COUNT_PLANETS; i++) cdataPlanetChanged[i] = 0.0;
  cdataCountScores = 0;
  cdataMatchOver = 0;
  cdataMatchWinner = 0;
  cdataNextMatch = 0;

  // Rules are the defaults until the server says otherwise
  gameOptionsReset(cdataOptions);
  cdataOptionsReceived = 0;

  // Initialize the checkin
  clientCheckin = 0;

  clientShip=NULL;
}

/* Client Data Access Functions */
// Existance Functions
int cdataExistsClient(object_t objectID) {
  int i;

  for(i=0; i < cdataCountClients; i++) {
    if (cdataArrayClients[i] == NULL) continue;
    if (cdataArrayClients[i]->objectID == objectID) {
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

// Creation Functions
int cdataCreateClient(struct sockaddr_in *client_addr, char *username, object_t objectID) {
  // Returns SW_FALSE if there is no room in the array;
  if (cdataCountClients >= MAX_COUNT_CLIENTS) {
    printf("  ** Too many clients\n");
    return SW_FALSE;
  }

  // Make sure the client does not already exist
  if (cdataExistsClient(objectID)) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (cdataArrayClients[cdataCountClients] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodeClients *newNode;
  newNode = (struct dataNodeClients *)malloc(sizeof(struct dataNodeClients));
  cdataArrayClients[cdataCountClients] = newNode;

  // See if this is the clientNode
  if (objectID == clientObjectID) {
    clientNode = newNode;
  }

  // Fill in the data values
  strncpy(newNode->username, username, USERNAME_MAX);
  newNode->client_addr = client_addr;
  newNode->objectID = objectID;

  // Increment the count
  cdataCountClients++;

  // Return true
  return SW_TRUE;
}

int cdataCreateExplosion(double xcoord, double ycoord) {
  // Returns SW_FALSE if there is no room in the array;
  if (cdataCountExplosions >= MAX_COUNT_EXPLOSIONS) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (cdataArrayExplosions[cdataCountExplosions] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodeExplosions *newNode;
  newNode = (struct dataNodeExplosions *)malloc(sizeof(struct dataNodeExplosions));
  cdataArrayExplosions[cdataCountExplosions] = newNode;

  // Fill in the data values
  newNode->xcoord = xcoord;
  newNode->ycoord = ycoord;
  newNode->timeToLive=EXPLOSION_TTL;

  // Increment the count
  cdataCountExplosions++;

  // Return true
  return SW_TRUE;
}

void cdataDeleteExplosion(int pos)
{
  free(cdataArrayExplosions[pos]);
  cdataArrayExplosions[pos]=cdataArrayExplosions[--cdataCountExplosions];
  cdataArrayExplosions[cdataCountExplosions]=NULL;
}


int cdataCreateShip(object_t objectID, double xcoord, double ycoord) {
  // Returns SW_FALSE if there is no room in the array;
  if (cdataCountShips >= MAX_COUNT_SHIPS) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (cdataArrayShips[cdataCountShips] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodeShips *newNode;
  newNode = (struct dataNodeShips *)malloc(sizeof(struct dataNodeShips));
  cdataArrayShips[cdataCountShips] = newNode;

  // Fill in the data values
  newNode->objectID = objectID;
  newNode->xcoord = xcoord;
  newNode->ycoord = ycoord;
  newNode->status = 1; // Live
  newNode->energy = SHIP_MAX_ENERGY;
  newNode->shields = SHIP_MAX_SHIELDS;
  newNode->orientation = 0.0;
  newNode->xVelocity = 0.0;
  newNode->yVelocity = 0.0;

  // Increment the count
  cdataCountShips++;

  // Attach to clientShip if it is the client's ship
  if (objectID == clientObjectID) {
    if (DEBUG) printf("Attaching clientShip pointer\n");
    clientShip = newNode;
  }

  // Return true
  return SW_TRUE;
}

int cdataCreateMissile(object_t objectID, double xcoord, double ycoord, double orientation, double xVelocity, double yVelocity) {
  // Returns SW_FALSE if there is no room in the array;
  if (cdataCountMissiles >= MAX_COUNT_MISSILES) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (cdataArrayMissiles[cdataCountMissiles] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodeMissiles *newNode;
  newNode = (struct dataNodeMissiles *)malloc(sizeof(struct dataNodeMissiles));
  cdataArrayMissiles[cdataCountMissiles] = newNode;

  // Fill in the data values
  newNode->objectID = objectID;
  newNode->xcoord = xcoord;
  newNode->ycoord = ycoord;
  newNode->orientation = orientation;
  newNode->xVelocity = xVelocity;
  newNode->yVelocity = yVelocity;
  newNode->ttl = time(NULL) + CLIENT_MISSILE_STALE;

  // Increment the count
  cdataCountMissiles++;

  // Return true
  return SW_TRUE;
}

int cdataCreatePlanet(object_t objectID, double xcoord, double ycoord) {
  // Returns SW_FALSE if there is no room in the array;
  if (cdataCountPlanets >= MAX_COUNT_PLANETS) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (cdataArrayPlanets[cdataCountPlanets] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodePlanets *newNode;
  newNode = (struct dataNodePlanets *)malloc(sizeof(struct dataNodePlanets));
  cdataArrayPlanets[cdataCountPlanets] = newNode;

  // Fill in the data values
  newNode->objectID = objectID;
  newNode->xcoord = xcoord;
  newNode->ycoord = ycoord;
  newNode->volcano = 0;

  // Increment the count
  cdataCountPlanets++;

  // Return true
  return SW_TRUE;
}

// Deletion functions
void cdataDeleteShip(object_t objectID) {
  int i;
  struct dataNodeShips *delNode = NULL;

  // Locate the node to delete
  for(i=0; i < cdataCountShips; i++) {
    if (cdataArrayShips[i] == NULL) {
      // We have a problem... I should fix this.
      continue;
    } 
    if (cdataArrayShips[i]->objectID == objectID) {
      // This is the node to delete
      delNode = cdataArrayShips[i];
      // Move the last node in this place
      if (cdataCountShips > 1) {
	cdataArrayShips[i] = cdataArrayShips[cdataCountShips - 1];
	cdataArrayShips[cdataCountShips - 1] = NULL;
	cdataCountShips--;
      } else {
	// There is only one node and it is being deleted.
	cdataArrayShips[0] = NULL;
	cdataCountShips = 0;
      }
    }
  }

  if (delNode != NULL) free(delNode);
}

void cdataDeleteMissile(object_t objectID) {
  int i;
  struct dataNodeMissiles *delNode = NULL;

  // Locate the node to delete
  for(i=0; i < cdataCountMissiles; i++) {
    if (cdataArrayMissiles[i] == NULL) {
      // We have a problem... I should fix this.
      continue;
    } 
    if (cdataArrayMissiles[i]->objectID == objectID) {
      // This is the node to delete
      delNode = cdataArrayMissiles[i];
      // Move the last node in this place
      if (cdataCountMissiles > 1) {
	cdataArrayMissiles[i] = cdataArrayMissiles[cdataCountMissiles - 1];
	cdataArrayMissiles[cdataCountMissiles - 1] = NULL;
	cdataCountMissiles--;
      } else {
	// There is only one node and it is being deleted.
	cdataArrayMissiles[0] = NULL;
	cdataCountMissiles = 0;
      }
    }
  }

  if (delNode != NULL) free(delNode);
}

void cdataDeletePlanet(object_t objectID) {
  int i;
  struct dataNodePlanets *delNode = NULL;

  // Locate the node to delete
  for(i=0; i < cdataCountPlanets; i++) {
    if (cdataArrayPlanets[i] == NULL) {
      // We have a problem... I should fix this.
      continue;
    } 
    if (cdataArrayPlanets[i]->objectID == objectID) {
      // This is the node to delete
      delNode = cdataArrayPlanets[i];
      // Move the last node in this place
      if (cdataCountPlanets > 1) {
	cdataArrayPlanets[i] = cdataArrayPlanets[cdataCountPlanets - 1];
	cdataArrayPlanets[cdataCountPlanets - 1] = NULL;
	cdataCountPlanets--;
      } else {
	// There is only one node and it is being deleted.
	cdataArrayPlanets[0] = NULL;
	cdataCountPlanets = 0;
      }
    }
  }

  if (delNode != NULL) free(delNode);
}

// Update Functions
void cdataUpdateAll() {
  time_t curtime = time(NULL);
  int i;
  // Remove unneeded objects from memory

  // Check for missiles to remove
  for(i=0; i < cdataCountMissiles; i++) {
    if (cdataArrayMissiles[i] == NULL) continue;
    if (cdataArrayMissiles[i]->ttl < curtime) {
      cdataDeleteMissile(cdataArrayMissiles[i]->objectID);
      // We need to check the same array slot again
      i--;
      continue;
    }
  }

}

int cdataUpdateShip(object_t objectID, double xcoord, double ycoord) {
  int i;

  // Locate the node to delete
  for(i=0; i < cdataCountShips; i++) {
    if (cdataArrayShips[i] == NULL) continue;
    if (cdataArrayShips[i]->objectID == objectID) {
      // This is the node to delete
      cdataArrayShips[i]->xcoord = xcoord;
      cdataArrayShips[i]->ycoord = ycoord;
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

int cdataUpdateMissile(object_t objectID, double xcoord, double ycoord) {
  int i;

  // Locate the node to delete
  for(i=0; i < cdataCountMissiles; i++) {
    if (cdataArrayMissiles[i] == NULL) continue;
    if (cdataArrayMissiles[i]->objectID == objectID) {
      // This is the node to delete
      cdataArrayMissiles[i]->xcoord = xcoord;
      cdataArrayMissiles[i]->ycoord = ycoord;
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

int cdataUpdatePlanet(object_t objectID, double xcoord, double ycoord) {
  int i;

  // Locate the node to delete
  for(i=0; i < cdataCountPlanets; i++) {
    if (cdataArrayPlanets[i] == NULL) continue;
    if (cdataArrayPlanets[i]->objectID == objectID) {
      // This is the node to delete
      cdataArrayPlanets[i]->xcoord = xcoord;
      cdataArrayPlanets[i]->ycoord = ycoord;
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

// Lookup functions
struct dataNodeClients *cdataLookupClient(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < cdataCountClients; i++) {
    // We do not want empty nodes
    if (cdataArrayClients[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (cdataArrayClients[i]->objectID == objectID) {
      return cdataArrayClients[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}

struct dataNodeShips *cdataLookupShips(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < cdataCountShips; i++) {
    // We do not want empty nodes
    if (cdataArrayShips[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (cdataArrayShips[i]->objectID == objectID) {
      return cdataArrayShips[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}

struct dataNodeMissiles *cdataLookupMissiles(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < cdataCountMissiles; i++) {
    // We do not want empty nodes
    if (cdataArrayMissiles[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (cdataArrayMissiles[i]->objectID == objectID) {
      return cdataArrayMissiles[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}

struct dataNodePlanets *cdataLookupPlanets(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < cdataCountPlanets; i++) {
    // We do not want empty nodes
    if (cdataArrayPlanets[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (cdataArrayPlanets[i]->objectID == objectID) {
      return cdataArrayPlanets[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}
