#include "global.h"
#include "sdata.h"
#include "server.h"

/* Global variable definitions (declared extern in sdata.h) */
int serverSocket;
object_t sdataNextObjectID;
int sdataCountClients;
int sdataCountExplosions;
int sdataCountShips;
int sdataCountMissiles;
int sdataCountPlanets;
double lastTime;
struct dataNodeClients *sdataArrayClients[MAX_COUNT_CLIENTS];
struct dataNodeExplosions *sdataArrayExplosions[MAX_COUNT_EXPLOSIONS];
struct dataNodeShips *sdataArrayShips[MAX_COUNT_SHIPS];
struct dataNodeMissiles *sdataArrayMissiles[MAX_COUNT_MISSILES];
struct dataNodePlanets *sdataArrayPlanets[MAX_COUNT_PLANETS];
int scoreChanged;

/* Server Data Access Functions */
object_t sdataNewObjectID() {
  return sdataNextObjectID++;
}

double randRange(double minValue, double maxValue) {
  double value;
  double range = maxValue - minValue;
  int randInt = rand() % (int)range; 
  value = (double)(randInt + minValue);
  return value;
}

/* Server Data Initialization */
void sdataInitialize() {
  int i;

  // Global variables
  sdataNextObjectID = 1;
  lastTime = platformNow();

  // Initialize all of the arrays
  for (i=0; i < MAX_COUNT_CLIENTS; i++) sdataArrayClients[i] = NULL;
  for (i=0; i < MAX_COUNT_SHIPS; i++) sdataArrayShips[i] = NULL;
  for (i=0; i < MAX_COUNT_MISSILES; i++) sdataArrayMissiles[i] = NULL;
  for (i=0; i < MAX_COUNT_PLANETS; i++) sdataArrayPlanets[i] = NULL;
  scoreChanged = 0;

  // Initialize all of the object counts
  sdataCountClients = 0;
  sdataCountShips = 0;
  sdataCountMissiles = 0;
  sdataCountPlanets = 0;
}

int lastTimeLeft(int usec) {
  // Returns how much of the usec microsecond wait remains, after taking
  // off the time passed since the previous call.
  double now = platformNow();
  int timePassed = (int)((now - lastTime) * 1000000.0);
  lastTime = now;

  if (timePassed >= usec) return 0;
  return usec - timePassed;
}

// Scores live on the client record; ships and missile owners share the
// client's objectID. Any change sets scoreChanged so the scoreboard goes out.
void scoreAdd(object_t objectID, int score) {
  struct dataNodeClients *client = sdataLookupClient(objectID);
  if (client == NULL) return;
  client->score += score;
  // Make sure the score is not negative
  if (client->score < 0) client->score = 0;
  scoreChanged = 1;
}

int scoreGet(object_t objectID) {
  struct dataNodeClients *client = sdataLookupClient(objectID);
  if (client == NULL) return 0;
  return client->score;
}

void scoreCountKill(object_t objectID) {
  struct dataNodeClients *client = sdataLookupClient(objectID);
  if (client == NULL) return;
  client->kills++;
  scoreChanged = 1;
}

void scoreCountDeath(object_t objectID) {
  struct dataNodeClients *client = sdataLookupClient(objectID);
  if (client == NULL) return;
  client->deaths++;
  scoreChanged = 1;
}

// Client Information
object_t sdataGetClientIDbyUsername(char *username) {
  int i;
  if (DEBUG) printf("    sdataGetClientIDbyUsername('%s')\n", username);
  for(i=0; i < sdataCountClients; i++) {
    if (strncmp(username, sdataArrayClients[i]->username, USERNAME_MAX) == 0) {
      // We have found the matching client
      return sdataArrayClients[i]->objectID;
    }
  }
  return SW_FALSE;
}

object_t sdataGetClientIDbyAddress(struct sockaddr_in client_addr) {
  int i;
  if (DEBUG) printf("    sdataGetClientIDbyAddress(ADDR)\n");
  for(i=0; i < sdataCountClients; i++) {
    if (client_addr.sin_family == (sdataArrayClients[i]->client_addr)->sin_family &&
	client_addr.sin_port == (sdataArrayClients[i]->client_addr)->sin_port &&
	client_addr.sin_addr.s_addr == (sdataArrayClients[i]->client_addr)->sin_addr.s_addr) {
      // We have found the matching client
      return sdataArrayClients[i]->objectID;
    }
  }
  return SW_FALSE;
}

