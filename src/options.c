#include "options.h"

// Asteroid frequency, from none to one every few seconds (ASTEROID_INTERVALS)
static const char *const asteroidLevels[ASTEROID_LEVELS] = {
  "None", "Rare", "Light", "Moderate", "Heavy", "Frequent"
};

// Case-insensitive compare, so --asteroids light works
static int nameCompare(const char *a, const char *b) {
  while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) a++, b++;
  return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

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
  {"asteroids",     "Asteroids",      "How often asteroids drift in",                            ASTEROIDS,         0, ASTEROID_LEVELS - 1, 1, asteroidLevels},
};

const char *gameOptionText(int option, int value) {
  static char number[16];
  const struct gameOption *o = &gameOptions[option];
  if (o->names != NULL && value >= o->minValue && value <= o->maxValue) return o->names[value - o->minValue];
  snprintf(number, sizeof(number), "%d", value);
  return number;
}

// "None, Rare, ..." for an option with names
static const char *gameOptionNames(int option) {
  static char list[LEN];
  int i, used = 0;
  list[0] = 0;
  for (i=0; i <= gameOptions[option].maxValue - gameOptions[option].minValue; i++) {
    used += snprintf(list + used, sizeof(list) - used, "%s%s", i ? ", " : "", gameOptions[option].names[i]);
    if (used >= (int)sizeof(list)) break;
  }
  return list;
}

// A value given on the command line: a number, or one of the option's names
static int gameOptionValue(int option, const char *text, long *value) {
  const struct gameOption *o = &gameOptions[option];
  char *end;
  int i;
  if (o->names != NULL) {
    for (i=0; i <= o->maxValue - o->minValue; i++) {
      if (nameCompare(text, o->names[i]) == 0) {
        *value = o->minValue + i;
        return 1;
      }
    }
  }
  *value = strtol(text, &end, 10);
  return *text != 0 && *end == 0;
}

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
    if (!gameOptionValue(option, argv[i+1], &value) || value != gameOptionClamp(option, (int)value)) {
      if (gameOptions[option].names != NULL) printf("%s must be one of %s\n", argv[i], gameOptionNames(option));
      else printf("%s must be a number from %d to %d\n", argv[i],
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
    if (gameOptions[i].names != NULL) {
      printf("\t--%-15s %s, default %s. %s\n", gameOptions[i].name, gameOptionNames(i),
             gameOptionText(i, gameOptions[i].defaultValue), gameOptions[i].help);
      continue;
    }
    printf("\t--%-15s %3d to %3d, default %3d. %s\n", gameOptions[i].name, gameOptions[i].minValue,
           gameOptions[i].maxValue, gameOptions[i].defaultValue, gameOptions[i].help);
  }
}
