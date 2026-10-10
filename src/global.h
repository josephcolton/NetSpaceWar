#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "platform.h"

/*
 * Wire format: every message is a struct below, sent as raw bytes. To keep
 * the layout identical across compilers and platforms, the structs are
 * byte-packed and use fixed-width types (doubles are IEEE 754 everywhere we
 * build). Byte order is the sender's; all supported platforms are little-endian.
 */
#if defined(__GNUC__) || defined(__clang__)
#define packed __attribute__((packed))
#else
#define packed
#endif

/* Testing Variables */
#define DEBUG 0

/* Global variables for messages */
#define CONNECT_TIMEOUT  5 // Seconds a client waits to log in
#define LOGIN_RETRY      0.5 // Seconds between login attempts
#define SERVER_SILENCE   5 // Seconds without packets before a client gives up on the server
#define SW_FALSE         0
#define SW_TRUE          1
#define MTU              1500
#define MAX_PACKET       65536 // Receive buffer size; a full missile list is larger than MTU
#define USERNAME_MAX     20
#define LEN              100
#define SPACE_MIN_XCOORD -100.0
#define SPACE_MAX_XCOORD 100.0
#define SPACE_MIN_YCOORD -100.0
#define SPACE_MAX_YCOORD 100.0
#define SHIP_MAX_ENERGY  100
#define SHIP_MAX_SHIELDS 100
#define SHIP_REST_SHIELD 1
#define KEEPALIVE_LIMIT  5
#define MISSILE_TTL      15
#define CLIENT_MISSILE_STALE 3 // Seconds a client keeps a missile the server has stopped sending
#define COLLISION_DMG    10
#define COLLISION_EXP    25
#define MISSILE_COST     2
#define THRUST_COST      .01
#define JUMP_COST        25
#define EXPLOSION_TTL    30
#define PHYSICS_PER_REST 10
#define GRAVITY          100
#define PLANETS          2

/* Point Assignments */
#define POINTS_HIT       10
#define POINTS_KILL      100
#define POINTS_DIE       -100
#define SCOREBOARD_TICKS 20 // Resend the scoreboard at least this often (in physics ticks)

/* Maximum Counts on data types */
#define MAX_COUNT_CLIENTS    5
#define MAX_COUNT_EXPLOSIONS 200
#define MAX_COUNT_SHIPS      MAX_COUNT_CLIENTS
#define MAX_COUNT_MISSILES   50
#define MAX_COUNT_PLANETS    5

/* Define Status Codes */
#define STATUS_OKAY    0
#define STATUS_INUSE   1
#define STATUS_INVALID 2
#define STATUS_FULL    3

/* Request Data Types */
#define DATA_STATICOBJS 0
#define DATA_CLIENTINFO 1
#define DATA_CLIENTLIST 2
#define DATA_SCORES     3
#define DATA_OPTIONS    4

/* Define Message Types */
#define MSG_LOGIN          0
#define MSG_LOGINACK       1
#define MSG_LOGOUT         2
#define MSG_LOGOUTACK      3
#define MSG_KEEPALIVE      4
#define MSG_REQUEST        5
#define MSG_CLIENTINFO     6
#define MSG_UPDATEPLAYERS  7
#define MSG_UPDATEMISSILES 8
#define MSG_EXPLOSION      9
#define MSG_CLIENTLIST     10
#define MSG_SCOREBOARD     11
#define MSG_CLIENTUPDATE   12
#define MSG_STATICOBJS     13
#define MSG_DELETEOBJ      14
#define MSG_GAMEOPTIONS    15

/* Define Actions */
#define ACTION_THRUST   0
#define ACTION_FIRE     1
#define ACTION_JUMP     2
#define ACTION_TRANSPWR 3

/* Define Object Types */
#define OBJECT_EXPLOSION 0
#define OBJECT_SHIP      1
#define OBJECT_MISSILE   2
#define OBJECT_PLANET    3





/* GUI or universe defines */
/* Internal units / Ship radius */
#define P_GUI_SHIP_RADIUS_INTERNAL (1.0)

