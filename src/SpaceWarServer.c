
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
  // Address to listen on
  struct in_addr listen_addr;
  // Server listen port number
  int server_port;
  // Collect various return values
  int status;
  // Local bind socket struct
  struct sockaddr_in mysocket_addr;
  // Keep track of time
  struct timeval timeout;
  int timeLeft = SERVER_TIMEOUT;
  int i, n;

  // File Descriptors for the select command
  fd_set readfds; // file descriptor set

  // --verbose and --silent can go anywhere after the port; the rest are game rules
  for (i=3, n=3; i < argc; i++) {
    if (strcmp(argv[i], "--verbose") == 0) serverLogLevel = LOG_VERBOSE;
    else if (strcmp(argv[i], "--silent") == 0) serverLogLevel = LOG_SILENT;
    else argv[n++] = argv[i];
  }
  if (argc > 3) argc = n;

  // Game rules: the defaults, then any --option value pairs after the port
  gameOptionsReset(serverOptions);
  if (argc < 3 || !gameOptionsParse(argc, argv, 3, serverOptions)) {
    printf("Usage:\n\t%s address port [--verbose | --silent] [--option value ...]\n\n", argv[0]);
    printf("Use 0.0.0.0 as the address to accept players on every network interface.\n");
    printf("Normally the server reports joins, leaves, deaths and respawns. --verbose\n");
    printf("also prints every packet; --silent prints nothing.\n\n");
    gameOptionsUsage();
    return argc < 3 ? 0 : 1;
  }

  // Start the socket library (needed on Windows before any network call)
  if (netInitialize() != 0) {
    fprintf(stderr, "Unable to initialize networking\n");
    exit(1);
  }

  // Find the address to listen on; 0.0.0.0 listens on every interface
  if (netResolveHost(argv[1], &listen_addr) != 0) {
    fprintf(stderr, "Unable to look up %s\n", argv[1]);
    exit(1);
  }
  if (VERBOSE) printf("IP Address: %s\n", inet_ntoa(listen_addr));

  // Get the server port number
  server_port = atoi(argv[2]);
  if (VERBOSE) printf("Server Port: %d\n", server_port);

  // Initialize mysocket Socket
  // printf("mysocket: creating mysocket socket\n");
  serverSocket = socket (AF_INET, SOCK_DGRAM, 0);
  // mysocket socket id will be negative on error
  if (serverSocket < 0) {
    fprintf(stderr, "mysocket: socket creation failed: %d\n", netLastError());
    exit(1);
  }
  netIgnoreConnReset(serverSocket);

  // Bind to the local socket
  // printf("mysocket: binding mysocket\n");
  // Erase the data in mysocket_addr struct
  memset(&mysocket_addr, 0, sizeof(mysocket_addr));
  // Initialize the data in mysocket_addr
  mysocket_addr.sin_family = AF_INET;
  mysocket_addr.sin_addr = listen_addr;
  mysocket_addr.sin_port = htons(server_port);
  // Bind the socket to the address
  status = bind(serverSocket,
                (struct sockaddr *)&mysocket_addr,
                sizeof(mysocket_addr));
  // Report if we have an error
  if (status < 0) {
    // Usually another program (or server) already has the port
    fprintf(stderr, "mysocket: bind failed: %d\n", netLastError());
    exit(1);
  }

  // Initialize the server
  serverInitialize();

  // infine listening loop
  if (VERBOSE) printf("Ready for Connections\n");
  serverLog("Listening on %s port %d", inet_ntoa(listen_addr), server_port);
  for (i=0; i < OPTION_COUNT; i++) {
    if (serverOptions[i] != gameOptions[i].defaultValue) serverLog("Rule: %s %d", gameOptions[i].label, serverOptions[i]);
  }

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
      fprintf(stderr, "select failed: %d\n", netLastError());
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
      broadcastScoreboard();
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