int sdataCreateClient(char *username, object_t objectID, struct sockaddr_in client_addr) {
  // Log entry into function
  if (DEBUG) printf("    sdataCreateClient('%s', %d, ADDR)\n", username, objectID);

  // Returns SW_FALSE if there is no room in the array;
  if (sdataCountClients >= MAX_COUNT_CLIENTS) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (sdataArrayClients[sdataCountClients] != NULL) return SW_FALSE;

  // Create the new objects and put them in the array
  struct sockaddr_in *newAddr;
  struct dataNodeClients *newNode;
  newAddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
  newNode = (struct dataNodeClients *)malloc(sizeof(struct dataNodeClients));
  memcpy(newAddr, &client_addr, sizeof(struct sockaddr_in));
  sdataArrayClients[sdataCountClients] = newNode;

  // Increment the count
  sdataCountClients++;

  // Fill in the data values
  strncpy(newNode->username, username, USERNAME_MAX);
  newNode->client_addr = newAddr;
  newNode->objectID = objectID;
  newNode->checkin = time(NULL);
  newNode->score = 0;
  newNode->kills = 0;
  newNode->deaths = 0;
  scoreChanged = 1; // A new line on the scoreboard

  // Return true
  return SW_TRUE;
}

/* Server Data Access Functions */
// Existance Functions
int sdataExistsClient(object_t objectID) {
  int i;

  for(i=0; i < sdataCountClients; i++) {
    if (sdataArrayClients[i] == NULL) continue;
    if (sdataArrayClients[i]->objectID == objectID) {
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

// Creation Functions
int sdataCreateExplosion(double xcoord, double ycoord) {
  int i;
  // Log entry into function
  if (DEBUG) printf("    sdataCreateExplosion(%.2f, %.2f)\n", xcoord, ycoord);

  // Send the explosion to each client
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_explosion(serverSocket, *(sdataArrayClients[i]->client_addr), xcoord, ycoord);
  }

  // Return true
  return SW_TRUE;
}

int sdataCreateShip(object_t objectID, double xcoord, double ycoord) {
  // Log entry into function
  if (DEBUG) printf("    sdataCreateShip(%d, %.2f, %.2f)\n", objectID, xcoord, ycoord);

  // Returns SW_FALSE if there is no room in the array;
  if (sdataCountShips >= MAX_COUNT_SHIPS) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (sdataArrayShips[sdataCountShips] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodeShips *newNode;
  newNode = (struct dataNodeShips *)malloc(sizeof(struct dataNodeShips));
  sdataArrayShips[sdataCountShips] = newNode;

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
  sdataCountShips++;

  // Return true
  return SW_TRUE;
}

int sdataCreateMissile(object_t objectID, double xcoord, double ycoord, double orientation, double xVelocity, double yVelocity, object_t owner) {
  // Log entry into function
  if (DEBUG) printf("    sdataCreateMissile(%d, %.2f, %.2f)\n", objectID, xcoord, ycoord);

  // Returns SW_FALSE if there is no room in the array;
  if (sdataCountMissiles >= MAX_COUNT_MISSILES) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (sdataArrayMissiles[sdataCountMissiles] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodeMissiles *newNode;
  newNode = (struct dataNodeMissiles *)malloc(sizeof(struct dataNodeMissiles));
  sdataArrayMissiles[sdataCountMissiles] = newNode;

  // Fill in the data values
  newNode->objectID = objectID;
  newNode->xcoord = xcoord;
  newNode->ycoord = ycoord;
  newNode->orientation = orientation;
  newNode->xVelocity = xVelocity;
  newNode->yVelocity = yVelocity;
  newNode->owner = owner;
  newNode->ttl = time(NULL) + MISSILE_TTL;

  // Increment the count
  sdataCountMissiles++;

  // Return true
  return SW_TRUE;
}

int sdataCreatePlanet(object_t objectID, double xcoord, double ycoord) {
  // Log entry into function
  if (DEBUG) printf("    sdataCreatePlanet(%d, %.2f, %.2f)\n", objectID, xcoord, ycoord);

  // Returns SW_FALSE if there is no room in the array;
  if (sdataCountPlanets >= MAX_COUNT_PLANETS) return SW_FALSE;

  // Makes sure the place in the array is NULL
  if (sdataArrayPlanets[sdataCountPlanets] != NULL) return SW_FALSE;

  // Create the new object and put it in the array
  struct dataNodePlanets *newNode;
  newNode = (struct dataNodePlanets *)malloc(sizeof(struct dataNodePlanets));
  sdataArrayPlanets[sdataCountPlanets] = newNode;

  // Fill in the data values
  newNode->objectID = objectID;
  newNode->xcoord = xcoord;
  newNode->ycoord = ycoord;

  // Increment the count
  sdataCountPlanets++;

  // Return true
  return SW_TRUE;
}

// Deletion functions
void sdataDeleteClient(object_t objectID) {
  int i;
  struct sockaddr_in *delAddr = NULL;
  struct dataNodeClients *delNode = NULL;

  // Log entry into function
  if (DEBUG) printf("    sdataDeleteClient(%d)\n", objectID);

  // Delete the ship from clients
  sdataDeleteShip(objectID);

  // Search for node to delete
  for(i=0; i < sdataCountClients; i++) {
    if (sdataArrayClients[i]->objectID == objectID) {
      delNode = sdataArrayClients[i];
      delAddr = sdataArrayClients[i]->client_addr;
      // Move the last node in this one's place.  Could be the same node.
      sdataCountClients--;
      sdataArrayClients[i] = sdataArrayClients[sdataCountClients];
      // Now we will NULL the last one.  This could be the one we just
      // moved if it is the one we are deleting.
      sdataArrayClients[sdataCountClients] = NULL;
      // Now we will free the array spaces
      free(delAddr);
      free(delNode);
      scoreChanged = 1; // Their line leaves the scoreboard
      break;
    }
  }
}

void sdataDeleteShip(object_t objectID) {
  int i;
  struct dataNodeShips *delNode = NULL;

  // Let the clients know it is being deleted
  broadcastDeleteShip(objectID);  

  // Locate the node to delete
  for(i=0; i < sdataCountShips; i++) {
    if (sdataArrayShips[i] == NULL) {
      // We have a problem... I should fix this.
      continue;
    } 
    if (sdataArrayShips[i]->objectID == objectID) {
      // This is the node to delete
      delNode = sdataArrayShips[i];
      // Move the last node in this place
      if (sdataCountShips > 1) {
	sdataArrayShips[i] = sdataArrayShips[sdataCountShips - 1];
	sdataArrayShips[sdataCountShips - 1] = NULL;
	sdataCountShips--;
      } else {
	// There is only one node and it is being deleted.
	sdataArrayShips[0] = NULL;
	sdataCountShips = 0;
      }
    }
  }

  if (delNode != NULL) free(delNode);
}

void sdataDeleteMissile(object_t objectID) {
  int i;
  struct dataNodeMissiles *delNode = NULL;

  // Locate the node to delete
  for(i=0; i < sdataCountMissiles; i++) {
    if (sdataArrayMissiles[i] == NULL) {
      // We have a problem... I should fix this.
      continue;
    } 
    if (sdataArrayMissiles[i]->objectID == objectID) {
      // This is the node to delete
      delNode = sdataArrayMissiles[i];
      broadcastDeleteMissile(objectID);
      // Move the last node in this place
      if (sdataCountMissiles > 1) {
	sdataArrayMissiles[i] = sdataArrayMissiles[sdataCountMissiles - 1];
	sdataArrayMissiles[sdataCountMissiles - 1] = NULL;
	sdataCountMissiles--;
      } else {
	// There is only one node and it is being deleted.
	sdataArrayMissiles[0] = NULL;
	sdataCountMissiles = 0;
      }
    }
  }

  if (delNode != NULL) free(delNode);
}

void sdataDeletePlanet(object_t objectID) {
  int i;
  struct dataNodePlanets *delNode = NULL;

  // Locate the node to delete
  for(i=0; i < sdataCountPlanets; i++) {
    if (sdataArrayPlanets[i] == NULL) {
      // We have a problem... I should fix this.
      continue;
    } 
    if (sdataArrayPlanets[i]->objectID == objectID) {
      // This is the node to delete
      delNode = sdataArrayPlanets[i];
      // Move the last node in this place
      if (sdataCountPlanets > 1) {
	sdataArrayPlanets[i] = sdataArrayPlanets[sdataCountPlanets - 1];
	sdataArrayPlanets[sdataCountPlanets - 1] = NULL;
	sdataCountPlanets--;
      } else {
	// There is only one node and it is being deleted.
	sdataArrayPlanets[0] = NULL;
	sdataCountPlanets = 0;
      }
    }
  }

  if (delNode != NULL) free(delNode);
}

// Update Functions
void sdataUpdateAll() {
  time_t curtime = time(NULL);
  int i;
  // Remove unneeded objects from memory

  // Check for missiles to remove
  for(i=0; i < sdataCountMissiles; i++) {
    if (sdataArrayMissiles[i] == NULL) continue;
    if (sdataArrayMissiles[i]->ttl < curtime) {
      sdataCreateExplosion(sdataArrayMissiles[i]->xcoord, sdataArrayMissiles[i]->ycoord);
      sdataDeleteMissile(sdataArrayMissiles[i]->objectID);
      // We need to check the same place in the array again
      i--;
      continue;
    }
  }

}

int sdataUpdateShip(object_t objectID, double xcoord, double ycoord) {
  int i;

  for(i=0; i < sdataCountShips; i++) {
    if (sdataArrayShips[i] == NULL) continue;
    if (sdataArrayShips[i]->objectID == objectID) {
      // This is the node to delete
      sdataArrayShips[i]->xcoord = xcoord;
      sdataArrayShips[i]->ycoord = ycoord;
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

int sdataUpdateMissile(object_t objectID, double xcoord, double ycoord) {
  int i;

  for(i=0; i < sdataCountMissiles; i++) {
    if (sdataArrayMissiles[i] == NULL) continue;
    if (sdataArrayMissiles[i]->objectID == objectID) {
      // This is the node to delete
      sdataArrayMissiles[i]->xcoord = xcoord;
      sdataArrayMissiles[i]->ycoord = ycoord;
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

int sdataUpdatePlanet(object_t objectID, double xcoord, double ycoord) {
  int i;

  for(i=0; i < sdataCountPlanets; i++) {
    if (sdataArrayPlanets[i] == NULL) continue;
    if (sdataArrayPlanets[i]->objectID == objectID) {
      // This is the node to delete
      sdataArrayPlanets[i]->xcoord = xcoord;
      sdataArrayPlanets[i]->ycoord = ycoord;
      return SW_TRUE;
    }
  }
  return SW_FALSE;
}

// Lookup functions
struct dataNodeClients *sdataLookupClient(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < sdataCountClients; i++) {
    // We do not want empty nodes
    if (sdataArrayClients[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (sdataArrayClients[i]->objectID == objectID) {
      return sdataArrayClients[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}

struct dataNodeShips *sdataLookupShips(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < sdataCountShips; i++) {
    // We do not want empty nodes
    if (sdataArrayShips[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (sdataArrayShips[i]->objectID == objectID) {
      return sdataArrayShips[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}

struct dataNodeMissiles *sdataLookupMissiles(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < sdataCountMissiles; i++) {
    // We do not want empty nodes
    if (sdataArrayMissiles[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (sdataArrayMissiles[i]->objectID == objectID) {
      return sdataArrayMissiles[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}

struct dataNodePlanets *sdataLookupPlanets(object_t objectID) {
  int i;
  // Locate the node
  for(i=0; i < sdataCountPlanets; i++) {
    // We do not want empty nodes
    if (sdataArrayPlanets[i] == NULL) continue;
    // If we have found the node, we want to return it.
    if (sdataArrayPlanets[i]->objectID == objectID) {
      return sdataArrayPlanets[i];
    }
  }
  // Return NULL if the node was not found
  return NULL;
}