#define P_GUI_SHOT_RADIUS_INTERNAL (0.25)

#define P_GUI_SHIP_RADIUS_PER_SCRH (50.0)

#define P_GUI_EXPLOSION_RADIUS_INT (7.0)

/*
  Given a buffer find how many pixels there are per ship radius.
*/
#define P_GUI_RADIUS_FUNC(x) ((double)(x->h) / P_GUI_SHIP_RADIUS_PER_SCRH)

// In internal units per frame per second. Must give physics module access to a timer. This should be per frame per second.
#define SHIP_MAX_VELOCITY          (10.0)
#define PHYS_SECS_PER_UPDATE       (0.05)
#define SERVER_TIMEOUT             (int)(1000000.0 * PHYS_SECS_PER_UPDATE)
#define MISSILE_EXTRA_SPEED        (15.0)

#define P_GUI_MAX_ENERGY	   (SHIP_MAX_ENERGY)
#define P_GUI_STAR_COUNT	   (100)
#define P_GUI_DEFAULT_PLANET_SIZE  (5.0)
#define P_GUI_EXP_TTL              (30)

#define P_GUI_ACCEL_PER_SECOND	(30.0)
// Is this a reasonable value? I don't know.

#define P_GUI_SECONDS_PER_JUMP  (3)
#define P_GUI_SECONDS_PER_SHOT  (.25)
#define P_GUI_ANGLE_PER_SECOND	(M_PI_2)
// Say 90 degrees a second, so it takes 4 seconds to spin once?

#define P_GUI_ENERGY_MOVE_PER_SECOND (60)


#define P_GUI_FLAG_FIRE    1
#define P_GUI_FLAG_HYPER   2
#define P_GUI_FLAG_QUIT    4 // Escape: leave the game
#define P_GUI_FLAG_CLOSE   8 // The window was closed

#define FULLSCREEN         0
#define UGLY_MISSILE_DRAW_POS_FIX


/* Message typedefs */
typedef unsigned short message_t;
typedef unsigned short status_t;
typedef unsigned short object_t;
typedef unsigned short data_t;
typedef unsigned short action_t;

/* Data Structs */
#pragma pack(push, 1)
struct dataNodeClients {
  struct sockaddr_in *client_addr;
  char username[USERNAME_MAX];
  object_t objectID;
  time_t checkin;
  int32_t score;
  int32_t kills;
  int32_t deaths;
} packed;

// One player's line on the scoreboard
struct wireScore {
  object_t objectID;
  char username[USERNAME_MAX];
  int32_t score;
  int32_t kills;
  int32_t deaths;
} packed;

// One entry of msg_clientlist. dataNodeClients holds a pointer and a
// time_t, so it cannot go on the wire directly.
struct wireClient {
  object_t objectID;
  char username[USERNAME_MAX];
} packed;

struct dataNodeExplosions {
  // startTime and endTime for calculating explosion progress
  int timeToLive;
  double xcoord;
  double ycoord;
} packed;

struct dataNodeShips {
  object_t objectID;
  double xcoord;
  double ycoord;
  int32_t status; // 1 for live, 0 for dead
  int32_t energy;
  int32_t shields;
  double orientation;
  double xVelocity;
  double yVelocity;
} packed;

struct dataNodeMissiles {
  object_t objectID;
  double xcoord;
  double ycoord;
  double orientation;
  double xVelocity;
  double yVelocity;
  object_t owner;
  int64_t ttl; // time() when the missile expires
} packed;

struct dataNodePlanets {
  object_t objectID;
  double xcoord;
  double ycoord;
} packed;

/* Basic Message Struct */
struct msg_basic {
  message_t mtype;
} packed;

/* Server Message Structs */
// Report login status to client
struct msg_loginack {
  message_t mtype; /* = MSG_LOGINACK */
  status_t status;
  object_t objectID;
} packed;

// Report logout/disconnect to client
struct msg_logoutack {
  message_t mtype; /* = MSG_LOGOUTACK */
} packed;

