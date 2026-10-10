#ifndef CLIENT_H
#define CLIENT_H

#include "global.h"

/* Client Updates */
void checkKeepAlive(int mysocket, struct sockaddr_in server_addr);

/* Client Handling Messages */
void chandle_msg_loginack(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_logoutack(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_staticobjs(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_clientinfo(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_updateplayers(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_updatemissiles(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_explosion(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_clientlist(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_scoreboard(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_deleteobj(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_updateasteroids(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_event(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_updateorbs(int mysocket, void *packet, struct sockaddr_in server_addr);
void chandle_msg_gameoptions(int mysocket, void *packet, struct sockaddr_in server_addr);

/* Client Sending Messages */
void csend_msg_login(int mysocket, struct sockaddr_in server_addr, char *username);
void csend_msg_logout(int mysocket, struct sockaddr_in server_addr);
void csend_msg_keepalive(int mysocket, struct sockaddr_in server_addr);
void csend_msg_request(int mysocket, struct sockaddr_in server_addr, data_t dtype);
void csend_msg_clientupdate(int mysocket, struct sockaddr_in server_addr, action_t action, double var1, double var2, int var3);


/* Connection helpers shared by the game client and the AI */
#define MSG_NONE 0xffff // clientReceive() result when no message was read

// Time of the last packet from the server, from platformNow()
extern double clientLastPacketTime;

// Resolve the server and open a UDP socket to it. Returns the socket, or
// -1 with the reason in clientStatusMessage.
int clientOpenSocket(const char *hostname, int port, struct sockaddr_in *server_addr);
// Read one packet and pass it to its handler. Returns its message type.
message_t clientReceive(int mysocket);
// Send the login, again every LOGIN_RETRY seconds until it is accepted,
// then the request for the game rules until they arrive
void clientRetryLogin(int mysocket, struct sockaddr_in server_addr);
// True when login has taken too long or the server has gone quiet; the
// reason is in clientStatusMessage. startTime is when we started connecting.
int clientConnectionProblem(double startTime);

#endif
