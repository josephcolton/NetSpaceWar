#include <stdarg.h>
#include "global.h"
#include "server.h"
#include "sdata.h"
#include "physics.h"
#include "asteroids.h"
#include "match.h"

int serverLogLevel = LOG_EVENTS;
char serverHostName[USERNAME_MAX];
int serverHostJoined;
int serverShutdown;

void serverClientLeaving(object_t objectID) {
  struct dataNodeClients *client = sdataLookupClient(objectID);
  if (client == NULL || serverHostName[0] == 0) return;
  if (strcmp(client->username, serverHostName) == 0) {
    serverLog("The host left, so the server is shutting down");
    serverShutdown = 1;
  }
}

void serverLog(const char *format, ...) {
  va_list args;
  char stamp[16];
  time_t now = time(NULL);

  if (serverLogLevel < LOG_EVENTS) return;
  strftime(stamp, sizeof(stamp), "%H:%M:%S", localtime(&now));
  printf("[%s] ", stamp);
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\n");
  fflush(stdout);
}

// Show a line to every player, at the bottom of their screen
void broadcastEvent(const char *format, ...) {
  struct msg_event packet;
  va_list args;
  int i;

  memset(&packet, 0, sizeof(packet));
  packet.mtype = MSG_EVENT;
  va_start(args, format);
  vsnprintf(packet.text, EVENT_TEXT_MAX, format, args);
  va_end(args);
  for (i=0; i < sdataCountClients; i++) {
    sendto(serverSocket, (const char *)&packet, sizeof(packet), 0,
           (struct sockaddr *)sdataArrayClients[i]->client_addr, sizeof(struct sockaddr_in));
  }
}

void serverEvent(const char *format, ...) {
  char text[EVENT_TEXT_MAX];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  serverLog("%s", text);
  broadcastEvent("%s", text);
}

const char *serverClientName(object_t objectID) {
  struct dataNodeClients *client = sdataLookupClient(objectID);
  return client ? client->username : "A player who left";
}

// A random place to put a ship where it has a fair chance: far enough from
// planets to escape (inside their gravity is fine), clear of asteroids, and
// not on top of another ship. After many failed tries the last spot is used.
void serverSpawnPoint(object_t self, double *xcoord, double *ycoord) {
  int tries, i, clear;
  for (tries=0; tries < 100; tries++) {
    *xcoord = SPACE_MIN_XCOORD + randUnit() * (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    *ycoord = SPACE_MIN_YCOORD + randUnit() * (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    clear = 1;
    for (i=0; i < sdataCountPlanets && clear; i++) {
      if (inRange(*xcoord, *ycoord, sdataArrayPlanets[i]->xcoord, sdataArrayPlanets[i]->ycoord, SPAWN_PLANET_DISTANCE)) clear = 0;
    }
    for (i=0; i < sdataCountAsteroids && clear; i++) {
      if (inRange(*xcoord, *ycoord, sdataArrayAsteroids[i]->xcoord, sdataArrayAsteroids[i]->ycoord, SPAWN_ASTEROID_DISTANCE)) clear = 0;
    }
    for (i=0; i < sdataCountShips && clear; i++) {
      if (sdataArrayShips[i] == NULL || sdataArrayShips[i]->status == 0 || sdataArrayShips[i]->objectID == self) continue;
      if (inRange(*xcoord, *ycoord, sdataArrayShips[i]->xcoord, sdataArrayShips[i]->ycoord, SPAWN_SHIP_DISTANCE)) clear = 0;
    }
    if (clear) return;
  }
}

int serverOpenDiscovery() {
  struct sockaddr_in addr;
  int s, on = 1;

  s = (int)socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) return -1;
  // Several servers on one computer can all answer searches
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(DISCOVERY_PORT);
  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    serverLog("Can't answer searches for games: port %d is in use", DISCOVERY_PORT);
    netCloseSocket(s);
    return -1;
  }
  netIgnoreConnReset(s);
  return s;
}

