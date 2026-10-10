#include "options.h"

const struct gameOption gameOptions[OPTION_COUNT] = {
  // name           label             help                                                       default            min max step
  {"planets",       "Planets",        "How many planets there are",                              PLANETS,           0, MAX_COUNT_PLANETS, 1},
  {"gravity",       "Gravity",        "How hard planets pull on ships and missiles",             GRAVITY,           0, 300, 10},
  {"missile-life",  "Missile life",   "Seconds before a missile blows up on its own",            MISSILE_TTL,       1, 30, 1},
  {"max-speed",     "Max speed",      "Fastest a ship can go under thrust",                      SHIP_MAX_VELOCITY, 4, 30, 1},
  {"crash-damage",  "Crash damage",   "Shield damage from hitting a ship (planets do 5 times)",  COLLISION_DMG,     0, 50, 1},
  {"missile-damage","Missile damage", "Shield damage from a missile hit",                        COLLISION_EXP,     5, 100, 5},
  {"shot-cost",     "Shot cost",      "Energy used by each missile",                             MISSILE_COST,      0, 20, 1},
  {"jump-cost",     "Jump cost",      "Energy used by a hyperjump",                              JUMP_COST,         0, 100, 5},
  {"volcanoes",     "Volcanoes",      "Planets that start with a volcano (up to Planets)",       VOLCANOES,         0, MAX_COUNT_PLANETS, 1},
  {"orb-wait",      "Orb wait",       "Seconds a volcano waits after its recharge orb is gone",  ORB_WAIT,          5, 120, 5},
};

void gameOptionsReset(int *values) {
  int i;
  for (i=0; i < OPTION_COUNT; i++) values[i] = gameOptions[i].defaultValue;
}

int gameOptionsFix(int *values) {
  if (values[OPTION_VOLCANOES] <= values[OPTION_PLANETS]) return 0;
  values[OPTION_VOLCANOES] = values[OPTION_PLANETS];
  return 1;
}

int gameOptionClamp(int option, int value) {
  if (value < gameOptions[option].minValue) return gameOptions[option].minValue;
  if (value > gameOptions[option].maxValue) return gameOptions[option].maxValue;
  return value;
}

int gameOptionFind(const char *name) {
  int i;
  for (i=0; i < OPTION_COUNT; i++) {
    if (strcmp(name, gameOptions[i].name) == 0) return i;
  }
  return -1;
}

int gameOptionsParse(int argc, char **argv, int first, int *values) {
  int i, option;
  char *end;
  long value;

  for (i=first; i < argc; i += 2) {
    if (strncmp(argv[i], "--", 2) != 0 || (option = gameOptionFind(argv[i] + 2)) < 0) {
      printf("Unknown option: %s\n", argv[i]);
      return 0;
    }
    if (i + 1 >= argc) {
      printf("Missing value for %s\n", argv[i]);
      return 0;
    }
    value = strtol(argv[i+1], &end, 10);
    if (*argv[i+1] == 0 || *end != 0 || value != gameOptionClamp(option, (int)value)) {
      printf("%s must be a number from %d to %d\n", argv[i],
             gameOptions[option].minValue, gameOptions[option].maxValue);
      return 0;
    }
    values[option] = (int)value;
  }
  if (values[OPTION_VOLCANOES] > values[OPTION_PLANETS]) {
    printf("--volcanoes can't be more than --planets (%d)\n", values[OPTION_PLANETS]);
    return 0;
  }
  return 1;
}

void gameOptionsUsage() {
  int i;
  printf("Options:\n");
  for (i=0; i < OPTION_COUNT; i++) {
    printf("\t--%-15s %3d to %3d, default %3d. %s\n", gameOptions[i].name, gameOptions[i].minValue,
           gameOptions[i].maxValue, gameOptions[i].defaultValue, gameOptions[i].help);
  }
}
