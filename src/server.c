#include "global.h"
#include "server.h"
#include "sdata.h"

// Initialize the server
void serverInitialize() {
  double xcoord, ycoord;
  int i;
  if (DEBUG) printf("Initializing Server\n");

  // Seed the randomizer
  srand(time(NULL));

  // Initialize sdata
  sdataInitialize();

  // Once the data has been initialized, we need to create
  // the map.
  for (i=0; i < PLANETS; i++) {
    xcoord = randRange(SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
    ycoord = randRange(SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
    sdataCreatePlanet(sdataNewObjectID(), xcoord, ycoord);
  }
}

// Client Keepalive Checks
void checkClientKeepAlive() {
  int i;
  time_t curTime = time(NULL);

  for(i=0; i < sdataCountClients; i++) {
    if (sdataArrayClients[i]->checkin + (KEEPALIVE_LIMIT * 3) < curTime) {
      printf("Client checkin timeout\n");
      // Client has not reported in the last KEEPALIVE_LIMIT * 3 seconds
      // Tell the client to log out
      ssend_msg_logoutack(serverSocket, *(sdataArrayClients[i]->client_addr));
      // Delete the client node
      sdataDeleteClient(sdataArrayClients[i]->objectID);
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
  double xcoord, ycoord;

  if (DEBUG) printf("Packet Received: shandle_msg_login\n");

  // Verify message type
  mtype = ((struct msg_login *)packet)->mtype;
  if (mtype != MSG_LOGIN) return;
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);

  // Get the other variables
  strncpy(username, ((struct msg_login *)packet)->username, USERNAME_MAX);
  username[USERNAME_MAX - 1] = 0; // The packet's copy may not be terminated
  if (DEBUG) printf("  packet->username = '%s'\n", username);

  // Additional details
  if (DEBUG) printf("  Processing:\n");

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
    if (DEBUG) printf("    Client already exists\n");
    ssend_msg_loginack(mysocket, client_addr, STATUS_INUSE, objectID);
    return;
  } else {
    if (DEBUG) printf("    Client does not yet exist.\n");
  }

  // Generate a objectID for the client
  objectID = sdataNewObjectID();
  if (DEBUG) printf("    Assigning objectID = %d\n", objectID);

  // Create the client node
  if (!sdataCreateClient(username, objectID, client_addr)) {
    ssend_msg_loginack(mysocket, client_addr, STATUS_FULL, 0);
    return;
  }

  // Create a ship to go with the client
  xcoord = randRange(SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
  ycoord = randRange(SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
  sdataCreateShip(objectID, xcoord, ycoord);

  // Notify the client of their objectID
  ssend_msg_loginack(mysocket, client_addr, STATUS_OKAY, objectID);
}

void shandle_msg_logout(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;
  object_t objectID;

  if (DEBUG) printf("Packet Received: shandle_msg_logout\n");

  // Verify message type
  mtype = ((struct msg_logout *)packet)->mtype;
  if (mtype != MSG_LOGOUT) return;

  // Display packet contents
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);

  // Confirm logout with client
  ssend_msg_logoutack(serverSocket, client_addr);

  // Delete the client node
  objectID = sdataGetClientIDbyAddress(client_addr);
  sdataDeleteClient(objectID);
}

void shandle_msg_keepalive(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;

  if (DEBUG) printf("Packet Received: shandle_msg_keepalive\n");

  // Verify message type
  mtype = ((struct msg_keepalive *)packet)->mtype;
  if (mtype != MSG_KEEPALIVE) return;

  // Display packet contents
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);
}

void shandle_msg_request(int mysocket, void *packet, struct sockaddr_in client_addr) {
  message_t mtype;
  data_t dtype;

  if (DEBUG) printf("Packet Received: shandle_msg_request\n");

  // Verify message type
  mtype = ((struct msg_request *)packet)->mtype;
  if (mtype != MSG_REQUEST) return;

  // Get the other variables
  dtype = ((struct msg_request *)packet)->dtype;

  // Display packet contents
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);
  if (DEBUG) printf("  packet->dtype = %d\n", dtype);

  // Reply to client request for data
  if (dtype == DATA_STATICOBJS) ssend_msg_staticobjs(mysocket, client_addr);
  if (dtype == DATA_CLIENTINFO) ssend_msg_clientinfo(mysocket, client_addr);
  if (dtype == DATA_CLIENTLIST) ssend_msg_clientlist(mysocket, client_addr);
  if (dtype == DATA_SCORES) ssend_msg_scoreboard(mysocket, client_addr);
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

  if (DEBUG) printf("Packet Received: shandle_msg_clientupdate\n");

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
  if (DEBUG) printf("  packet->mtype = %d\n", mtype);
  if (DEBUG) printf("  packet->action = %d\n", action);
  if (DEBUG) printf("  packet->var1 = %.2f\n", var1);
  if (DEBUG) printf("  packet->var2 = %.2f\n", var2);
  if (DEBUG) printf("  packet->var3 = %d\n", var3);

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


    // Calculate the xVelocity and yVelocity based on current values
    // and the orientation of the ship.
    if (clientShip->xVelocity*clientShip->xVelocity+clientShip->yVelocity*clientShip->xVelocity>SHIP_MAX_VELOCITY*SHIP_MAX_VELOCITY)
    {
      // If the client is going too fast slow it down?
    }

  }
  
  if (action == ACTION_FIRE) {
    // Verify that the ship has the energy
    if (clientShip->energy < MISSILE_COST) return;
    clientShip->energy -= MISSILE_COST;

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
    if (clientShip->energy >= JUMP_COST || clientShip->status == 0)
    {
      // Put the ship in a new location
      clientShip->xcoord = randRange(SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
      clientShip->ycoord = randRange(SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
      // Stop the ship from moving
      clientShip->xVelocity = 0.0;
      clientShip->yVelocity = 0.0;
      // Use the energy cost
      clientShip->energy -= JUMP_COST;
    }    

    // if the ship is dead, we need to bring it back to life
    if (clientShip->status == 0) {
      clientShip->status = 1;
      clientShip->energy = SHIP_MAX_ENERGY;
      clientShip->shields = SHIP_MAX_SHIELDS;
      // The penalty for dying was taken when the ship was destroyed
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

  if (DEBUG) printf("Packet Created: ssend_msg_loginack\n");

  // Construct the packet
  packet.mtype = MSG_LOGINACK;
  packet.status = status;
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
    printf("Error: unable to send msg_loginack: %d\n", netLastError());
  }
}

void ssend_msg_logoutack(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  struct msg_logoutack packet;
  // lastcontact = time(NULL);

  if (DEBUG) printf("Packet Created: ssend_msg_logoutack\n");

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
    printf("Error: unable to send msg_logoutack: %d\n", netLastError());
  }
}

void ssend_msg_staticobjs(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_staticobjs *packet = NULL;
  int packetSize = 0;

  if (DEBUG) printf("Packet Created: ssend_msg_staticobjs\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_staticobjs);
  packetSize += sizeof(struct dataNodePlanets) * sdataCountPlanets;
  packet = (struct msg_staticobjs *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_STATICOBJS;
  packet->obj_count = sdataCountPlanets;
  if (DEBUG) printf("  packet->mtype = %d\n", packet->mtype);
  if (DEBUG) printf("  packet->obj_count = %d\n", packet->obj_count);
  for(i=0; i < sdataCountPlanets; i++) {
    (packet->planets[i]).objectID = sdataArrayPlanets[i]->objectID;
    (packet->planets[i]).xcoord = sdataArrayPlanets[i]->xcoord;
    (packet->planets[i]).ycoord = sdataArrayPlanets[i]->ycoord;
    if (DEBUG) printf("  packet->planet[%d].objectID = %d\n", i, (packet->planets[i]).objectID);
    if (DEBUG) printf("  packet->planet[%d].xcoord = %.2f\n", i, (packet->planets[i]).xcoord);
    if (DEBUG) printf("  packet->planet[%d].ycoord = %.2f\n", i, (packet->planets[i]).ycoord);
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
    printf("Error: unable to send msg_staticobjs: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_clientinfo(int mysocket, struct sockaddr_in client_addr) {
  // Client has requested personal information about itself.
  // This packet is to inform the client about itself.
  int bytes;
  struct msg_clientinfo packet;
  struct dataNodeShips *clientShip = NULL;

  if (DEBUG) printf("Packet Created: ssend_msg_clientinfo\n");

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
  if (DEBUG) printf("  packet.mtype = %d\n", packet.mtype);
  if (DEBUG) printf("  packet.objectID = %d\n", packet.objectID);
  if (DEBUG) printf("  packet.xcoord = %.2f\n", packet.xcoord);
  if (DEBUG) printf("  packet.ycoord = %.2f\n", packet.ycoord);
  if (DEBUG) printf("  packet.status = %d\n", packet.status);
  if (DEBUG) printf("  packet.energy = %d\n", packet.energy);
  if (DEBUG) printf("  packet.shields = %d\n", packet.shields);
  if (DEBUG) printf("  packet.orientation = %.2f\n", packet.orientation);
  if (DEBUG) printf("  packet.xVelocity = %.2f\n", packet.xVelocity);
  if (DEBUG) printf("  packet.yVelocity = %.2f\n", packet.yVelocity);

  // Send the packet
  bytes = sendto(mysocket,
		 (const char *)&packet,
		 sizeof(packet),
		 0,
		 (struct sockaddr *)&client_addr,
		 sizeof(client_addr));

  // See if we got any errors
  if (bytes < 0) {
    printf("Error: unable to send msg_clientinfo: %d\n", netLastError());
  }
}

void ssend_msg_updateplayers(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_updateplayers *packet = NULL;
  int packetSize = 0;

  if (DEBUG) printf("Packet Created: ssend_msg_updateplayers\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_updateplayers);
  packetSize += sizeof(struct dataNodeShips) * sdataCountShips;
  packet = (struct msg_updateplayers *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_UPDATEPLAYERS;
  if (DEBUG) printf("  packet->mtype = %d\n", packet->mtype);
  packet->obj_count = sdataCountShips;
  if (DEBUG) printf("  packet->obj_count = %d\n", packet->obj_count);

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
    if (DEBUG) printf("  packet->ships[%d].objectID = %d\n", i, (packet->ships[i]).objectID);
    if (DEBUG) printf("  packet->ships[%d].xcoord = %.2f\n", i, (packet->ships[i]).xcoord);
    if (DEBUG) printf("  packet->ships[%d].ycoord = %.2f\n", i, (packet->ships[i]).ycoord);
    if (DEBUG) printf("  packet->ships[%d].status = %d\n", i, (packet->ships[i]).status);
    if (DEBUG) printf("  packet->ships[%d].energy = %d\n", i, (packet->ships[i]).energy);
    if (DEBUG) printf("  packet->ships[%d].shields = %d\n", i, (packet->ships[i]).shields);
    if (DEBUG) printf("  packet->ships[%d].orientation = %.2f\n", i, (packet->ships[i]).orientation);
    if (DEBUG) printf("  packet->ships[%d].xVelocity = %.2f\n", i, (packet->ships[i]).xVelocity);
    if (DEBUG) printf("  packet->ships[%d].yVelocity = %.2f\n", i, (packet->ships[i]).yVelocity);
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
    printf("Error: unable to send msg_updateplayers: %d\n", netLastError());
  }



  free(packet);
}

void ssend_msg_updatemissiles(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_updatemissiles *packet = NULL;
  int packetSize = 0;

  if (DEBUG) printf("Packet Created: ssend_msg_updatemissiles\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_updatemissiles);
  packetSize += sizeof(struct dataNodeMissiles) * sdataCountMissiles;
  packet = (struct msg_updatemissiles *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_UPDATEMISSILES;
  if (DEBUG) printf("  packet->mtype = %d\n", packet->mtype);
  packet->obj_count = sdataCountMissiles;
  if (DEBUG) printf("  packet->obj_count = %d\n", packet->obj_count);

  for(i=0; i < sdataCountMissiles; i++) {
    (packet->missiles[i]).objectID = sdataArrayMissiles[i]->objectID;
    (packet->missiles[i]).xcoord = sdataArrayMissiles[i]->xcoord;
    (packet->missiles[i]).ycoord = sdataArrayMissiles[i]->ycoord;
    (packet->missiles[i]).orientation = sdataArrayMissiles[i]->orientation;
    (packet->missiles[i]).xVelocity = sdataArrayMissiles[i]->xVelocity;
    (packet->missiles[i]).yVelocity = sdataArrayMissiles[i]->yVelocity;
    // Display ship contents
    if (DEBUG) printf("  packet->missiles[%d].objectID = %d\n", i, (packet->missiles[i]).objectID);
    if (DEBUG) printf("  packet->missiles[%d].xcoord = %.2f\n", i, (packet->missiles[i]).xcoord);
    if (DEBUG) printf("  packet->missiles[%d].ycoord = %.2f\n", i, (packet->missiles[i]).ycoord);
    if (DEBUG) printf("  packet->missiles[%d].orientation = %.2f\n", i, (packet->missiles[i]).orientation);
    if (DEBUG) printf("  packet->missiles[%d].xVelocity = %.2f\n", i, (packet->missiles[i]).xVelocity);
    if (DEBUG) printf("  packet->missiles[%d].yVelocity = %.2f\n", i, (packet->missiles[i]).yVelocity);
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
    printf("Error: unable to send msg_updatemissiles: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_explosion(int mysocket, struct sockaddr_in client_addr, double xcoord, double ycoord) {
  int bytes;
  struct msg_explosion packet;
  // lastcontact = time(NULL);

  if (DEBUG) printf("Packet Created: ssend_msg_explosion\n");

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
    printf("Error: unable to send msg_explosion: %d\n", netLastError());
  }
}

void ssend_msg_clientlist(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_clientlist *packet = NULL;
  int packetSize = 0;

  if (DEBUG) printf("Packet Created: ssend_msg_clientlist\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_clientlist);
  packetSize += sizeof(struct wireClient) * sdataCountClients;
  packet = (struct msg_clientlist *)malloc(packetSize);

  // Populate the packet
  packet->mtype = MSG_CLIENTLIST;
  packet->obj_count = sdataCountClients;
  if (DEBUG) printf("  packet->mtype = %d\n", packet->mtype);
  if (DEBUG) printf("  packet->obj_count = %d\n", packet->obj_count);

  for(i=0; i < sdataCountClients; i++) {
    strncpy((packet->clients[i]).username, sdataArrayClients[i]->username, USERNAME_MAX);
    (packet->clients[i]).objectID = sdataArrayClients[i]->objectID;
    if (DEBUG) printf("  packet->clients[%d].username = '%s'\n", i, (packet->clients[i]).username);
    if (DEBUG) printf("  packet->clients[%d].objectID = %d\n", i, (packet->clients[i]).objectID);
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
    printf("Error: unable to send msg_clientlist: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_scoreboard(int mysocket, struct sockaddr_in client_addr) {
  int bytes;
  int i;
  struct msg_scoreboard *packet = NULL;
  int packetSize = 0;

  if (DEBUG) printf("Packet Created: ssend_msg_scoreboard\n");

  // Determine packet size and create packet
  packetSize = sizeof(struct msg_scoreboard);
  packetSize += sizeof(struct wireScore) * sdataCountClients;
  packet = (struct msg_scoreboard *)calloc(1, packetSize);

  // Populate the packet
  packet->mtype = MSG_SCOREBOARD;
  packet->obj_count = sdataCountClients;
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
    printf("Error: unable to send msg_scoreboard: %d\n", netLastError());
  }
  free(packet);
}

void ssend_msg_deleteobj(int mysocket, struct sockaddr_in client_addr, data_t object_type, object_t objectID) {
  int bytes;
  struct msg_deleteobj packet;
  
  if (DEBUG) printf("Packet Created: ssend_msg_deleteobj\n");

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
    printf("Error: unable to send msg_deleteobj: %d\n", netLastError());
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
