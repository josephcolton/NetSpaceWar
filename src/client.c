#include "global.h"
#include "cdata.h"
#include "client.h"

/* Client Updates */
void checkKeepAlive(int mysocket, struct sockaddr_in server_addr) {    
  if (clientCheckin + KEEPALIVE_LIMIT < time(NULL)) {
    // We need to send a keep-alive message
    csend_msg_keepalive(mysocket, server_addr);
  }
}

/* Client Handling Messages */
void chandle_msg_loginack(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  status_t status;
  object_t objectID;

  if (DEBUG) printf("Packet Received: chandle_msg_loginack\n");

  // Verify message type
  mtype = ((struct msg_loginack *)packet)->mtype;
  if (mtype != MSG_LOGINACK) return;

  // Get the other variables
  status = ((struct msg_loginack *)packet)->status;
  objectID = ((struct msg_loginack *)packet)->objectID;

  // Packet contents
  if (DEBUG) printf("  packet->mtype = %d\n", ((struct msg_loginack *)packet)->mtype);
  if (DEBUG) printf("  packet->status = %d\n", ((struct msg_loginack *)packet)->status);
  if (DEBUG) printf("  packet->objectID = %d\n", ((struct msg_loginack *)packet)->objectID);

  // Decide if the loginack is useful
  if (status == STATUS_INUSE) {
    snprintf(clientStatusMessage, LEN, "The name %s is already taken", clientUsername);
    printf("%s\n", clientStatusMessage);
    clientAlive = 0;
    return;
  }
  if (status == STATUS_INVALID) {
    snprintf(clientStatusMessage, LEN, "The server rejected the name %s", clientUsername);
    printf("%s\n", clientStatusMessage);
    clientAlive = 0;
    return;
  }
  if (status == STATUS_FULL) {
    snprintf(clientStatusMessage, LEN, "The server is full");
    printf("%s\n", clientStatusMessage);
    clientAlive = 0;
    return;
  }

  // The username must have been good.
  if (status == STATUS_OKAY) {
    if (clientObjectID != 0) {
      printf("The client already has an ObjectID!\n");
      return;
    }

    // Assign the clientObjectID
    clientObjectID = objectID;

    // The create the client and client ship
    if (DEBUG) printf("  Creating Client\n");
    cdataCreateClient(NULL, clientUsername, objectID);

    // After creating the client, the next thing to do is
    // to start gathering information about the map, and
    // everything else on the map.
    csend_msg_request(mysocket, server_addr, DATA_STATICOBJS);
    // csend_msg_request(mysocket, server_addr, DATA_CLIENTINFO);
    csend_msg_request(mysocket, server_addr, DATA_CLIENTLIST);
    csend_msg_request(mysocket, server_addr, DATA_SCORES);
  } else {
    clientAlive = 0;
  }
  
  return;
}

void chandle_msg_logoutack(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;

  if (DEBUG) printf("Packet Received: chandle_msg_logoutack\n");

  // Verify message type
  mtype = ((struct msg_logoutack *)packet)->mtype;
  if (mtype != MSG_LOGOUTACK) return;

  // This packet indicates that the server has closed the connection.
  printf("  Shutting Down\n");
  clientAlive = 0;
}

void chandle_msg_staticobjs(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  int i, count, status;
  object_t objectID;
  double xcoord, ycoord;

  if (DEBUG) printf("Packet Received: chandle_msg_staticobjs\n");

  // Verify message type
  mtype = ((struct msg_staticobjs *)packet)->mtype;
  if (mtype != MSG_STATICOBJS) return;

  // Packet contents
  if (DEBUG) printf("  packet->mtype = %d\n", ((struct msg_staticobjs *)packet)->mtype);
  if (DEBUG) printf("  packet->obj_count = %d\n", ((struct msg_staticobjs *)packet)->obj_count);
  count = ((struct msg_staticobjs *)packet)->obj_count;
  // Never read past the receive buffer, whatever the packet claims
  if (count < 0) count = 0;
  if (count > MAX_COUNT_PLANETS) count = MAX_COUNT_PLANETS;

  // Gather planets from packet.
  for(i=0; i < count; i++) {
    objectID = ((struct msg_staticobjs *)packet)->planets[i].objectID;
    xcoord = ((struct msg_staticobjs *)packet)->planets[i].xcoord;
    ycoord = ((struct msg_staticobjs *)packet)->planets[i].ycoord;
    if (DEBUG) printf("  packet->planet[%d].objectID = %d\n", i, objectID);
    if (DEBUG) printf("  packet->planet[%d].xcoord = %.2f\n", i, xcoord);
    if (DEBUG) printf("  packet->planet[%d].ycoord = %.2f\n", i, ycoord);
    // Create the planet and check status
    status = cdataCreatePlanet(objectID, xcoord, ycoord);
    if (status == SW_FALSE) {
      if (DEBUG) printf("  ** Problem Creating Planet\n");
    }
  }
}

