#ifndef OPTIONS_H
#define OPTIONS_H

#include "global.h"

/* Game rules the host can change. The server takes them as command-line
   flags (--planets 3) and the launcher's Settings screen picks them. The
   defaults are the constants in global.h. */
enum {
  OPTION_PLANETS,
  OPTION_GRAVITY,
  OPTION_MISSILE_TTL,
  OPTION_MAX_VELOCITY,
  OPTION_CRASH_DMG,
  OPTION_MISSILE_DMG,
  OPTION_MISSILE_COST,
  OPTION_JUMP_COST,
  OPTION_COUNT
};

struct gameOption {
  const char *name;   // Command-line flag (without --) and settings.txt key
  const char *label;  // Menu label
  const char *help;   // One line for the menu
  int defaultValue;
  int minValue, maxValue, step;
};

extern const struct gameOption gameOptions[OPTION_COUNT];

void gameOptionsReset(int *values);
int gameOptionClamp(int option, int value);
int gameOptionFind(const char *name);  // -1 if there is no such option
// Read "--name value" pairs from argv[first..]. Returns 0 (after printing
// why) on an unknown option or a value out of range.
int gameOptionsParse(int argc, char **argv, int first, int *values);
void gameOptionsUsage();

#endif
