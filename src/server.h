#ifndef SERVER_H
#define SERVER_H

#include "global.h"

/* Server Initialization, misc */
void serverInitialize();
void checkClientKeepAlive();

/* Server Handling Messages */
void shandle_msg_login(int mysocket, void *packet, struct sockaddr_in client_addr);
void shandle_msg_logout(int mysocket, void *packet, struct sockaddr_in client_addr);
void shandle_msg_keepalive(int mysocket, void *packet, struct sockaddr_in client_addr);
void shandle_msg_request(int mysocket, void *packet, struct sockaddr_in client_addr);
void shandle_msg_clientupdate(int mysocket, void *packet, struct sockaddr_in client_addr);

/* Server Sending Messages */
void ssend_msg_loginack(int mysocket, struct sockaddr_in client_addr, status_t status, object_t objectID);
void ssend_msg_logoutack(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_staticobjs(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_clientinfo(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_updateplayers(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_updateobjects(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_explosion(int mysocket, struct sockaddr_in client_addr, double xcoord, double ycoord);
void ssend_msg_clientlist(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_scoreboard(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_gameoptions(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_deleteobj(int mysocket, struct sockaddr_in client_addr, data_t object_type, object_t objectID);

/* Broadcast Messages */
void broadcastUpdatePlayers();
void broadcastUpdateMissiles();
void broadcastScoreboard();
void broadcastDeleteMissile(object_t objectID);
void broadcastDeleteShip(object_t objectID);

#endif
