
#include "global.h"
#include "server.h"
#include "sdata.h"
#include "physics.h"

void handle_socket(int mysocket) {
  int status;
  void *packet;
  message_t mtype;
  struct sockaddr_in client_addr;
  socklen_t addrlen;
  object_t objectID;
  struct dataNodeClients *clientNode = NULL;

  // Malloc the packet
  packet = calloc(1, MAX_PACKET);

  // Erase the data in the client_addr just for fun
  memset(&client_addr, 0, sizeof(client_addr));

  // Set the addrlen size, if not set, we get all kinds of strange addresses
  addrlen = sizeof(client_addr);

  // Get the packet
  status = recvfrom(mysocket,
		    packet,
		    MAX_PACKET,
		    0,
		    (struct sockaddr *)&client_addr,
		    &addrlen);

  // Ignore failed reads and packets too short to have a message type
  if (status < (int)sizeof(message_t)) {
    free(packet);
    return;
  }

  // Now we should have the packet, identify it.
  mtype = ((struct msg_basic *)packet)->mtype;

  // Update checkin time
  objectID = sdataGetClientIDbyAddress(client_addr);
  clientNode = sdataLookupClient(objectID);
  if (clientNode != NULL) {
    clientNode->checkin = time(NULL);
  }

  // We need to call the correct function.  This function needs to handle
  // gathering all data from the packet because it will be freed once
  // the function has completed.
  if (mtype == MSG_LOGIN) shandle_msg_login(mysocket, packet, client_addr);
  if (mtype == MSG_LOGOUT) shandle_msg_logout(mysocket, packet, client_addr);
  if (mtype == MSG_KEEPALIVE) shandle_msg_keepalive(mysocket, packet, client_addr);
  if (mtype == MSG_REQUEST) shandle_msg_request(mysocket, packet, client_addr);
  if (mtype == MSG_CLIENTUPDATE) shandle_msg_clientupdate(mysocket, packet, client_addr);

  // Free the packet
  free(packet);
}

int main(int argc, char **argv) {
  // Character array versions of server hostname and ip address
  char server_name[LEN];
  char server_ipaddr[LEN];
  // Get host by name return data
  struct hostent *lookup;
  // Server listen port number
  int server_port;
  // Collect various return values
  int status;
  // Local bind socket struct
  struct sockaddr_in mysocket_addr;
  // Keep track of time
  struct timeval timeout;
  int timeLeft = SERVER_TIMEOUT;

  // File Descriptors for the select command
  fd_set readfds; // file descriptor set

  if (argc != 3) {
    printf("Usage:\n\t%s hostname port\n\n", argv[0]);
    return 0;
  }

  // Get the servername and ip address
  strcpy(server_name, argv[1]);
  // Start the socket library (needed on Windows before any network call)
  if (netInitialize() != 0) {
    printf("Unable to initialize networking\n");
    exit(1);
  }
  lookup = gethostbyname(server_name);
  if (lookup == NULL) {
    printf("gethostbyname(%s) failed\n", server_name);
    exit(1);
  } else {
    // printf("Server: %s\n", lookup->h_name);
    strcpy(server_ipaddr, inet_ntoa(*(struct in_addr*)lookup->h_addr));
    if (DEBUG) printf("IP Address: %s\n", server_ipaddr);
  }

  // Get the server port number
  server_port = atoi(argv[2]);
  if (DEBUG) printf("Server Port: %d\n", server_port);

  // Initialize mysocket Socket
  // printf("mysocket: creating mysocket socket\n");
  serverSocket = socket (AF_INET, SOCK_DGRAM, 0);
  // mysocket socket id will be negative on error
  if (serverSocket < 0) {
    printf("mysocket: socket creation failed: %d", netLastError());
    exit(0);
  }
  netIgnoreConnReset(serverSocket);

  // Bind to the local socket
  // printf("mysocket: binding mysocket\n");
  // Erase the data in mysocket_addr struct
  memset(&mysocket_addr, 0, sizeof(mysocket_addr));
  // Initialize the data in mysocket_addr
  mysocket_addr.sin_family = AF_INET;
  mysocket_addr.sin_addr.s_addr = inet_addr(server_ipaddr);
  mysocket_addr.sin_port = htons(server_port);
  // Bind the socket to the address
  status = bind(serverSocket,
                (struct sockaddr *)&mysocket_addr,
                sizeof(mysocket_addr));
  // Report if we have an error
  if (status < 0) {
    printf("mysocket: bind failed: %d\n", netLastError());
    exit(0);
  }

  // Initialize the server
  serverInitialize();

  // infine listening loop
  if (DEBUG) printf("Ready for Connections\n");

  while(1) {
    fflush(stdout);
    // Add mysocket socket file descriptor to select set
    FD_ZERO (&readfds); // initialize set
    FD_SET (serverSocket, &readfds); // Add mysocket port to set

    // Set the timeout for reading
    timeLeft = lastTimeLeft(timeLeft);
    timeout.tv_sec = 0;
    timeout.tv_usec = timeLeft;

    // Status lets me know what is ready for reading, if anything
    status = select (serverSocket+1, &readfds, NULL, NULL, &timeout);

    // Select failure
    if (status < 0) {
      printf("select failed: %d\n", netLastError());
    }

    // Timeout
    if (status == 0) {
      // Start timing the next tick from now
      timeLeft = SERVER_TIMEOUT;
      lastTime = platformNow();

      // Do the following everytime anything happens
      // Check client live status
      checkClientKeepAlive();
      // sdata update
      sdataUpdateAll();
      // Update physics
      physicsUpdate();
      // Broadcast updates
      broadcastUpdatePlayers();
      broadcastUpdateMissiles();
    }

    // Packet Received
    if (status > 0) {
      if (FD_ISSET (serverSocket, &readfds)) {
	// The socket is ready to read because we have a packet
	// from a client program.
        handle_socket(serverSocket);
      }
    }

  }

  return 0;
}
