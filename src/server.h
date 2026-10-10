#ifndef SERVER_H
#define SERVER_H

#include "global.h"

/* Server output on stdout */
#define LOG_SILENT  0 // Nothing (--silent)
#define LOG_EVENTS  1 // Joins, leaves, deaths and respawns (the default)
#define LOG_VERBOSE 2 // Every packet too (--verbose)
extern int serverLogLevel;
#define VERBOSE (serverLogLevel >= LOG_VERBOSE)
// Report a game event, with the time, unless --silent
void serverLog(const char *format, ...);
// A client's username for messages; they may already have left
const char *serverClientName(object_t objectID);

/* A server started by a player's game (--host-player NAME) shuts down when
   that player leaves, or if they haven't joined after HOST_JOIN_TIMEOUT
   seconds, so it can't outlive a crashed game. */
#define HOST_JOIN_TIMEOUT 30
extern char serverHostName[USERNAME_MAX]; // Empty for a standalone server
extern int serverHostJoined;
extern int serverShutdown;                // Set when the main loop should end
// Call before deleting a client who is leaving
void serverClientLeaving(object_t objectID);

/* Server Initialization, misc */
void serverInitialize();
// Where to put a ship joining, respawning or hyperjumping (self: its own objectID)
void serverSpawnPoint(object_t self, double *xcoord, double *ycoord);
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
void ssend_msg_updateasteroids(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_updateorbs(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_explosion(int mysocket, struct sockaddr_in client_addr, double xcoord, double ycoord);
void ssend_msg_clientlist(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_scoreboard(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_gameoptions(int mysocket, struct sockaddr_in client_addr);
void ssend_msg_deleteobj(int mysocket, struct sockaddr_in client_addr, data_t object_type, object_t objectID);

/* Broadcast Messages */
void broadcastUpdatePlayers();
void broadcastUpdateMissiles();
void broadcastUpdateOrbs();
void broadcastUpdateAsteroids();
void broadcastStaticObjs();
void broadcastStaticObjsRepeat();
void broadcastScoreboard();
void broadcastDeleteMissile(object_t objectID);
void broadcastDeleteShip(object_t objectID);

#endif