// Report static objects to client
struct msg_staticobjs {
  message_t mtype; /* = MSG_STATICOBJS */
  int32_t obj_count;
  struct dataNodePlanets planets[]; // obj_count entries
} packed;

// Report a clients information
struct msg_clientinfo {
  message_t mtype; /* = MSG_CLIENTINFO */
  object_t objectID;
  double xcoord;
  double ycoord;
  int32_t status;
  int32_t energy;
  int32_t shields;
  double orientation;
  double xVelocity;
  double yVelocity;
} packed;

// Update player positions
struct msg_updateplayers {
  message_t mtype; /* = MSG_UPDATEPLAYERS */
  int32_t obj_count;
  struct dataNodeShips ships[]; // obj_count entries
} packed;

// Update object positions
struct msg_updatemissiles {
  message_t mtype; /* = MSG_UPDATEMISSILES */
  int32_t obj_count;
  struct dataNodeMissiles missiles[]; // obj_count entries
} packed;

// Report explosion location to client
struct msg_explosion {
  message_t mtype; /* = MSG_EXPLOSION */
  double xcoord;
  double ycoord;
} packed;

// Report a list of clients in objectID/username pairs
struct msg_clientlist {
  message_t mtype; /* = MSG_CLIENTLIST */
  int32_t obj_count;
  struct wireClient clients[]; // obj_count entries
} packed;

// Report every player's score
struct msg_scoreboard {
  message_t mtype; /* = MSG_SCOREBOARD */
  int32_t obj_count;
  struct wireScore scores[]; // obj_count entries
} packed;

// The server's game rules, in options.h order. New options go on the end,
// so a client reads the first obj_count it knows and keeps defaults for the rest.
struct msg_gameoptions {
  message_t mtype; /* = MSG_GAMEOPTIONS */
  int32_t obj_count;
  int32_t values[]; // obj_count entries
} packed;

struct msg_deleteobj {
  message_t mtype; /* = MSG_DELETEOBJ */
  data_t object_type;
  object_t objectID;
} packed;

/* Client Message Structs */
// Request login to server
struct msg_login {
  message_t mtype; /* = MSG_LOGIN */
  char username[USERNAME_MAX];
} packed;

// Notify server of logout
struct msg_logout {
  message_t mtype; /* = MSG_LOGOUT */
} packed;

// Keep alive, sent ever 10 seconds that nothing else has been sent.
struct msg_keepalive {
  message_t mtype; /* = MSG_KEEPALIVE */
} packed;

// Request data from server
struct msg_request {
  message_t mtype; /* = MSG_REQUEST */
  data_t dtype;
} packed;

// Notify server of updated client state
struct msg_clientupdate {
  message_t mtype; /* = MSG_CLIENTUPDATE */
  action_t action;
  double var1;
  double var2;
  int32_t var3;
} packed;

#pragma pack(pop)

/* Catch any change to the wire layout at compile time */
_Static_assert(sizeof(struct wireClient) == 22, "wireClient wire size");
_Static_assert(sizeof(struct dataNodeShips) == 54, "dataNodeShips wire size");
_Static_assert(sizeof(struct dataNodeMissiles) == 52, "dataNodeMissiles wire size");
_Static_assert(sizeof(struct dataNodePlanets) == 18, "dataNodePlanets wire size");
_Static_assert(sizeof(struct msg_clientinfo) == 56, "msg_clientinfo wire size");
_Static_assert(sizeof(struct msg_updateplayers) == 6, "msg_updateplayers wire size");
_Static_assert(sizeof(struct msg_clientupdate) == 24, "msg_clientupdate wire size");
_Static_assert(sizeof(struct msg_login) == 22, "msg_login wire size");
_Static_assert(sizeof(struct wireScore) == 34, "wireScore wire size");
_Static_assert(sizeof(struct msg_scoreboard) == 6, "msg_scoreboard wire size");
_Static_assert(sizeof(struct msg_gameoptions) == 6, "msg_gameoptions wire size");

#endif