void chandle_msg_clientinfo(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  object_t objectID;
  double xcoord;
  double ycoord;
  int status;
  int energy;
  int shields;
  double orientation;
  double xVelocity;
  double yVelocity;

  if (DEBUG) printf("Packet Received: chandle_msg_clientinfo\n");

  // Verify message type
  mtype = ((struct msg_clientinfo *)packet)->mtype;
  if (mtype != MSG_CLIENTINFO) return;

  // Get the other variables
  objectID = ((struct msg_clientinfo *)packet)->objectID;
  xcoord = ((struct msg_clientinfo *)packet)->xcoord;
  ycoord = ((struct msg_clientinfo *)packet)->ycoord;
  status = ((struct msg_clientinfo *)packet)->status;
  energy = ((struct msg_clientinfo *)packet)->energy;
  shields = ((struct msg_clientinfo *)packet)->shields;
  orientation = ((struct msg_clientinfo *)packet)->orientation;
  xVelocity = ((struct msg_clientinfo *)packet)->xVelocity;
  yVelocity = ((struct msg_clientinfo *)packet)->yVelocity;

  // Display packet contents
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);
  if (DEBUG) printf("  packet->objectID = %d\n", objectID);
  if (DEBUG) printf("  packet->xcoord = %.2f\n", xcoord);
  if (DEBUG) printf("  packet->ycoord = %.2f\n", ycoord);
  if (DEBUG) printf("  packet->status = %d\n", status);
  if (DEBUG) printf("  packet->energy = %d\n", energy);
  if (DEBUG) printf("  packet->shields = %d\n", shields);
  if (DEBUG) printf("  packet->orientation = %.2f\n", orientation);
  if (DEBUG) printf("  packet->xVelocity = %.2f\n", xVelocity);
  if (DEBUG) printf("  packet->yVelocity = %.2f\n", yVelocity);

  // Create the ship
  cdataCreateShip(objectID, xcoord, ycoord);
}

