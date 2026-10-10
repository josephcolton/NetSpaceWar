#include "global.h"
#include "cdata.h"
#include "client.h"
#include "gui.h"
#include "game.h"

// Send the player's input to the server
static void gameSendInput(int mysocket, struct sockaddr_in server_addr,
                          double input_angle, double input_accel, int input_energy, int input_flags) {
  // See if the user turned or accelerated
  if (input_angle != 0 || input_accel != 0) {
    csend_msg_clientupdate(mysocket, server_addr, ACTION_THRUST, input_accel, input_angle, 0);
  }

  // The user may have wanted to transfer power.
  if (input_energy != 0) {
    csend_msg_clientupdate(mysocket, server_addr, ACTION_TRANSPWR, 0.0, 0.0, input_energy);
  }

  // See if the user fired a missile
  if (input_flags & P_GUI_FLAG_FIRE) {
    csend_msg_clientupdate(mysocket, server_addr, ACTION_FIRE, 0.0, 0.0, 0);
  }

  // See if the user is hyper jumping
  if (input_flags & P_GUI_FLAG_HYPER) {
    csend_msg_clientupdate(mysocket, server_addr, ACTION_JUMP, 0.0, 0.0, 0);
  }
}

int gamePlay(const char *hostname, int port, const char *username) {
  int mysocket;
  int status;
  int result = GAME_LEFT;
  struct sockaddr_in server_addr;
  struct timeval timeout;
  double starttime;
  double input_angle, input_accel;
  int input_energy, input_flags;
  char connecting[LEN];
  message_t mtype;
#ifdef UGLY_MISSILE_DRAW_POS_FIX
  int seenmpacket=0;
  int seenspacket=0;
#endif

  // File Descriptors for the select command
  fd_set readfds; // file descriptor set

  // Start from a clean slate; this may not be the first game
  cdataInitialize();
  strncpy(clientUsername, username, USERNAME_MAX);
  clientUsername[USERNAME_MAX - 1] = 0;

  gui_init_sdl();
  SDL_StopTextInput(gui_get_window());
  snprintf(connecting, LEN, "%s port %d", hostname, port);
  gui_draw_status("Connecting...", connecting);

  mysocket = clientOpenSocket(hostname, port, &server_addr);
  if (mysocket < 0) return GAME_FAILED;
  starttime = platformNow();

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

    if (status > 0 && FD_ISSET (mysocket, &readfds)) {
      // We received a packet from the server
      mtype = clientReceive(mysocket);
#ifdef UGLY_MISSILE_DRAW_POS_FIX
      // Only players and missiles move, so redraw once both have updated
      if (mtype == MSG_UPDATEPLAYERS)  seenspacket=1;
      if (mtype == MSG_UPDATEMISSILES) seenmpacket=1;
#else
      (void)mtype;
#endif
    }

    // Read input every pass so the window keeps responding while we connect
    gui_get_input(&input_angle, &input_accel, &input_energy, &input_flags);

    if (clientShip != NULL) {
#ifdef UGLY_MISSILE_DRAW_POS_FIX
      if (seenmpacket==1 && seenspacket==1)
      {
#endif
	// Update gui
	gui_update_display();
#ifdef UGLY_MISSILE_DRAW_POS_FIX
	seenmpacket=0;
	seenspacket=0;
      }
#endif
      gameSendInput(mysocket, server_addr, input_angle, input_accel, input_energy, input_flags);
    }

    // Escape leaves the game; closing the window leaves and quits
    if (input_flags & (P_GUI_FLAG_QUIT | P_GUI_FLAG_CLOSE)) {
      if (clientObjectID != 0) csend_msg_logout(mysocket, server_addr);
      result = (input_flags & P_GUI_FLAG_CLOSE) ? GAME_CLOSED : GAME_LEFT;
      clientAlive = 0;
    }

    // Give up if we cannot log in or the server has gone away
    if (clientAlive && clientConnectionProblem(starttime)) {
      printf("%s\n", clientStatusMessage);
      result = GAME_FAILED;
      clientAlive = 0;
    }
  }

  // A refused login stops the loop from the loginack handler
  if (result == GAME_LEFT && clientStatusMessage[0] != 0) result = GAME_FAILED;

  if (DEBUG) printf("Completed while(clientAlive) loop\n");
  netCloseSocket(mysocket);
  return result;
}
