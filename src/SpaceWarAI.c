
#include "global.h"
#include "cdata.h"
#include "client.h"
#include "ai.h"

#define MYSOCKET_PORT -1

// Set when a player update arrives; the AI makes one decision per update
int seenspacket=0;

void handle_socket(int mysocket) {
  int status;
  void *packet;
  message_t mtype;
  struct sockaddr_in server_addr;
  socklen_t addrlen;

  // Malloc the packet
  packet = calloc(1, MAX_PACKET);

  // Erase the data in the server_addr just for fun
  memset(&server_addr, 0, sizeof(server_addr));
  addrlen = sizeof(server_addr);

  // Receive the packet and sending address
  status = recvfrom(mysocket,
		    packet,
		    MAX_PACKET,
		    0, 
		    (struct sockaddr *)&server_addr,
		    &addrlen);

  // Ignore failed reads and packets too short to have a message type
  if (status < (int)sizeof(message_t)) {
    free(packet);
    return;
  }

  // Now we have the packet, we need to identify it.
  mtype = ((struct msg_basic *)packet)->mtype;

  // We need to call correct function.  The correct function
  // will need to gather all required information from the
  // packet before leaving because the packet will be freed.
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

  if (mtype == MSG_UPDATEPLAYERS)  seenspacket=1;

  // The packet is freed
  free(packet);
}

int main(int argc, char *argv[]) {
  char server_name[LEN];
  char server_ipaddr[LEN];
  int mysocket;
  int status;
  int server_port;
  struct sockaddr_in mysocket_addr, server_addr;
  struct hostent *lookup;
  struct timeval timeout;
  time_t starttime = time(NULL);

  // File Descriptors for the select command
  fd_set readfds; // file descriptor set

  // Usage statement
  if (argc != 4) {
    printf("Usage:\n\t%s servername serverport username\n\n", argv[0]);
    return 0;
  }

  // Initialize Client Data
  cdataInitialize();

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
    strcpy(server_ipaddr, inet_ntoa(*(struct in_addr*)lookup->h_addr));
    printf("Server IP Address: %s\n", server_ipaddr);
  }
  // Get the server port number
  server_port = atoi(argv[2]);
  printf("Server Port: %d\n", server_port);
  // Get the username
  strncpy(clientUsername, argv[3], USERNAME_MAX);
  clientUsername[USERNAME_MAX - 1] = 0;

  // Initialize Mysocket Socket
  // printf("mysocket: creating mysocket socket\n");
  mysocket = socket (AF_INET, SOCK_DGRAM, 0);
  // mysocket socket id will be negative on error
  if (mysocket < 0) {
    printf("mysocket: socket creation failed: %d", netLastError());
    exit(0);
  }
  netIgnoreConnReset(mysocket);

  // Bind to the local socket
  // printf("mysocket: binding mysocket\n");
  // Erase the data in mysocket_addr struct
  memset(&mysocket_addr, 0, sizeof(mysocket_addr));

  // Connect to the server
  // printf("mysocket: creating server_addr struct\n");
  // Erase the data in the server_addr struct
  memset(&server_addr, 0, sizeof(server_addr));
  // Set the server variables
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(server_ipaddr);
  server_addr.sin_port = htons(server_port);

  // Send login request to the server
  csend_msg_login(mysocket, server_addr, clientUsername);

  // Identify OS
#ifdef OS_Linux
  if (DEBUG) printf("Starting Linux Client\n");
#endif
#ifdef OS_Darwin
  if (DEBUG) printf("Starting Darwin Client\n");
#endif

  // Keep making requests until we get a reply
  clientAIinitAI();
  while(clientAlive) {
    fflush(stdout);
    // Add mysocket socket file descriptor to select set
    FD_ZERO (&readfds); // initialize set
    FD_SET (mysocket, &readfds); // Add mysocket port to set
      
    // Set the timeout for reading
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // We want about 10 times a second.

    status = select(mysocket+1, &readfds, NULL, NULL, &timeout);
    checkKeepAlive(mysocket, server_addr);
    cdataUpdateAll();

    if (status < 0) {
      printf("select failed: %d\n", netLastError());
      continue;
    }

    // Check the select status to see what happened
    if (status == 0) {
      // printf("timeout\n"); // Does not matter
    }
    if (FD_ISSET (mysocket, &readfds)) {
      // We received a packet from the server
      handle_socket(mysocket);
    }

    // Make AI decisions once per player update, so they use fresh positions
    if (seenspacket) {
      seenspacket=0;
      clientAIupdate(mysocket, server_addr);
    }

    if (clientShip == NULL) {
      // If the clientShip is still NULL, it might indicate a dead server
      if (starttime + CONNECT_TIMEOUT < time(NULL)) {
	printf("Server Connection Timeout\n");
	clientAlive = 0;
      }
    }
  }
  if (DEBUG) printf("Completed while(clientAlive) loop\n");

  return 0;
}

