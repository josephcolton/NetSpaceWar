#include "global.h"
#include "cdata.h"
#include "client.h"
#include "ai.h"

int main(int argc, char *argv[]) {
  int mysocket;
  int status;
  struct sockaddr_in server_addr;
  struct timeval timeout;
  double starttime;

  // File Descriptors for the select command
  fd_set readfds; // file descriptor set

  // Usage statement
  if (argc != 4) {
    printf("Usage:\n\t%s servername serverport username\n\n", argv[0]);
    return 0;
  }

  // Initialize Client Data
  cdataInitialize();
  strncpy(clientUsername, argv[3], USERNAME_MAX);
  clientUsername[USERNAME_MAX - 1] = 0;

  // Connect to the server
  mysocket = clientOpenSocket(argv[1], atoi(argv[2]), &server_addr);
  if (mysocket < 0) {
    printf("%s\n", clientStatusMessage);
    return 1;
  }
  starttime = platformNow();

  clientAIinitAI();
  while(clientAlive) {
    fflush(stdout);
    // Keep asking to log in until the server answers
    clientRetryLogin(mysocket, server_addr);

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

    // Make AI decisions once per player update, so they use fresh positions
    if (status > 0 && FD_ISSET (mysocket, &readfds) &&
        clientReceive(mysocket) == MSG_UPDATEPLAYERS) {
      clientAIupdate(mysocket, server_addr);
    }

    // Give up if we cannot log in or the server has gone away
    if (clientConnectionProblem(starttime)) {
      printf("%s\n", clientStatusMessage);
      clientAlive = 0;
    }
  }
  if (DEBUG) printf("Completed while(clientAlive) loop\n");

  netCloseSocket(mysocket);
  return 0;
}
