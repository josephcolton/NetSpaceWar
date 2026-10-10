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

/* Client Sending Messages */
void csend_msg_login(int mysocket, struct sockaddr_in server_addr, char *username);
void csend_msg_logout(int mysocket, struct sockaddr_in server_addr);
void csend_msg_keepalive(int mysocket, struct sockaddr_in server_addr);
void csend_msg_request(int mysocket, struct sockaddr_in server_addr, data_t dtype);
void csend_msg_clientupdate(int mysocket, struct sockaddr_in server_addr, action_t action, double var1, double var2, int var3);

#endif