void chandle_msg_updateplayers(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  int i;
  int count;
  object_t objectID;
  double xcoord, ycoord;
  int status, energy, shields;
  double orientation;
  double xVelocity, yVelocity;
  struct dataNodeShips *updateShip = NULL;

  if (DEBUG) printf("Packet Received: chandle_msg_updateplayers\n");

  // Verify message type
  mtype = ((struct msg_updateplayers *)packet)->mtype;
  if (mtype != MSG_UPDATEPLAYERS) return;
  count = ((struct msg_updateplayers *)packet)->obj_count;
  // Never read past the receive buffer, whatever the packet claims
  if (count < 0) count = 0;
  if (count > MAX_COUNT_SHIPS) count = MAX_COUNT_SHIPS;

  // Packet data
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);
  if (DEBUG) printf("  packet->obj_count = %d\n", count);

  for (i=0; i < count; i++) {
    objectID = (((struct msg_updateplayers *)packet)->ships[i]).objectID;
    xcoord = (((struct msg_updateplayers *)packet)->ships[i]).xcoord;
    ycoord = (((struct msg_updateplayers *)packet)->ships[i]).ycoord;
    status = (((struct msg_updateplayers *)packet)->ships[i]).status;
    energy = (((struct msg_updateplayers *)packet)->ships[i]).energy;
    shields = (((struct msg_updateplayers *)packet)->ships[i]).shields;
    orientation = (((struct msg_updateplayers *)packet)->ships[i]).orientation;
    xVelocity = (((struct msg_updateplayers *)packet)->ships[i]).xVelocity;
    yVelocity = (((struct msg_updateplayers *)packet)->ships[i]).yVelocity;
    // Display ship contents
    if (DEBUG) printf("  packet->ships[%d].objectID = %d\n", i, objectID);
    if (DEBUG) printf("  packet->ships[%d].xcoord = %.2f\n", i, xcoord);
    if (DEBUG) printf("  packet->ships[%d].ycoord = %.2f\n", i, ycoord);
    if (DEBUG) printf("  packet->ships[%d].status = %d\n", i, status);
    if (DEBUG) printf("  packet->ships[%d].energy = %d\n", i, energy);
    if (DEBUG) printf("  packet->ships[%d].shields = %d\n", i, shields);
    if (DEBUG) printf("  packet->ships[%d].orientation = %.2f\n", i, orientation);
    if (DEBUG) printf("  packet->ships[%d].xVelocity = %.2f\n", i, xVelocity);
    if (DEBUG) printf("  packet->ships[%d].yVelocity = %.2f\n", i, yVelocity);
    // See if the ship has been created first
    updateShip = cdataLookupShips(objectID);
    if (updateShip == NULL) {
      // Ship does not yet exist, create it.
      cdataCreateShip(objectID, xcoord, ycoord);
      if (DEBUG) printf("  ship information created\n");
      // Get a pointer for updates
      updateShip = cdataLookupShips(objectID);
    }
    // Update the ship information
    if (updateShip != NULL) {
      updateShip->xcoord = xcoord;
      updateShip->ycoord = ycoord;
      updateShip->status = status;
      updateShip->energy = energy;
      updateShip->shields = shields;
      updateShip->orientation = orientation;
      updateShip->xVelocity = xVelocity;
      updateShip->yVelocity = yVelocity;
      if (DEBUG) printf("  ship information updated\n");
    }
  }
}

void chandle_msg_updatemissiles(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  int i, count;
  object_t objectID;
  double xcoord, ycoord;
  double orientation;
  double xVelocity, yVelocity;
  struct dataNodeMissiles *updateMissile = NULL;

  if (DEBUG) printf("Packet Received: chandle_msg_updatemissiles\n");

  // Verify message type
  mtype = ((struct msg_updatemissiles *)packet)->mtype;
  if (mtype != MSG_UPDATEMISSILES) return;
  count = ((struct msg_updatemissiles *)packet)->obj_count;
  // Never read past the receive buffer, whatever the packet claims
  if (count < 0) count = 0;
  if (count > MAX_COUNT_MISSILES) count = MAX_COUNT_MISSILES;

  // Packet data
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);
  if (DEBUG) printf("  packet->obj_count = %d\n", count);

  for (i=0; i < count; i++) {
    objectID = (((struct msg_updatemissiles *)packet)->missiles[i]).objectID;
    xcoord = (((struct msg_updatemissiles *)packet)->missiles[i]).xcoord;
    ycoord = (((struct msg_updatemissiles *)packet)->missiles[i]).ycoord;
    orientation = (((struct msg_updatemissiles *)packet)->missiles[i]).orientation;
    xVelocity = (((struct msg_updatemissiles *)packet)->missiles[i]).xVelocity;
    yVelocity = (((struct msg_updatemissiles *)packet)->missiles[i]).yVelocity;

    // Display ship contents                                                                        
    if (DEBUG) printf("  packet->missiles[%d].objectID = %d\n", i, objectID);
    if (DEBUG) printf("  packet->missiles[%d].xcoord = %.2f\n", i, xcoord);
    if (DEBUG) printf("  packet->missiles[%d].ycoord = %.2f\n", i, ycoord);
    if (DEBUG) printf("  packet->missiles[%d].orientation = %.2f\n", i, orientation);
    if (DEBUG) printf("  packet->missiles[%d].xVelocity = %.2f\n", i, xVelocity);
    if (DEBUG) printf("  packet->missiles[%d].yVelocity = %.2f\n", i, yVelocity);

    // See if the missile already exists
    updateMissile = cdataLookupMissiles(objectID);
    // Create the missile if the pointer is NULL
    if (updateMissile == NULL) {
      cdataCreateMissile(objectID, xcoord, ycoord, orientation, xVelocity, yVelocity);
      if (DEBUG) printf("  missile information created\n");
      // Get a pointer for updates
      updateMissile = cdataLookupMissiles(objectID);
    }
    // Update the ship information
    if (updateMissile != NULL) {
      updateMissile->xcoord = xcoord;
      updateMissile->ycoord = ycoord;
      updateMissile->orientation = orientation;
      updateMissile->xVelocity = xVelocity;
      updateMissile->yVelocity = yVelocity;
      if (DEBUG) printf("  missile information updated\n");
    }
  }
}