void serverAnswerDiscovery(int discoverySocket, int gamePort) {
  char buffer[64];
  struct sockaddr_in from;
  socklen_t fromLength = sizeof(from);
  struct msg_serverinfo info;
  int bytes;

  bytes = recvfrom(discoverySocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&from, &fromLength);
  if (bytes < (int)sizeof(message_t) || ((struct msg_basic *)buffer)->mtype != MSG_DISCOVER) return;
  memset(&info, 0, sizeof(info));
  info.mtype = MSG_SERVERINFO;
  info.port = (uint16_t)gamePort;
  info.players = (uint8_t)sdataCountClients;
  info.maxPlayers = MAX_COUNT_CLIENTS;
  strncpy(info.version, NSW_VERSION, VERSION_MAX - 1);
  strncpy(info.host, serverHostName, USERNAME_MAX - 1);
  sendto(discoverySocket, (const char *)&info, sizeof(info), 0, (struct sockaddr *)&from, fromLength);
}

// Initialize the server
void serverInitialize() {
  double xcoord, ycoord;
  int i, j, tries;
  if (VERBOSE) printf("Initializing Server\n");

  // Seed the randomizer
  srand(time(NULL));

  // Initialize sdata
  sdataInitialize();

  // Once the data has been initialized, we need to create
  // the map.
  for (i=0; i < SERVER_OPTION(PLANETS); i++) {
    // Keep planets apart, so they don't overlap or trap ships between them
    for (tries=0; tries < 100; tries++) {
      xcoord = randRange(SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
      ycoord = randRange(SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
      for (j=0; j < sdataCountPlanets; j++) {
	if (inRange(xcoord, ycoord, sdataArrayPlanets[j]->xcoord, sdataArrayPlanets[j]->ycoord, PLANET_SPACING)) break;
      }
      if (j == sdataCountPlanets) break;
    }
    sdataCreatePlanet(sdataNewObjectID(), xcoord, ycoord);
  }
  volcanoInitialize();
  asteroidsInitialize();
}

// Client Keepalive Checks
void checkClientKeepAlive() {
  int i;
  time_t curTime = time(NULL);

  for(i=0; i < sdataCountClients; i++) {
    if (sdataArrayClients[i]->checkin + (KEEPALIVE_LIMIT * 3) < curTime) {
      serverLog("%s timed out", sdataArrayClients[i]->username);
      broadcastEvent("%s left (lost connection)", sdataArrayClients[i]->username);
      // Client has not reported in the last KEEPALIVE_LIMIT * 3 seconds
      // Tell the client to log out
      ssend_msg_logoutack(serverSocket, *(sdataArrayClients[i]->client_addr));
      // Delete the client node
      serverClientLeaving(sdataArrayClients[i]->objectID);
      sdataDeleteClient(sdataArrayClients[i]->objectID);
      // Repeat the same client index, incase there are more.
      i--;
      continue;
    }
  }
}

/* Server Handling Messages */
void shandle_msg_login(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;
  object_t objectID;
  char username[USERNAME_MAX];
  char version[VERSION_MAX];
  int compatible;
  double xcoord, ycoord;

  if (VERBOSE) printf("Packet Received: shandle_msg_login\n");

  // Verify message type
  mtype = ((struct msg_login *)packet)->mtype;
  if (mtype != MSG_LOGIN) return;
  if (VERBOSE) printf("  packet->mtype = %d\n", mtype);

  // Get the other variables
  strncpy(username, ((struct msg_login *)packet)->username, USERNAME_MAX);
  username[USERNAME_MAX - 1] = 0; // The packet's copy may not be terminated
  if (VERBOSE) printf("  packet->username = '%s'\n", username);

  // Players need the same major version (see versionCompatible())
  strncpy(version, ((struct msg_login *)packet)->version, VERSION_MAX);
  version[VERSION_MAX - 1] = 0;
  compatible = versionCompatible(version);
  if (version[0] == 0) strcpy(version, VERSION_OLDEST);
  if (!compatible) {
    serverLog("%s could not join: they have version %s, the server has %s", username, version, NSW_VERSION);
    ssend_msg_loginack(mysocket, client_addr, STATUS_VERSION, 0);
    return;
  }

  // Additional details
  if (VERBOSE) printf("  Processing:\n");

  // Verify that the username does not exist
  objectID = sdataGetClientIDbyUsername(username);

  // A repeat login from the same address means our earlier loginack was
  // lost (clients resend until they hear back), so just acknowledge again
  if (objectID > 0 && objectID == sdataGetClientIDbyAddress(client_addr)) {
    ssend_msg_loginack(mysocket, client_addr, STATUS_OKAY, objectID);
    return;
  }

  // If the user does exist, we will return an error message and quit
  if (objectID > 0) {
    // The user already exists, send error reply with the objectID
    // of the client that matches
    if (VERBOSE) printf("    Client already exists\n");
    ssend_msg_loginack(mysocket, client_addr, STATUS_INUSE, objectID);
    return;
  } else {
    if (VERBOSE) printf("    Client does not yet exist.\n");
  }

  // Generate a objectID for the client
  objectID = sdataNewObjectID();
  if (VERBOSE) printf("    Assigning objectID = %d\n", objectID);

  // Create the client node
  if (!sdataCreateClient(username, objectID, client_addr)) {
    ssend_msg_loginack(mysocket, client_addr, STATUS_FULL, 0);
    return;
  }

  // Create a ship to go with the client
  serverSpawnPoint(objectID, &xcoord, &ycoord);
  sdataCreateShip(objectID, xcoord, ycoord);

  // Notify the client of their objectID
  ssend_msg_loginack(mysocket, client_addr, STATUS_OKAY, objectID);
  if (serverHostName[0] && strcmp(username, serverHostName) == 0) serverHostJoined = 1;
  serverLog("%s joined from %s:%d", username, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
  broadcastEvent("%s joined", username);
}

void shandle_msg_logout(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;
  object_t objectID;

  if (VERBOSE) printf("Packet Received: shandle_msg_logout\n");

  // Verify message type
  mtype = ((struct msg_logout *)packet)->mtype;
  if (mtype != MSG_LOGOUT) return;

  // Display packet contents
  if (VERBOSE) printf("  packet->mtype = %d\n", mtype);

  // Confirm logout with client
  ssend_msg_logoutack(serverSocket, client_addr);

  // Delete the client node
  objectID = sdataGetClientIDbyAddress(client_addr);
  if (objectID > 0) serverEvent("%s left", serverClientName(objectID));
  serverClientLeaving(objectID);
  sdataDeleteClient(objectID);
}

void shandle_msg_keepalive(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;

  if (VERBOSE) printf("Packet Received: shandle_msg_keepalive\n");

  // Verify message type
  mtype = ((struct msg_keepalive *)packet)->mtype;
  if (mtype != MSG_KEEPALIVE) return;

  // Display packet contents
  if (VERBOSE) printf("  packet->mtype = %d\n", mtype);
}

void shandle_msg_request(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;
  data_t dtype;

  if (VERBOSE) printf("Packet Received: shandle_msg_request\n");

  // Verify message type
  mtype = ((struct msg_request *)packet)->mtype;
  if (mtype != MSG_REQUEST) return;

  // Get the other variables
  dtype = ((struct msg_request *)packet)->dtype;

  // Display packet contents
  if (VERBOSE) printf("  packet->mtype = %d\n", mtype);
  if (VERBOSE) printf("  packet->dtype = %d\n", dtype);

  // Reply to client request for data
  if (dtype == DATA_STATICOBJS) ssend_msg_staticobjs(mysocket, client_addr);
  if (dtype == DATA_CLIENTINFO) ssend_msg_clientinfo(mysocket, client_addr);
  if (dtype == DATA_CLIENTLIST) ssend_msg_clientlist(mysocket, client_addr);
  if (dtype == DATA_SCORES) ssend_msg_scoreboard(mysocket, client_addr);
  if (dtype == DATA_OPTIONS) ssend_msg_gameoptions(mysocket, client_addr);
}

void shandle_msg_clientupdate(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;
  action_t action;
  double var1;
  double var2;
  int var3;
  object_t objectID;
  object_t newObjectID;
  struct dataNodeShips *clientShip;
  double orientation;
  double xcoord;
  double ycoord;
  double xVelocity;
  double yVelocity;
  double speed;

  if (VERBOSE) printf("Packet Received: shandle_msg_clientupdate\n");

  // Verify message type
  mtype = ((struct msg_clientupdate *)packet)->mtype;
  if (mtype != MSG_CLIENTUPDATE) return;

  // Figure out which ship the packet was from
  objectID = sdataGetClientIDbyAddress(client_addr);
  clientShip = sdataLookupShips(objectID);
  if (clientShip == NULL) return;

  // Get the other variables
  action = ((struct msg_clientupdate *)packet)->action;
  var1 = ((struct msg_clientupdate *)packet)->var1;
  var2 = ((struct msg_clientupdate *)packet)->var2;
  var3 = ((struct msg_clientupdate *)packet)->var3;

  // Display packet contents
  if (VERBOSE) printf("  packet->mtype = %d\n", mtype);
  if (VERBOSE) printf("  packet->action = %d\n", action);
  if (VERBOSE) printf("  packet->var1 = %.2f\n", var1);
  if (VERBOSE) printf("  packet->var2 = %.2f\n", var2);
  if (VERBOSE) printf("  packet->var3 = %d\n", var3);

  // Nobody moves while the match rankings are up
  if (matchOver) return;

  // Update ship status
  if (action == ACTION_THRUST) {
    // var1 is thrust
    // var2 is direction
    clientShip->orientation += var2;
    if (clientShip->orientation > 2*M_PI) 
	    clientShip->orientation -= 2*M_PI;
    if (clientShip->orientation < -2*M_PI) 
	    clientShip->orientation += 2*M_PI;

    //    if (var1*THRUST_COST > clientShip->energy) var1=clientShip->energy/THRUST_COST; // This won't work without some other info, can't make it cheap enough because energy is an integer.
    clientShip->xVelocity += cos(clientShip->orientation)*var1;
    clientShip->yVelocity += sin(clientShip->orientation)*var1;
    //    clientShip->energy-=var1*THRUST_COST;


    // Thrust cannot push the ship past the speed limit
    speed = sqrt(clientShip->xVelocity*clientShip->xVelocity + clientShip->yVelocity*clientShip->yVelocity);
    if (speed > SERVER_OPTION(MAX_VELOCITY)) {
      clientShip->xVelocity *= SERVER_OPTION(MAX_VELOCITY) / speed;
      clientShip->yVelocity *= SERVER_OPTION(MAX_VELOCITY) / speed;
    }

  }
  
  if (action == ACTION_FIRE) {
    // Verify that the ship has the energy
    if (clientShip->energy < SERVER_OPTION(MISSILE_COST)) return;
    clientShip->energy -= SERVER_OPTION(MISSILE_COST);

    // Create the missile  
    newObjectID = sdataNewObjectID();
    orientation = clientShip->orientation;
    xVelocity = clientShip->xVelocity+cos(clientShip->orientation)*MISSILE_EXTRA_SPEED;
    yVelocity = clientShip->yVelocity+sin(clientShip->orientation)*MISSILE_EXTRA_SPEED;
    xcoord = clientShip->xcoord + cos(clientShip->orientation)*(P_GUI_SHIP_RADIUS_INTERNAL+3*P_GUI_SHOT_RADIUS_INTERNAL);
    ycoord = clientShip->ycoord + sin(clientShip->orientation)*(P_GUI_SHIP_RADIUS_INTERNAL+3*P_GUI_SHOT_RADIUS_INTERNAL);
    sdataCreateMissile(newObjectID, xcoord, ycoord, orientation, xVelocity, yVelocity, clientShip->objectID);
  }

  if (action == ACTION_JUMP) {
    if (clientShip->energy >= SERVER_OPTION(JUMP_COST) || clientShip->status == 0)
    {
      // Put the ship in a new location, somewhere it has a chance
      serverSpawnPoint(objectID, &xcoord, &ycoord);
      clientShip->xcoord = xcoord;
      clientShip->ycoord = ycoord;
      // Stop the ship from moving
      clientShip->xVelocity = 0.0;
      clientShip->yVelocity = 0.0;
      // Use the energy cost
      clientShip->energy -= SERVER_OPTION(JUMP_COST);
    }    

    // if the ship is dead, we need to bring it back to life
    if (clientShip->status == 0) {
      clientShip->status = 1;
      clientShip->energy = SHIP_MAX_ENERGY;
      clientShip->shields = SHIP_MAX_SHIELDS;
      // The penalty for dying was taken when the ship was destroyed
      serverLog("%s respawned", serverClientName(objectID));
    }

  }

  if (action == ACTION_TRANSPWR) {
    // Dead ships have no power to move
    if (clientShip->status == 0) return;
    // Need to make sure var3 is a reasonable value
    // Transfer power from energy to shields
    clientShip->shields += var3;
    clientShip->energy -= var3;
    if (clientShip->shields<0)
    {
      clientShip->energy+=clientShip->shields;
      clientShip->shields=0;
    }
    if (clientShip->energy<0)
    {
      clientShip->shields+=clientShip->energy;
      clientShip->energy=0;
    }

    if (clientShip->shields>SHIP_MAX_SHIELDS)
    {
      clientShip->energy+=clientShip->shields-SHIP_MAX_SHIELDS;
      clientShip->shields=SHIP_MAX_SHIELDS;
    }
    if (clientShip->energy>SHIP_MAX_ENERGY)
    {
      clientShip->shields+=clientShip->energy-SHIP_MAX_ENERGY;
      clientShip->energy=SHIP_MAX_ENERGY;
    }



  }
}

/* Server Sending Messages */
void ssend_msg_loginack(int mysocket, struct sockaddr_in client_addr, status_t status, object_t objectID) {
  int bytes;
  struct msg_loginack packet;
  // lastcontact = time(NULL);

  if (VERBOSE) printf("Packet Created: ssend_msg_loginack\n");

  // Construct the packet
  memset(&packet, 0, sizeof(packet));
  packet.mtype = MSG_LOGINACK;
  packet.status = status;
  packet.objectID = objectID;
  strncpy(packet.version, NSW_VERSION, VERSION_MAX - 1);

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_loginack: %d\n", netLastError());
  }
}

void ssend_msg_logoutack(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  struct msg_logoutack packet;
  // lastcontact = time(NULL);

  if (VERBOSE) printf("Packet Created: ssend_msg_logoutack\n");

  // Construct the packet
  packet.mtype = MSG_LOGOUTACK;

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_logoutack: %d\n", netLastError());
  }
}

void ssend_msg_staticobjs(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_staticobjs *packet = NULL;
  int packetSize = 0;

  if (VERBOSE) printf("Packet Created: ssend_msg_staticobjs\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_staticobjs);
  packetSize += sizeof(struct dataNodePlanets) * sdataCountPlanets;
  packet = (struct msg_staticobjs *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_STATICOBJS;
  packet->obj_count = sdataCountPlanets;
  if (VERBOSE) printf("  packet->mtype = %d\n", packet->mtype);
  if (VERBOSE) printf("  packet->obj_count = %d\n", packet->obj_count);
  for(i=0; i < sdataCountPlanets; i++) {
    (packet->planets[i]).objectID = sdataArrayPlanets[i]->objectID;
    (packet->planets[i]).xcoord = sdataArrayPlanets[i]->xcoord;
    (packet->planets[i]).ycoord = sdataArrayPlanets[i]->ycoord;
    (packet->planets[i]).volcano = sdataArrayPlanets[i]->volcano;
    if (VERBOSE) printf("  packet->planet[%d].objectID = %d\n", i, (packet->planets[i]).objectID);
    if (VERBOSE) printf("  packet->planet[%d].xcoord = %.2f\n", i, (packet->planets[i]).xcoord);
    if (VERBOSE) printf("  packet->planet[%d].ycoord = %.2f\n", i, (packet->planets[i]).ycoord);
  }
 
  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)packet,
		 packetSize,
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_staticobjs: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_clientinfo(int mysocket, struct sockaddr_in client_addr) {
  // Client has requested personal information about itself.
  // This packet is to inform the client about itself.
  int bytes;
  struct msg_clientinfo packet;
  struct dataNodeShips *clientShip = NULL;

  if (VERBOSE) printf("Packet Created: ssend_msg_clientinfo\n");

  // Start packet population
  packet.mtype = MSG_CLIENTINFO;

  // First we need to look the client up.
  packet.objectID = sdataGetClientIDbyAddress(client_addr);

  // Now we will lookup the client's ship information
  clientShip = sdataLookupShips(packet.objectID);
  if (clientShip != NULL) {
    packet.xcoord = clientShip->xcoord;
    packet.ycoord = clientShip->ycoord;
    packet.status = clientShip->status;
    packet.energy = clientShip->energy;
    packet.shields = clientShip->shields;
    packet.orientation = clientShip->orientation;
    packet.xVelocity = clientShip->xVelocity;
    packet.yVelocity = clientShip->yVelocity;
  } else {
    packet.status = -1;
  }

  // Display packet information
  if (VERBOSE) printf("  packet.mtype = %d\n", packet.mtype);
  if (VERBOSE) printf("  packet.objectID = %d\n", packet.objectID);
  if (VERBOSE) printf("  packet.xcoord = %.2f\n", packet.xcoord);
  if (VERBOSE) printf("  packet.ycoord = %.2f\n", packet.ycoord);
  if (VERBOSE) printf("  packet.status = %d\n", packet.status);
  if (VERBOSE) printf("  packet.energy = %d\n", packet.energy);
  if (VERBOSE) printf("  packet.shields = %d\n", packet.shields);
  if (VERBOSE) printf("  packet.orientation = %.2f\n", packet.orientation);
  if (VERBOSE) printf("  packet.xVelocity = %.2f\n", packet.xVelocity);
  if (VERBOSE) printf("  packet.yVelocity = %.2f\n", packet.yVelocity);

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_clientinfo: %d\n", netLastError());
  }
}

void ssend_msg_updateplayers(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_updateplayers *packet = NULL;
  int packetSize = 0;

  if (VERBOSE) printf("Packet Created: ssend_msg_updateplayers\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_updateplayers);
  packetSize += sizeof(struct dataNodeShips) * sdataCountShips;
  packet = (struct msg_updateplayers *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_UPDATEPLAYERS;
  if (VERBOSE) printf("  packet->mtype = %d\n", packet->mtype);
  packet->obj_count = sdataCountShips;
  if (VERBOSE) printf("  packet->obj_count = %d\n", packet->obj_count);

  for(i=0; i < sdataCountShips; i++) {
    (packet->ships[i]).objectID = sdataArrayShips[i]->objectID;
    (packet->ships[i]).xcoord = sdataArrayShips[i]->xcoord;
    (packet->ships[i]).ycoord = sdataArrayShips[i]->ycoord;
    (packet->ships[i]).status = sdataArrayShips[i]->status;
    (packet->ships[i]).energy = sdataArrayShips[i]->energy;
    (packet->ships[i]).shields = sdataArrayShips[i]->shields;
    (packet->ships[i]).orientation = sdataArrayShips[i]->orientation;
    (packet->ships[i]).xVelocity = sdataArrayShips[i]->xVelocity;
    (packet->ships[i]).yVelocity = sdataArrayShips[i]->yVelocity;
    // Display ship contents
    if (VERBOSE) printf("  packet->ships[%d].objectID = %d\n", i, (packet->ships[i]).objectID);
    if (VERBOSE) printf("  packet->ships[%d].xcoord = %.2f\n", i, (packet->ships[i]).xcoord);
    if (VERBOSE) printf("  packet->ships[%d].ycoord = %.2f\n", i, (packet->ships[i]).ycoord);
    if (VERBOSE) printf("  packet->ships[%d].status = %d\n", i, (packet->ships[i]).status);
    if (VERBOSE) printf("  packet->ships[%d].energy = %d\n", i, (packet->ships[i]).energy);
    if (VERBOSE) printf("  packet->ships[%d].shields = %d\n", i, (packet->ships[i]).shields);
    if (VERBOSE) printf("  packet->ships[%d].orientation = %.2f\n", i, (packet->ships[i]).orientation);
    if (VERBOSE) printf("  packet->ships[%d].xVelocity = %.2f\n", i, (packet->ships[i]).xVelocity);
    if (VERBOSE) printf("  packet->ships[%d].yVelocity = %.2f\n", i, (packet->ships[i]).yVelocity);
  }

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)packet,
		 packetSize,
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_updateplayers: %d\n", netLastError());
  }



  free(packet);
}

void ssend_msg_updatemissiles(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_updatemissiles *packet = NULL;
  int packetSize = 0;

  if (VERBOSE) printf("Packet Created: ssend_msg_updatemissiles\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_updatemissiles);
  packetSize += sizeof(struct dataNodeMissiles) * sdataCountMissiles;
  packet = (struct msg_updatemissiles *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_UPDATEMISSILES;
  if (VERBOSE) printf("  packet->mtype = %d\n", packet->mtype);
  packet->obj_count = sdataCountMissiles;
  if (VERBOSE) printf("  packet->obj_count = %d\n", packet->obj_count);

  for(i=0; i < sdataCountMissiles; i++) {
    (packet->missiles[i]).objectID = sdataArrayMissiles[i]->objectID;
    (packet->missiles[i]).xcoord = sdataArrayMissiles[i]->xcoord;
    (packet->missiles[i]).ycoord = sdataArrayMissiles[i]->ycoord;
    (packet->missiles[i]).orientation = sdataArrayMissiles[i]->orientation;
    (packet->missiles[i]).xVelocity = sdataArrayMissiles[i]->xVelocity;
    (packet->missiles[i]).yVelocity = sdataArrayMissiles[i]->yVelocity;
    // Display ship contents
    if (VERBOSE) printf("  packet->missiles[%d].objectID = %d\n", i, (packet->missiles[i]).objectID);
    if (VERBOSE) printf("  packet->missiles[%d].xcoord = %.2f\n", i, (packet->missiles[i]).xcoord);
    if (VERBOSE) printf("  packet->missiles[%d].ycoord = %.2f\n", i, (packet->missiles[i]).ycoord);
    if (VERBOSE) printf("  packet->missiles[%d].orientation = %.2f\n", i, (packet->missiles[i]).orientation);
    if (VERBOSE) printf("  packet->missiles[%d].xVelocity = %.2f\n", i, (packet->missiles[i]).xVelocity);
    if (VERBOSE) printf("  packet->missiles[%d].yVelocity = %.2f\n", i, (packet->missiles[i]).yVelocity);
  }
 
  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)packet,
		 packetSize,
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_updatemissiles: %d\n", netLastError());
  }
  free(packet);
}

// Sent every tick, even when empty: clients drop asteroids missing from the list
void ssend_msg_updateasteroids(int mysocket, struct sockaddr_in client_addr) {
  int i, bytes;
  int packetSize = sizeof(struct msg_updateasteroids) + sizeof(struct dataNodeAsteroids) * sdataCountAsteroids;
  struct msg_updateasteroids *packet = (struct msg_updateasteroids *)malloc(packetSize);

  packet->mtype = MSG_UPDATEASTEROIDS;
  packet->obj_count = sdataCountAsteroids;
  for (i=0; i < sdataCountAsteroids; i++) packet->asteroids[i] = *sdataArrayAsteroids[i];

  bytes = sendto(mysocket, (const char *)packet, packetSize, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_updateasteroids: %d\n", netLastError());
  }
  free(packet);
}

// Sent every tick, even when empty: clients drop orbs missing from the list
void ssend_msg_updateorbs(int mysocket, struct sockaddr_in client_addr) {
  int i, bytes;
  int packetSize = sizeof(struct msg_updateorbs) + sizeof(struct dataNodeOrbs) * sdataCountOrbs;
  struct msg_updateorbs *packet = (struct msg_updateorbs *)malloc(packetSize);

  if (VERBOSE) printf("Packet Created: ssend_msg_updateorbs (%d orbs)\n", sdataCountOrbs);
  packet->mtype = MSG_UPDATEORBS;
  packet->obj_count = sdataCountOrbs;
  for (i=0; i < sdataCountOrbs; i++) packet->orbs[i] = *sdataArrayOrbs[i];

  bytes = sendto(mysocket, (const char *)packet, packetSize, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_updateorbs: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_explosion(int mysocket, struct sockaddr_in client_addr, double xcoord, double ycoord) {
  int bytes;
  struct msg_explosion packet;
  // lastcontact = time(NULL);

  if (VERBOSE) printf("Packet Created: ssend_msg_explosion\n");

  // Construct the packet
  packet.mtype = MSG_EXPLOSION;
  packet.xcoord = xcoord;
  packet.ycoord = ycoord;

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_explosion: %d\n", netLastError());
  }
}

void ssend_msg_clientlist(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_clientlist *packet = NULL;
  int packetSize = 0;

  if (VERBOSE) printf("Packet Created: ssend_msg_clientlist\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_clientlist);
  packetSize += sizeof(struct wireClient) * sdataCountClients;
  packet = (struct msg_clientlist *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_CLIENTLIST;
  packet->obj_count = sdataCountClients;
  if (VERBOSE) printf("  packet->mtype = %d\n", packet->mtype);
  if (VERBOSE) printf("  packet->obj_count = %d\n", packet->obj_count);

  for(i=0; i < sdataCountClients; i++) {
    strncpy((packet->clients[i]).username, sdataArrayClients[i]->username, USERNAME_MAX);
    (packet->clients[i]).objectID = sdataArrayClients[i]->objectID;
    if (VERBOSE) printf("  packet->clients[%d].username = '%s'\n", i, (packet->clients[i]).username);
    if (VERBOSE) printf("  packet->clients[%d].objectID = %d\n", i, (packet->clients[i]).objectID);
  }
 
  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)packet,
		 packetSize,
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_clientlist: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_scoreboard(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_scoreboard *packet = NULL;
  int packetSize = 0;

  if (VERBOSE) printf("Packet Created: ssend_msg_scoreboard\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_scoreboard);
  packetSize += sizeof(struct wireScore) * sdataCountClients;
  packet = (struct msg_scoreboard *)calloc(1, packetSize);

  // Populate the packet
  packet->mtype = MSG_SCOREBOARD;
  packet->obj_count = sdataCountClients;
  packet->matchOver = matchOver;
  packet->winner = matchWinner;
  packet->nextMatch = matchOver ? matchSecondsLeft() : 0;
  for(i=0; i < sdataCountClients; i++) {
    (packet->scores[i]).objectID = sdataArrayClients[i]->objectID;
    strncpy((packet->scores[i]).username, sdataArrayClients[i]->username, USERNAME_MAX);
    (packet->scores[i]).score = sdataArrayClients[i]->score;
    (packet->scores[i]).kills = sdataArrayClients[i]->kills;
    (packet->scores[i]).deaths = sdataArrayClients[i]->deaths;
  }

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)packet,
		 packetSize,
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_scoreboard: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_gameoptions(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_gameoptions *packet = NULL;
  int packetSize = 0;

  if (VERBOSE) printf("Packet Created: ssend_msg_gameoptions\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_gameoptions) + sizeof(int32_t) * OPTION_COUNT;
  packet = (struct msg_gameoptions *)calloc(1, packetSize);

  // Populate the packet
  packet->mtype = MSG_GAMEOPTIONS;
  packet->obj_count = OPTION_COUNT;
  for(i=0; i < OPTION_COUNT; i++) packet->values[i] = serverOptions[i];

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)packet,
		 packetSize,
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_gameoptions: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_deleteobj(int mysocket, struct sockaddr_in client_addr, data_t object_type, object_t objectID) {
  int bytes;
  struct msg_deleteobj packet;
  
  if (VERBOSE) printf("Packet Created: ssend_msg_deleteobj\n");

  // Construct the packet
  packet.mtype = MSG_DELETEOBJ;
  packet.object_type = object_type;
  packet.objectID = objectID;

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    fprintf(stderr, "Error: unable to send msg_deleteobj: %d\n", netLastError());
  }
}

// Broadcast Messages
void broadcastUpdatePlayers() {
  int i;

  // Send updates to all players
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_updateplayers(serverSocket, *(sdataArrayClients[i]->client_addr));
  }
}

void broadcastScoreboard() {
  // Called every tick: send when a score changes, and every
  // SCOREBOARD_TICKS anyway in case a packet was lost
  static int ticksSinceSent = 0;
  int i;

  ticksSinceSent++;
  if (!scoreChanged && ticksSinceSent < SCOREBOARD_TICKS) return;
  scoreChanged = 0;
  ticksSinceSent = 0;

  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_scoreboard(serverSocket, *(sdataArrayClients[i]->client_addr));
  }
}

void broadcastUpdateMissiles() {
  int i;

  // Send updates to all players
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_updatemissiles(serverSocket, *(sdataArrayClients[i]->client_addr));
  }
}

// The planet list, to everyone now (a volcano changed)...
void broadcastStaticObjs() {
  int i;
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_staticobjs(serverSocket, *(sdataArrayClients[i]->client_addr));
  }
}

// ...and every STATICOBJS_TICKS, called each tick, in case that was lost
void broadcastStaticObjsRepeat() {
  static int ticks = 0;
  if (++ticks < STATICOBJS_TICKS) return;
  ticks = 0;
  broadcastStaticObjs();
}

void broadcastUpdateAsteroids() {
  int i;
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_updateasteroids(serverSocket, *(sdataArrayClients[i]->client_addr));
  }
}

void broadcastUpdateOrbs() {
  int i;
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_updateorbs(serverSocket, *(sdataArrayClients[i]->client_addr));
  }
}

void broadcastDeleteMissile(object_t objectID) {
  int i;

  // Send updates to all players
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_deleteobj(serverSocket, *(sdataArrayClients[i]->client_addr), OBJECT_MISSILE, objectID);
  }
}

void broadcastDeleteShip(object_t objectID) {
  int i;

  // Send updates to all players
  for (i=0; i < sdataCountClients; i++) {
    ssend_msg_deleteobj(serverSocket, *(sdataArrayClients[i]->client_addr), OBJECT_SHIP, objectID);
  }
}
