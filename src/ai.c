#include "global.h"
#include "client.h"
#include "cdata.h"
#include "ai.h"

/* Global variable definitions (declared extern in ai.h) */
object_t clientAItargetID;

/* AI state between decisions */
static double aiLastUpdate = 0.0;
static double aiLastShot = 0.0;
static double aiLastJump = 0.0;

// Wall clock time in seconds
static double aiNow() {
  return platformNow();
}

// Shortest signed distance from a to b along one axis of the looping universe
static double aiWrapDelta(double a, double b, double minCoord, double maxCoord) {
  double span = maxCoord - minCoord;
  double d = b - a;
  if (d > span / 2) d -= span;
  if (d < -span / 2) d += span;
  return d;
}

// Angle normalized to [-PI, PI]
static double aiNormalizeAngle(double angle) {
  while (angle > M_PI) angle -= 2 * M_PI;
  while (angle < -M_PI) angle += 2 * M_PI;
  return angle;
}

// Distance from our ship to a point, accounting for the universe looping
static double aiDistanceTo(double xcoord, double ycoord) {
  double dx = aiWrapDelta(clientShip->xcoord, xcoord, SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
  double dy = aiWrapDelta(clientShip->ycoord, ycoord, SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
  return sqrt(dx * dx + dy * dy);
}

/* Limits that follow the server's game rules (cdataOptions) */

// Speed used to close on a target or get away from a planet
static double aiCruiseSpeed() {
  return CLIENT_OPTION(MAX_VELOCITY) * AI_CRUISE_FRACTION;
}

// Farthest target worth shooting at: missiles blow up at the end of their life
static double aiFireRange() {
  double reach = MISSILE_EXTRA_SPEED * CLIENT_OPTION(MISSILE_TTL) * AI_MISSILE_REACH;
  return (reach < AI_FIRE_RANGE) ? reach : AI_FIRE_RANGE;
}

// Where to stop closing in: well inside firing range when missiles are short-lived
static double aiEngageRange() {
  double range = aiFireRange() * AI_ENGAGE_FRACTION;
  return (range < AI_ENGAGE_RANGE) ? range : AI_ENGAGE_RANGE;
}

// Distance from a planet's center to steer clear of. Stronger gravity
// needs more room to escape; with no gravity, only the planet itself.
static double aiPlanetDanger() {
  return P_GUI_DEFAULT_PLANET_SIZE + AI_PLANET_MARGIN * (0.5 + 0.5 * CLIENT_OPTION(GRAVITY) / GRAVITY);
}

void clientAIinitAI() {
  clientAItargetID = 0;
  aiLastUpdate = aiNow();
  aiLastShot = 0.0;
  aiLastJump = 0.0;
}

object_t clientAIselectTarget() {
  int i;
  object_t closeID = 0;
  double closeDist = 0.0;
  double dist;
  double currentDist = -1.0;
  struct dataNodeShips *ship;

  if (clientShip == NULL) return 0;

  // Calculate which live ship is the closest
  for (i=0; i < cdataCountShips; i++) {
    ship = cdataArrayShips[i];
    if (ship == NULL) continue;
    if (ship->objectID == clientObjectID) continue;
    if (ship->status == 0) continue;
    dist = aiDistanceTo(ship->xcoord, ship->ycoord);
    if (ship->objectID == clientAItargetID) currentDist = dist;
    if (closeID == 0 || dist < closeDist) {
      closeID = ship->objectID;
      closeDist = dist;
    }
  }

  // Stick with the current target unless another is clearly closer
  if (currentDist >= 0 && closeDist > currentDist * AI_TARGET_SWITCH) {
    return clientAItargetID;
  }
  return closeID;
}

double clientAIobjectDirection(object_t objectID) {
  struct dataNodeShips *target;
  double dx, dy, rvx, rvy;
  double a, b, c, disc, t, t1, t2;

  target = cdataLookupShips(objectID);
  if (clientShip == NULL || target == NULL) return 0.0;

  // Target position and velocity relative to our ship
  dx = aiWrapDelta(clientShip->xcoord, target->xcoord, SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
  dy = aiWrapDelta(clientShip->ycoord, target->ycoord, SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
  rvx = target->xVelocity - clientShip->xVelocity;
  rvy = target->yVelocity - clientShip->yVelocity;

  // Missiles inherit our velocity, so relative to us they travel at
  // MISSILE_EXTRA_SPEED. Solve |d + rv*t| = MISSILE_EXTRA_SPEED*t for
  // the time t when a missile could meet the target.
  a = rvx * rvx + rvy * rvy - MISSILE_EXTRA_SPEED * MISSILE_EXTRA_SPEED;
  b = 2 * (dx * rvx + dy * rvy);
  c = dx * dx + dy * dy;
  t = -1.0;
  if (fabs(a) < 0.000001) {
    if (b < 0) t = -c / b;
  } else {
    disc = b * b - 4 * a * c;
    if (disc >= 0) {
      t1 = (-b - sqrt(disc)) / (2 * a);
      t2 = (-b + sqrt(disc)) / (2 * a);
      if (t1 > 0 && (t2 <= 0 || t1 < t2)) t = t1;
      else if (t2 > 0) t = t2;
    }
  }

  // Lead the target if an intercept exists, otherwise aim straight at it
  if (t > 0) {
    dx += rvx * t;
    dy += rvy * t;
  }
  return atan2(dy, dx);
}

// Move power between energy and shields so the ship can keep firing
static void aiManageEnergy(int mysocket, struct sockaddr_in server_addr, double dt) {
  int amount = (int)(P_GUI_ENERGY_MOVE_PER_SECOND * dt);
  if (amount < 1) amount = 1;

  if (clientShip->energy < AI_ENERGY_LOW && clientShip->shields > AI_SHIELDS_SPARE) {
    // Shields to energy
    csend_msg_clientupdate(mysocket, server_addr, ACTION_TRANSPWR, 0.0, 0.0, -amount);
  } else if (clientShip->shields < AI_SHIELDS_LOW && clientShip->energy > AI_ENERGY_SPARE) {
    // Energy to shields
    csend_msg_clientupdate(mysocket, server_addr, ACTION_TRANSPWR, 0.0, 0.0, amount);
  }
}

// Find the planet we will pass closest to within AI_PLANET_LOOKAHEAD
// seconds, if that pass is inside aiPlanetDanger(), or NULL. Planets are
// harmless when crashes do no damage.
static struct dataNodePlanets *aiPlanetThreat() {
  int i;
  double px, py, t, dist;
  double vx = clientShip->xVelocity;
  double vy = clientShip->yVelocity;
  double speed2 = vx * vx + vy * vy;
  double closeDist = aiPlanetDanger();
  struct dataNodePlanets *closest = NULL;

  if (CLIENT_OPTION(CRASH_DMG) == 0) return NULL;

  for (i=0; i < cdataCountPlanets; i++) {
    if (cdataArrayPlanets[i] == NULL) continue;
    // Our position relative to the planet
    px = aiWrapDelta(cdataArrayPlanets[i]->xcoord, clientShip->xcoord, SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
    py = aiWrapDelta(cdataArrayPlanets[i]->ycoord, clientShip->ycoord, SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
    // Time of closest approach on our current course
    t = 0.0;
    if (speed2 > 0.000001) t = -(px * vx + py * vy) / speed2;
    if (t < 0.0) t = 0.0;
    if (t > AI_PLANET_LOOKAHEAD) t = AI_PLANET_LOOKAHEAD;
    px += vx * t;
    py += vy * t;
    dist = sqrt(px * px + py * py);
    if (dist < closeDist) {
      closeDist = dist;
      closest = cdataArrayPlanets[i];
    }
  }
  return closest;
}

// The closest recharge orb worth chasing, or NULL when we don't need one
static struct dataNodeOrbs *aiWantedOrb() {
  int i;
  double dist, closeDist = AI_ORB_RANGE;
  struct dataNodeOrbs *closest = NULL;

  if (clientShip->energy + clientShip->shields >= AI_RECHARGE_BELOW) return NULL;
  for (i=0; i < cdataCountOrbs; i++) {
    dist = aiDistanceTo(cdataOrbs[i].xcoord, cdataOrbs[i].ycoord);
    if (dist < closeDist) {
      closeDist = dist;
      closest = &cdataOrbs[i];
    }
  }
  return closest;
}

void clientAIupdate(int mysocket, struct sockaddr_in server_addr) {
  double now, dt;
  struct dataNodeShips *target = NULL;
  struct dataNodePlanets *planet;
  struct dataNodeOrbs *orb;
  double dx, dy, dist = 0.0;
  double wantX = 0.0, wantY = 0.0; // Velocity we want to have
  double fixX, fixY, fixSpeed;
  double heading, aimAngle = 0.0;
  double turn, maxTurn, accel = 0.0;
  double tolerance;
  int correcting;

  now = aiNow();
  dt = now - aiLastUpdate;
  aiLastUpdate = now;
  // Ignore clock jumps and long stalls
  if (dt <= 0.0 || dt > 0.25) dt = PHYS_SECS_PER_UPDATE;

  // Make sure we have a ship
  if (clientShip == NULL) return;

  // Make sure we are alive
  if (clientShip->status == 0) {
    // Respawning; rate limited so a stale update does not trigger a second jump
    if (now - aiLastJump >= 1.0) {
      csend_msg_clientupdate(mysocket, server_addr, ACTION_JUMP, 0.0, 0.0, 0);
      aiLastJump = now;
    }
    return;
  }

  aiManageEnergy(mysocket, server_addr, dt);

  // Make sure we have a target
  clientAItargetID = clientAIselectTarget();
  if (clientAItargetID) {
    target = cdataLookupShips(clientAItargetID);
  }
  if (target != NULL) {
    dx = aiWrapDelta(clientShip->xcoord, target->xcoord, SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
    dy = aiWrapDelta(clientShip->ycoord, target->ycoord, SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
    dist = sqrt(dx * dx + dy * dy);
    aimAngle = clientAIobjectDirection(clientAItargetID);
    // Close in on a distant target, otherwise hold position (want zero velocity)
    if (dist > aiEngageRange()) {
      wantX = dx / dist * aiCruiseSpeed();
      wantY = dy / dist * aiCruiseSpeed();
    }
  }

  // Running low: head for where a recharge orb will be when we get there.
  // Keep shooting at the target on the way.
  orb = aiWantedOrb();
  if (orb != NULL) {
    double ox = aiWrapDelta(clientShip->xcoord, orb->xcoord, SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
    double oy = aiWrapDelta(clientShip->ycoord, orb->ycoord, SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
    double arrive = sqrt(ox * ox + oy * oy) / aiCruiseSpeed();
    ox += orb->xVelocity * arrive;
    oy += orb->yVelocity * arrive;
    if (ox != 0.0 || oy != 0.0) {
      wantX = ox / sqrt(ox * ox + oy * oy) * aiCruiseSpeed();
      wantY = oy / sqrt(ox * ox + oy * oy) * aiCruiseSpeed();
    }
  }

  // Avoiding planets comes before chasing targets and orbs
  planet = aiPlanetThreat();
  if (planet != NULL) {
    dx = aiWrapDelta(planet->xcoord, clientShip->xcoord, SPACE_MIN_XCOORD, SPACE_MAX_XCOORD);
    dy = aiWrapDelta(planet->ycoord, clientShip->ycoord, SPACE_MIN_YCOORD, SPACE_MAX_YCOORD);
    if (dx == 0.0 && dy == 0.0) dx = 1.0;
    wantX = dx / sqrt(dx * dx + dy * dy) * aiCruiseSpeed();
    wantY = dy / sqrt(dx * dx + dy * dy) * aiCruiseSpeed();
  }

  // Thrust is the only way to change velocity, so to reach the wanted
  // velocity we point along the difference and burn.
  fixX = wantX - clientShip->xVelocity;
  fixY = wantY - clientShip->yVelocity;
  fixSpeed = sqrt(fixX * fixX + fixY * fixY);
  correcting = (fixSpeed > AI_VELOCITY_SLOP || (planet != NULL && fixSpeed > 0.5));

  if (correcting) {
    heading = atan2(fixY, fixX);
  } else if (target != NULL) {
    heading = aimAngle;
  } else {
    heading = clientShip->orientation;
  }

  // Turn no faster than a human player can
  maxTurn = P_GUI_ANGLE_PER_SECOND * dt;
  turn = aiNormalizeAngle(heading - clientShip->orientation);
  if (turn > maxTurn) turn = maxTurn;
  if (turn < -maxTurn) turn = -maxTurn;

  // Burn once pointed close enough to the wanted heading
  if (correcting && fabs(aiNormalizeAngle(heading - clientShip->orientation - turn)) < AI_THRUST_CONE) {
    accel = P_GUI_ACCEL_PER_SECOND * dt;
    if (accel > fixSpeed) accel = fixSpeed;
  }

  if (turn != 0.0 || accel != 0.0) {
    csend_msg_clientupdate(mysocket, server_addr, ACTION_THRUST, accel, turn, 0);
  }

  // Fire whenever the gun lines up, even while maneuvering
  if (target != NULL && dist < aiFireRange() &&
      now - aiLastShot >= P_GUI_SECONDS_PER_SHOT &&
      clientShip->energy >= CLIENT_OPTION(MISSILE_COST) + AI_ENERGY_RESERVE) {
    // Accept a miss angle that still puts the missile on the target's hull
    tolerance = atan2(P_GUI_SHIP_RADIUS_INTERNAL + P_GUI_SHOT_RADIUS_INTERNAL, dist);
    if (fabs(aiNormalizeAngle(aimAngle - clientShip->orientation - turn)) < tolerance) {
      csend_msg_clientupdate(mysocket, server_addr, ACTION_FIRE, 0.0, 0.0, 0);
      aiLastShot = now;
    }
  }
}