void chandle_msg_explosion(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  double xcoord, ycoord;
  int status;

  if (DEBUG) printf("Packet Received: chandle_msg_explosion\n");

  // Verify message type
  mtype = ((struct msg_explosion *)packet)->mtype;
  if (mtype != MSG_EXPLOSION) return;

  // Get the other variables
  xcoord = ((struct msg_explosion *)packet)->xcoord;
  ycoord = ((struct msg_explosion *)packet)->ycoord;

  // Packet data
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);
  if (DEBUG) printf("  packet->xcoord = %.2f\n", xcoord);
  if (DEBUG) printf("  packet->ycoord = %.2f\n", ycoord);

  // Create the explosion node
  status = cdataCreateExplosion(xcoord, ycoord);
  if (status == SW_FALSE) {
    if (DEBUG) printf("  ** Problem Creating Explosion\n");
  }
}

void chandle_msg_clientlist(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  int i, count;
  object_t objectID;
  char username[USERNAME_MAX];

  if (DEBUG) printf("Packet Received: chandle_msg_clientlist\n");

  // Verify message type
  mtype = ((struct msg_clientlist *)packet)->mtype;
  if (mtype != MSG_CLIENTLIST) return;

  // Packet contents
  if (DEBUG) printf("  packet->mtype = %d\n", ((struct msg_clientlist *)packet)->mtype);
  if (DEBUG) printf("  packet->obj_count = %d\n", ((struct msg_clientlist *)packet)->obj_count);
  count = ((struct msg_clientlist *)packet)->obj_count;
  // Never read past the receive buffer, whatever the packet claims
  if (count < 0) count = 0;
  if (count > MAX_COUNT_CLIENTS) count = MAX_COUNT_CLIENTS;

  // Gather clients from packet.
  for(i=0; i < count; i++) {
    objectID = ((struct msg_clientlist *)packet)->clients[i].objectID;
    strncpy(username, ((struct msg_clientlist *)packet)->clients[i].username, USERNAME_MAX);
    username[USERNAME_MAX - 1] = 0; // The packet's copy may not be terminated
    if (DEBUG) printf("  packet->clients[%d].objectID = %d\n", i, objectID);
    if (DEBUG) printf("  packet->clients[%d].username = '%s'\n", i, username);
    // Create the client
    cdataCreateClient(NULL, username, objectID);
  }
}

void chandle_msg_scoreboard(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;

  if (DEBUG) printf("Packet Received: chandle_msg_scoreboard\n");

  // Verify message type
  mtype = ((struct msg_scoreboard *)packet)->mtype;
  if (mtype != MSG_SCOREBOARD) return;

  // Get the other variables
  // Do something useful
}

void chandle_msg_deleteobj(int mysocket, void *packet, struct sockaddr_in server_addr) {
  message_t mtype;
  data_t object_type;
  object_t objectID;

  if (DEBUG) printf("Packet Received: chandle_msg_deleteobj\n");

  // Verify message type
  mtype = ((struct msg_deleteobj *)packet)->mtype;
  if (mtype != MSG_DELETEOBJ) return;
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);

  // Packet contents
  object_type = ((struct msg_deleteobj *)packet)->object_type;
  objectID = ((struct msg_deleteobj *)packet)->objectID;
  if (DEBUG) printf("  packet->object_type = %d\n", object_type);
  if (DEBUG) printf("  packet->objectID = %d\n", objectID);

  // Determine the type of object to delete
  if (object_type == OBJECT_SHIP) {
    cdataDeleteShip(objectID);
  }
  if (object_type == OBJECT_MISSILE) {
    cdataDeleteMissile(objectID);
  }
  if (object_type == OBJECT_PLANET) {
    cdataDeletePlanet(objectID);
  }
}

/* Client Sending Messages */
void csend_msg_login(int mysocket, struct sockaddr_in server_addr, char *username) {
  int bytes;
  struct msg_login packet;
  // lastcontact = time(NULL);

  if (DEBUG) printf("Packet Created: csend_msg_login\n");

  // Construct the packet
  packet.mtype = MSG_LOGIN;
  strncpy(packet.username, username, USERNAME_MAX);

  // Packet contents
  if (DEBUG) printf("  packet.mtype = %d\n", packet.mtype);
  if (DEBUG) printf("  packet.username = '%s'\n", packet.username);

  // Send the packet
  clientCheckin = time(NULL);
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&server_addr,
		 sizeof(server_addr));

  // See if we got any errors
  if (bytes < 0) {
    printf("Error: unable to send msg_login: %d\n", netLastError());
  }
}

void csend_msg_logout(int mysocket, struct sockaddr_in server_addr) {
  int bytes;
  struct msg_logout packet;
  // lastcontact = time(NULL);

  if (DEBUG) printf("Packet Created: csend_msg_logout\n");

  // Construct the packet
  packet.mtype = MSG_LOGOUT;

  // Packet contents
  if (DEBUG) printf("  packet.mtype = %d\n", packet.mtype);

  // Send the packet
  clientCheckin = time(NULL);
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&server_addr,
		 sizeof(server_addr));

  // See if we got any errors
  if (bytes < 0) {
    printf("Error: unable to send msg_logout: %d\n", netLastError());
  }
}

void csend_msg_keepalive(int mysocket, struct sockaddr_in server_addr) {
  int bytes;
  struct msg_keepalive packet;
  // lastcontact = time(NULL);

  if (DEBUG) printf("Packet Created: csend_msg_keepalive\n");

  // Construct the packet
  packet.mtype = MSG_KEEPALIVE;

  // Packet contents
  if (DEBUG) printf("  packet.mtype = %d\n", packet.mtype);

  // Send the packet
  clientCheckin = time(NULL);
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&server_addr,
		 sizeof(server_addr));

  // See if we got any errors
  if (bytes < 0) {
    printf("Error: unable to send msg_keepalive: %d\n", netLastError());
  }
}

void csend_msg_request(int mysocket, struct sockaddr_in server_addr, data_t dtype) {
  int bytes;
  struct msg_request packet;
  // lastcontact = time(NULL);

  if (DEBUG) printf("Packet Created: csend_msg_request\n");

  // Construct the packet
  packet.mtype = MSG_REQUEST;
  packet.dtype = dtype;

  // Packet contents
  if (DEBUG) printf("  packet.mtype = %d\n", packet.mtype);
  if (DEBUG) printf("  packet.dtype = %d\n", packet.dtype);

  // Send the packet
  clientCheckin = time(NULL);
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&server_addr,
		 sizeof(server_addr));

  // See if we got any errors
  if (bytes < 0) {
    printf("Error: unable to send msg_request: %d\n", netLastError());
  }
}

void csend_msg_clientupdate(int mysocket, struct sockaddr_in server_addr, action_t action, double var1, double var2, int var3) {
  int bytes;
  struct msg_clientupdate packet;
  // lastcontact = time(NULL);

  if (DEBUG) printf("Packet Created: csend_msg_clientupdate\n");

  // Construct the packet
  packet.mtype = MSG_CLIENTUPDATE;
  packet.action = action;
  packet.var1 = var1;
  packet.var2 = var2;
  packet.var3 = var3;

  // Packet contents
  if (DEBUG) printf("  packet.mtype = %d\n", packet.mtype);
  if (DEBUG) printf("  packet.action = %d\n", packet.action);
  if (DEBUG) printf("  packet.var1 = %.2f\n", packet.var1);
  if (DEBUG) printf("  packet.var2 = %.2f\n", packet.var2);
  if (DEBUG) printf("  packet.var3 = %d\n", packet.var3);

  // Send the packet
  clientCheckin = time(NULL);
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&server_addr,
		 sizeof(server_addr));

  // See if we got any errors
  if (bytes < 0) {
    printf("Error: unable to send msg_clientupdate: %d\n", netLastError());
  }
}


/* Connection helpers shared by the game client and the AI */
double clientLastPacketTime = 0.0;
static double clientLastLoginTime = 0.0;

int clientOpenSocket(const char *hostname, int port, struct sockaddr_in *server_addr) {
  int mysocket;

  if (netInitialize() != 0) {
    snprintf(clientStatusMessage, LEN, "Unable to start networking");
    return -1;
  }

  memset(server_addr, 0, sizeof(*server_addr));
  server_addr->sin_family = AF_INET;
  server_addr->sin_port = htons(port);
  if (netResolveHost(hostname, &server_addr->sin_addr) != 0) {
    snprintf(clientStatusMessage, LEN, "Unable to find the server %s", hostname);
    return -1;
  }
  if (DEBUG) printf("Server IP Address: %s port %d\n", inet_ntoa(server_addr->sin_addr), port);

  mysocket = (int)socket(AF_INET, SOCK_DGRAM, 0);
  if (mysocket < 0) {
    snprintf(clientStatusMessage, LEN, "Unable to create a socket: %d", netLastError());
    return -1;
  }
  netIgnoreConnReset(mysocket);

  // Starts the timeouts that clientConnectionProblem() checks
  clientLastPacketTime = platformNow();
  clientLastLoginTime = 0.0;
  return mysocket;
}

message_t clientReceive(int mysocket) {
  int status;
  void *packet;
  message_t mtype;
  struct sockaddr_in server_addr;
  socklen_t addrlen = sizeof(server_addr);

  packet = calloc(1, MAX_PACKET);
  memset(&server_addr, 0, sizeof(server_addr));
  status = recvfrom(mysocket, packet, MAX_PACKET, 0, (struct sockaddr *)&server_addr, &addrlen);

  // Ignore failed reads and packets too short to have a message type
  if (status < (int)sizeof(message_t)) {
    free(packet);
    return MSG_NONE;
  }
  clientLastPacketTime = platformNow();

  // The handler must gather everything it needs before the packet is freed
  mtype = ((struct msg_basic *)packet)->mtype;
  if (mtype == MSG_LOGINACK)       chandle_msg_loginack(mysocket, packet, server_addr);
  if (mtype == MSG_LOGOUTACK)      chandle_msg_logoutack(mysocket, packet, server_addr);
  if (mtype == MSG_STATICOBJS)     chandle_msg_staticobjs(mysocket, packet, server_addr);
  if (mtype == MSG_CLIENTINFO)     chandle_msg_clientinfo(mysocket, packet, server_addr);
  if (mtype == MSG_UPDATEPLAYERS)  chandle_msg_updateplayers(mysocket, packet, server_addr);
  if (mtype == MSG_UPDATEMISSILES) chandle_msg_updatemissiles(mysocket, packet, server_addr);
  if (mtype == MSG_EXPLOSION)      chandle_msg_explosion(mysocket, packet, server_addr);
  if (mtype == MSG_CLIENTLIST)     chandle_msg_clientlist(mysocket, packet, server_addr);
  if (mtype == MSG_SCOREBOARD)     chandle_msg_scoreboard(mysocket, packet, server_addr);
  if (mtype == MSG_DELETEOBJ)      chandle_msg_deleteobj(mysocket, packet, server_addr);

  free(packet);
  return mtype;
}

void clientRetryLogin(int mysocket, struct sockaddr_in server_addr) {
  // UDP can lose the login or its reply, and the server may still be
  // starting up, so keep asking until we have an object ID
  double now = platformNow();
  if (clientObjectID != 0) return;
  if (now - clientLastLoginTime < LOGIN_RETRY) return;
  csend_msg_login(mysocket, server_addr, clientUsername);
  clientLastLoginTime = now;
}

int clientConnectionProblem(double startTime) {
  double now = platformNow();
  if (clientObjectID == 0 && now - startTime > CONNECT_TIMEOUT) {
    if (clientStatusMessage[0] == 0) snprintf(clientStatusMessage, LEN, "No answer from the server");
    return 1;
  }
  if (clientObjectID != 0 && now - clientLastPacketTime > SERVER_SILENCE) {
    snprintf(clientStatusMessage, LEN, "Lost connection to the server");
    return 1;
  }
  return 0;
}
