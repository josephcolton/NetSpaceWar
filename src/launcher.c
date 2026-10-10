#include "global.h"
#include "cdata.h"
#include "gui.h"
#include "game.h"
#include "launcher.h"
#include "options.h"
#include "audio.h"
#include "discover.h"

#define LAUNCHER_DEFAULT_PORT "27960"
#define LAUNCHER_MAX_BOTS     (MAX_COUNT_CLIENTS - 1) // Leave room for the host
#define LAUNCHER_HOST_MAX     64
#define LAUNCHER_PORT_MAX     6
#define LAUNCHER_MAX_ITEMS    (OPTION_COUNT + 4)
#define LAUNCHER_LIST_TOP     70  // Where a scrolling screen's rows show...
#define LAUNCHER_LIST_BOTTOM  340 // ...and end
#define LAUNCHER_GAMES_TOP    336 // Where the Join screen lists games found on the network
#define LAUNCHER_MAX_ARGS     (5 + 2 * OPTION_COUNT) // Server: address, port, --silent, --host-player name, --option value...

#ifdef _WIN32
#define LAUNCHER_EXE_SUFFIX ".exe"
#else
#define LAUNCHER_EXE_SUFFIX ""
#endif

/* Colors */
#define COLOR_TITLE    0x60c0ff
#define COLOR_TEXT     0xc0c0c0
#define COLOR_DIM      0x808080
#define COLOR_FOCUS    0xffff40
#define COLOR_ERROR    0xff6060
#define COLOR_FIELD    0x101828

/* What the player has chosen, saved between runs */
typedef struct {
  char name[USERNAME_MAX];
  char host[LAUNCHER_HOST_MAX];
  char port[LAUNCHER_PORT_MAX];
  int bots;
  int options[OPTION_COUNT];  // Game rules passed to the server
  int sound, music;           // This player's volumes, 0 to AUDIO_VOLUME_MAX
} LauncherSettings;

typedef enum { SCREEN_MAIN, SCREEN_HOST, SCREEN_JOIN, SCREEN_SETTINGS, SCREEN_ABOUT, SCREEN_OPTIONS } LauncherScreen;
typedef enum { ITEM_BUTTON, ITEM_TEXT, ITEM_CHOICE, ITEM_GAME } LauncherItemKind;
typedef enum {
  ACTION_NONE, ACTION_SHOW_HOST, ACTION_SHOW_JOIN, ACTION_QUIT,
  ACTION_START_HOST, ACTION_START_JOIN, ACTION_BACK,
  ACTION_SHOW_SETTINGS, ACTION_RESET_SETTINGS, ACTION_SHOW_ABOUT, ACTION_SHOW_OPTIONS, ACTION_FIND_GAMES
} LauncherAction;

/* One line of a menu: a button, a text field, or a number to pick */
typedef struct {
  LauncherItemKind kind;
  const char *label;
  LauncherAction action;  // ITEM_BUTTON
  char *text;             // ITEM_TEXT
  int textSize;
  int digitsOnly;
  int *value;             // ITEM_CHOICE
  int minValue, maxValue, step;
  int wrap;               // Past one end goes around to the other
  const char *help;       // Game options: shown under the menu while focused
  int hidden;             // Scrolled out of view
  int game;               // ITEM_GAME: which found game
  SDL_FRect box;          // Where it is on screen, for the mouse
} LauncherItem;

typedef struct {
  LauncherSettings settings;
  LauncherScreen screen;
  LauncherItem items[LAUNCHER_MAX_ITEMS];
  int itemCount;
  int focus;
  char status[LEN];       // Error or result message shown under the menu
  char localAddress[64];  // Where other players can find a hosted game
  int running;
  float scroll, scrollMax; // How far a long screen's rows are scrolled
  int followFocus;         // Scroll to the focused row (it moved by keyboard)
  struct foundGame found[DISCOVER_MAX]; // Games on the local network (Join screen)
  int foundCount;
  int searching;
} Launcher;

/* Settings file */

static char *launcherSettingsPath() {
  static char path[1024];
  char *dir = SDL_GetPrefPath("NetSpaceWar", "NetSpaceWar");
  if (dir == NULL) return NULL;
  snprintf(path, sizeof(path), "%ssettings.txt", dir);
  SDL_free(dir);
  return path;
}

static void launcherCopy(char *dest, int size, const char *src) {
  strncpy(dest, src, size);
  dest[size - 1] = 0;
}

static void launcherLoadSettings(LauncherSettings *settings) {
  char line[LEN], *value, *path;
  FILE *file;
  int i, option;

  launcherCopy(settings->name, USERNAME_MAX, "Player");
  launcherCopy(settings->host, LAUNCHER_HOST_MAX, "localhost");
  launcherCopy(settings->port, LAUNCHER_PORT_MAX, LAUNCHER_DEFAULT_PORT);
  settings->bots = 2;
  settings->sound = 8;
  settings->music = 5;
  gameOptionsReset(settings->options);

  path = launcherSettingsPath();
  if (path == NULL || (file = fopen(path, "r")) == NULL) return;
  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = 0;
    value = strchr(line, '=');
    if (value == NULL) continue;
    *value++ = 0;
    if (strcmp(line, "name") == 0 && value[0]) launcherCopy(settings->name, USERNAME_MAX, value);
    if (strcmp(line, "host") == 0 && value[0]) launcherCopy(settings->host, LAUNCHER_HOST_MAX, value);
    if (strcmp(line, "port") == 0 && value[0]) launcherCopy(settings->port, LAUNCHER_PORT_MAX, value);
    if (strcmp(line, "bots") == 0) settings->bots = atoi(value);
    if (strcmp(line, "sound") == 0) settings->sound = atoi(value);
    if (strcmp(line, "music") == 0) settings->music = atoi(value);
    if ((option = gameOptionFind(line)) >= 0) settings->options[option] = atoi(value);
  }
  fclose(file);
  if (settings->bots < 0) settings->bots = 0;
  if (settings->bots > LAUNCHER_MAX_BOTS) settings->bots = LAUNCHER_MAX_BOTS;
  if (settings->sound < 0 || settings->sound > AUDIO_VOLUME_MAX) settings->sound = 8;
  if (settings->music < 0 || settings->music > AUDIO_VOLUME_MAX) settings->music = 5;
  for (i=0; i < OPTION_COUNT; i++) settings->options[i] = gameOptionClamp(i, settings->options[i]);
  gameOptionsFix(settings->options);
}

static void launcherSaveSettings(const LauncherSettings *settings) {
  char *path = launcherSettingsPath();
  FILE *file;
  int i;
  if (path == NULL || (file = fopen(path, "w")) == NULL) return;
  fprintf(file, "name=%s\nhost=%s\nport=%s\nbots=%d\n",
          settings->name, settings->host, settings->port, settings->bots);
  fprintf(file, "sound=%d\nmusic=%d\n", settings->sound, settings->music);
  for (i=0; i < OPTION_COUNT; i++) fprintf(file, "%s=%d\n", gameOptions[i].name, settings->options[i]);
  fclose(file);
}

/* Starting the server and bots */

// Run one of our programs from the folder this one is in, with a
// NULL-terminated argument list after the program name. Their output is
// thrown away; it is debug logging nobody sees.
static SDL_Process *launcherSpawn(const char *program, const char **argList) {
  char path[1024];
  const char *args[LAUNCHER_MAX_ARGS + 2];
  SDL_PropertiesID props;
  SDL_Process *process;
  int i;

  snprintf(path, sizeof(path), "%s%s%s", SDL_GetBasePath(), program, LAUNCHER_EXE_SUFFIX);
  args[0] = path;
  for (i=0; i < LAUNCHER_MAX_ARGS && argList[i] != NULL; i++) args[i+1] = argList[i];
  args[i+1] = NULL;

  props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
#ifdef _WIN32
  // Background processes get no console window
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
#else
  // Stay a normal child (so Ctrl+C in a terminal stops everything), but quiet
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
#endif
  process = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  return process;
}

static void launcherStop(SDL_Process *process) {
  int exitcode;
  if (process == NULL) return;
  SDL_KillProcess(process, true);
  SDL_WaitProcess(process, true, &exitcode);
  SDL_DestroyProcess(process);
}

static int launcherValidPort(Launcher *l) {
  int port = atoi(l->settings.port);
  if (port < 1 || port > 65535) {
    snprintf(l->status, LEN, "Enter a port number from 1 to 65535");
    return 0;
  }
  return port;
}

static int launcherValidName(Launcher *l) {
  if (l->settings.name[0] == 0) {
    snprintf(l->status, LEN, "Enter a name for your ship");
    return 0;
  }
  return 1;
}

// Report how a game ended. Returns 0 if the player closed the window.
static int launcherGameOver(Launcher *l, int result) {
  if (result == GAME_FAILED) launcherCopy(l->status, LEN, clientStatusMessage);
  return result != GAME_CLOSED;
}

static void launcherHostGame(Launcher *l) {
  SDL_Process *server;
  SDL_Process *bots[LAUNCHER_MAX_BOTS];
  char botName[USERNAME_MAX];
  const char *args[LAUNCHER_MAX_ARGS + 1];
  char flags[OPTION_COUNT][LEN], values[OPTION_COUNT][16];
  int port, i, n, exitcode, result;

  if (!launcherValidName(l) || !(port = launcherValidPort(l))) return;
  launcherSaveSettings(&l->settings);

  // Listen on every interface so other computers can join
  gui_draw_status("Starting server...", NULL);
  n = 0;
  args[n++] = "0.0.0.0";
  args[n++] = l->settings.port;
  args[n++] = "--silent"; // Its output goes nowhere
  // It shuts down when we leave, even if this game crashes
  args[n++] = "--host-player";
  args[n++] = l->settings.name;
  for (i=0; i < OPTION_COUNT; i++) {
    snprintf(flags[i], LEN, "--%s", gameOptions[i].name);
    snprintf(values[i], sizeof(values[i]), "%d", l->settings.options[i]);
    args[n++] = flags[i];
    args[n++] = values[i];
  }
  args[n] = NULL;
  server = launcherSpawn("SpaceWarServer", args);
  if (server == NULL) {
    snprintf(l->status, LEN, "Could not start SpaceWarServer: %s", SDL_GetError());
    return;
  }

  // The server stops right away if it cannot use the port
  SDL_Delay(300);
  if (SDL_WaitProcess(server, false, &exitcode)) {
    SDL_DestroyProcess(server);
    snprintf(l->status, LEN, "The server could not start. Is port %d already in use?", port);
    return;
  }

  for (i=0; i < l->settings.bots; i++) {
    snprintf(botName, USERNAME_MAX, "Bot %d", i + 1);
    args[0] = "127.0.0.1";
    args[1] = l->settings.port;
    args[2] = botName;
    args[3] = NULL;
    bots[i] = launcherSpawn("SpaceWarAI", args);
  }

  result = gamePlay("127.0.0.1", port, l->settings.name);

  for (i=0; i < l->settings.bots; i++) launcherStop(bots[i]);
  launcherStop(server);
  l->running = launcherGameOver(l, result);
}

static void launcherJoinGame(Launcher *l) {
  int port;
  if (!launcherValidName(l) || !(port = launcherValidPort(l))) return;
  if (l->settings.host[0] == 0) {
    snprintf(l->status, LEN, "Enter the server's address");
    return;
  }
  launcherSaveSettings(&l->settings);
  l->running = launcherGameOver(l, gamePlay(l->settings.host, port, l->settings.name));
}

/* Menus */

static LauncherItem *launcherAdd(Launcher *l, LauncherItemKind kind, const char *label) {
  LauncherItem *item = &l->items[l->itemCount++];
  memset(item, 0, sizeof(*item));
  item->kind = kind;
  item->label = label;
  return item;
}

static void launcherAddButton(Launcher *l, const char *label, LauncherAction action) {
  launcherAdd(l, ITEM_BUTTON, label)->action = action;
}

static void launcherAddText(Launcher *l, const char *label, char *text, int size, int digitsOnly) {
  LauncherItem *item = launcherAdd(l, ITEM_TEXT, label);
  item->text = text;
  item->textSize = size;
  item->digitsOnly = digitsOnly;
}

static LauncherItem *launcherAddChoice(Launcher *l, const char *label, int *value, int minValue, int maxValue) {
  LauncherItem *item = launcherAdd(l, ITEM_CHOICE, label);
  item->value = value;
  item->minValue = minValue;
  item->maxValue = maxValue;
  item->step = 1;
  item->wrap = 1;
  return item;
}

// A game rule: stops at the ends of its range instead of wrapping
static void launcherAddOption(Launcher *l, int option) {
  const struct gameOption *o = &gameOptions[option];
  LauncherItem *item = launcherAddChoice(l, o->label, &l->settings.options[option], o->minValue, o->maxValue);
  item->step = o->step;
  item->wrap = 0;
  item->help = o->help;
}

// Slide a scrolling screen's rows by l->scroll, keeping a row chosen with
// the keyboard in view, and hide the rows outside the list area
static void launcherScroll(Launcher *l) {
  LauncherItem *focus = &l->items[l->focus];
  float bottom = LAUNCHER_LIST_TOP;
  int i;

  for (i=0; i < l->itemCount; i++) {
    if (l->items[i].kind != ITEM_BUTTON && l->items[i].box.y + l->items[i].box.h > bottom) {
      bottom = l->items[i].box.y + l->items[i].box.h;
    }
  }
  if (l->followFocus && focus->kind != ITEM_BUTTON) {
    if (focus->box.y - l->scroll < LAUNCHER_LIST_TOP) l->scroll = focus->box.y - LAUNCHER_LIST_TOP;
    if (focus->box.y + focus->box.h - l->scroll > LAUNCHER_LIST_BOTTOM) {
      l->scroll = focus->box.y + focus->box.h - LAUNCHER_LIST_BOTTOM;
    }
  }
  l->followFocus = 0;
  l->scrollMax = bottom - LAUNCHER_LIST_BOTTOM;
  if (l->scrollMax < 0) l->scrollMax = 0;
  if (l->scroll > l->scrollMax) l->scroll = l->scrollMax;
  if (l->scroll < 0) l->scroll = 0;

  for (i=0; i < l->itemCount; i++) {
    LauncherItem *item = &l->items[i];
    if (item->kind == ITEM_BUTTON) continue;
    item->box.y -= l->scroll;
    item->hidden = (item->box.y < LAUNCHER_LIST_TOP - 1 || item->box.y + item->box.h > LAUNCHER_LIST_BOTTOM + 1);
  }
}

// Decide what is on the current screen and where each item goes
static void launcherBuildScreen(Launcher *l) {
  int i, scrolling;
  float y;

  l->itemCount = 0;
  if (l->screen == SCREEN_MAIN) {
    launcherAddButton(l, "Host Game", ACTION_SHOW_HOST);
    launcherAddButton(l, "Join Game", ACTION_SHOW_JOIN);
    launcherAddButton(l, "Options", ACTION_SHOW_OPTIONS);
    launcherAddButton(l, "About", ACTION_SHOW_ABOUT);
    launcherAddButton(l, "Quit", ACTION_QUIT);
  } else if (l->screen == SCREEN_HOST) {
    launcherAddText(l, "Your name", l->settings.name, USERNAME_MAX, 0);
    launcherAddText(l, "Port", l->settings.port, LAUNCHER_PORT_MAX, 1);
    launcherAddChoice(l, "AI bots", &l->settings.bots, 0, LAUNCHER_MAX_BOTS);
    launcherAddButton(l, "Start Game", ACTION_START_HOST);
    launcherAddButton(l, "Game Settings", ACTION_SHOW_SETTINGS);
    launcherAddButton(l, "Back", ACTION_BACK);
  } else if (l->screen == SCREEN_SETTINGS) {
    for (i=0; i < OPTION_COUNT; i++) launcherAddOption(l, i);
    launcherAddButton(l, "Reset to Defaults", ACTION_RESET_SETTINGS);
    launcherAddButton(l, "Back", ACTION_BACK);
  } else if (l->screen == SCREEN_ABOUT) {
    launcherAddButton(l, "Back", ACTION_BACK);
  } else if (l->screen == SCREEN_OPTIONS) {
    launcherAddChoice(l, "Sound volume", &l->settings.sound, 0, AUDIO_VOLUME_MAX)->wrap = 0;
    launcherAddChoice(l, "Music volume", &l->settings.music, 0, AUDIO_VOLUME_MAX)->wrap = 0;
    launcherAddButton(l, "Back", ACTION_BACK);
  } else {
    launcherAddText(l, "Your name", l->settings.name, USERNAME_MAX, 0);
    launcherAddText(l, "Server", l->settings.host, LAUNCHER_HOST_MAX, 0);
    launcherAddText(l, "Port", l->settings.port, LAUNCHER_PORT_MAX, 1);
    launcherAddButton(l, "Join Game", ACTION_START_JOIN);
    launcherAddButton(l, "Find Games", ACTION_FIND_GAMES);
    launcherAddButton(l, "Back", ACTION_BACK);
    for (i=0; i < l->foundCount; i++) launcherAdd(l, ITEM_GAME, NULL)->game = i;
  }

  // Fields are rows of label + box; buttons are centered below them. The
  // settings screen has more rows than fit, so they scroll between
  // LAUNCHER_LIST_TOP and LAUNCHER_LIST_BOTTOM and its buttons stay below.
  scrolling = (l->screen == SCREEN_SETTINGS);
  y = (l->screen == SCREEN_MAIN) ? 220 : (l->screen == SCREEN_ABOUT) ? 392 : scrolling ? LAUNCHER_LIST_TOP :
      (l->screen == SCREEN_JOIN) ? 96 : 124;
  for (i=0; i < l->itemCount; i++) {
    LauncherItem *item = &l->items[i];
    item->hidden = 0;
    if (item->kind == ITEM_BUTTON) {
      if (i > 0 && l->items[i-1].kind != ITEM_BUTTON) y = scrolling ? LAUNCHER_LIST_BOTTOM + 10 : y + 16;
      item->box.w = 300;
      item->box.x = (GUI_SCREEN_W - item->box.w) / 2;
    } else {
      item->box.x = 290;
      item->box.w = 240;
    }
    item->box.y = y;
    item->box.h = scrolling ? 24 : 28;
    y += scrolling ? 30 : 40;
    // The Join screen is packed tighter to fit the games found under it
    if (l->screen == SCREEN_JOIN) y -= (item->kind == ITEM_BUTTON) ? 6 : 4;
    if (item->kind == ITEM_GAME) {
      item->box.x = 60;
      item->box.w = GUI_SCREEN_W - 120;
      item->box.y = LAUNCHER_GAMES_TOP + item->game * 20;
      item->box.h = 18;
    }
  }
  if (l->focus >= l->itemCount) l->focus = 0;
  if (scrolling) launcherScroll(l);
}

// Look for games on the local network; the Join screen lists them as they answer
static void launcherFindGames(Launcher *l) {
  l->foundCount = 0;
  l->searching = 1;
  discoverStart();
}

// A game found on the network was chosen: fill in where to join it
static void launcherPickGame(Launcher *l, int game) {
  struct foundGame *found = &l->found[game];
  int i;
  if (!found->compatible) {
    snprintf(l->status, LEN, "That game runs version %s and you have %s", found->version, NSW_VERSION);
    return;
  }
  launcherCopy(l->settings.host, LAUNCHER_HOST_MAX, found->address);
  snprintf(l->settings.port, LAUNCHER_PORT_MAX, "%d", found->port);
  l->status[0] = 0;
  // On to the Join Game button
  for (i=0; i < l->itemCount; i++) {
    if (l->items[i].kind == ITEM_BUTTON && l->items[i].action == ACTION_START_JOIN) l->focus = i;
  }
}

static void launcherShowScreen(Launcher *l, LauncherScreen screen) {
  if (screen == SCREEN_JOIN) launcherFindGames(l);
  else {
    discoverStop();
    l->searching = 0;
  }
  l->screen = screen;
  l->focus = 0;
  l->scroll = 0;
  l->status[0] = 0;
  launcherBuildScreen(l);
}

// Where Back and Esc go from each screen
static LauncherScreen launcherParent(Launcher *l) {
  return l->screen == SCREEN_SETTINGS ? SCREEN_HOST : SCREEN_MAIN;
}

static void launcherActivate(Launcher *l, LauncherAction action) {
  switch (action) {
  case ACTION_SHOW_HOST:  launcherShowScreen(l, SCREEN_HOST); break;
  case ACTION_SHOW_JOIN:  launcherShowScreen(l, SCREEN_JOIN); break;
  case ACTION_BACK:       launcherShowScreen(l, launcherParent(l)); break;
  case ACTION_SHOW_SETTINGS: launcherShowScreen(l, SCREEN_SETTINGS); break;
  case ACTION_SHOW_ABOUT: launcherShowScreen(l, SCREEN_ABOUT); break;
  case ACTION_SHOW_OPTIONS: launcherShowScreen(l, SCREEN_OPTIONS); break;
  case ACTION_FIND_GAMES: l->status[0] = 0; launcherFindGames(l); break;
  case ACTION_RESET_SETTINGS:
    gameOptionsReset(l->settings.options);
    launcherSaveSettings(&l->settings);
    break;
  case ACTION_QUIT:       l->running = 0; break;
  case ACTION_START_HOST: l->status[0] = 0; launcherHostGame(l); break;
  case ACTION_START_JOIN: l->status[0] = 0; launcherJoinGame(l); break;
  default: break;
  }
  // Games turn text input off; menus need it on for typing
  SDL_StartTextInput(gui_get_window());
}

static void launcherChangeChoice(LauncherItem *item, int delta) {
  *item->value += delta * item->step;
  if (*item->value < item->minValue) *item->value = item->wrap ? item->maxValue : item->minValue;
  if (*item->value > item->maxValue) *item->value = item->wrap ? item->minValue : item->maxValue;
}

static void launcherTypeText(LauncherItem *item, const char *typed) {
  int len = (int)strlen(item->text);
  for (; *typed; typed++) {
    // The font only has plain ASCII
    if (*typed < 32 || *typed > 126) continue;
    if (item->digitsOnly && !isdigit((unsigned char)*typed)) continue;
    if (len >= item->textSize - 1) break;
    item->text[len++] = *typed;
    item->text[len] = 0;
  }
}

static int launcherItemAt(Launcher *l, float x, float y) {
  int i;
  SDL_FPoint point = {x, y};
  for (i=0; i < l->itemCount; i++) {
    if (!l->items[i].hidden && SDL_PointInRectFloat(&point, &l->items[i].box)) return i;
  }
  return -1;
}

static void launcherHandleKey(Launcher *l, SDL_Keycode key, SDL_Keymod mod) {
  LauncherItem *item = &l->items[l->focus];

  l->followFocus = 1; // If this moves the focus, scroll to it

  switch (key) {
  case SDLK_UP:
    l->focus = (l->focus + l->itemCount - 1) % l->itemCount;
    break;
  case SDLK_DOWN:
    l->focus = (l->focus + 1) % l->itemCount;
    break;
  case SDLK_TAB:
    if (mod & SDL_KMOD_SHIFT) l->focus = (l->focus + l->itemCount - 1) % l->itemCount;
    else l->focus = (l->focus + 1) % l->itemCount;
    break;
  case SDLK_LEFT:
    if (item->kind == ITEM_CHOICE) launcherChangeChoice(item, -1);
    break;
  case SDLK_RIGHT:
    if (item->kind == ITEM_CHOICE) launcherChangeChoice(item, 1);
    break;
  case SDLK_BACKSPACE:
    if (item->kind == ITEM_TEXT && item->text[0]) item->text[strlen(item->text) - 1] = 0;
    break;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    // Enter on a field moves on to the next line
    if (item->kind == ITEM_BUTTON) launcherActivate(l, item->action);
    else if (item->kind == ITEM_GAME) launcherPickGame(l, item->game);
    else l->focus = (l->focus + 1) % l->itemCount;
    break;
  case SDLK_ESCAPE:
    if (l->screen != SCREEN_MAIN) launcherShowScreen(l, launcherParent(l));
    break;
  default:
    break;
  }
}

static void launcherHandleEvent(Launcher *l, SDL_Event *event) {
  int i;
  LauncherItem *item;

  switch (event->type) {
  case SDL_EVENT_QUIT:
    l->running = 0;
    break;
  case SDL_EVENT_KEY_DOWN:
    launcherHandleKey(l, event->key.key, event->key.mod);
    break;
  case SDL_EVENT_TEXT_INPUT:
    item = &l->items[l->focus];
    if (item->kind == ITEM_TEXT) launcherTypeText(item, event->text.text);
    break;
  case SDL_EVENT_GAMEPAD_ADDED:
  case SDL_EVENT_GAMEPAD_REMOVED:
    gui_gamepad_event(event);
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    // The D-pad moves, A (the fire button) or Start chooses, B goes back
    i = event->gbutton.button;
    if (i == SDL_GAMEPAD_BUTTON_DPAD_UP) launcherHandleKey(l, SDLK_UP, 0);
    else if (i == SDL_GAMEPAD_BUTTON_DPAD_DOWN) launcherHandleKey(l, SDLK_DOWN, 0);
    else if (i == SDL_GAMEPAD_BUTTON_DPAD_LEFT) launcherHandleKey(l, SDLK_LEFT, 0);
    else if (i == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) launcherHandleKey(l, SDLK_RIGHT, 0);
    else if (i == (int)gui_pad_role(GUI_PAD_BACK)) launcherHandleKey(l, SDLK_ESCAPE, 0);
    else if (i == (int)gui_pad_role(GUI_PAD_FIRE) || i == SDL_GAMEPAD_BUTTON_START) {
      item = &l->items[l->focus];
      if (item->kind == ITEM_TEXT) {
        // On a text field (typed with the keyboard), go straight to the
        // screen's main button: Start Game or Join Game
        for (i=0; i < l->itemCount && l->items[i].kind != ITEM_BUTTON; i++);
        if (i < l->itemCount) l->focus = i;
      } else {
        launcherHandleKey(l, SDLK_RETURN, 0);
      }
    }
    break;
  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    // The left stick moves one step each time it is pushed past halfway
    if (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX || event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
      static int pushed[2];
      int which = (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY);
      int dir = event->gaxis.value > 16000 ? 1 : event->gaxis.value < -16000 ? -1 : 0;
      if (dir != 0 && dir != pushed[which]) {
        if (which) launcherHandleKey(l, dir > 0 ? SDLK_DOWN : SDLK_UP, 0);
        else launcherHandleKey(l, dir > 0 ? SDLK_RIGHT : SDLK_LEFT, 0);
      }
      pushed[which] = dir;
    }
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    l->scroll -= event->wheel.y * 30;
    break;
  case SDL_EVENT_MOUSE_MOTION:
    SDL_ConvertEventToRenderCoordinates(gui_get_renderer(), event);
    i = launcherItemAt(l, event->motion.x, event->motion.y);
    if (i >= 0) l->focus = i;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    SDL_ConvertEventToRenderCoordinates(gui_get_renderer(), event);
    i = launcherItemAt(l, event->button.x, event->button.y);
    if (i < 0) break;
    l->focus = i;
    item = &l->items[i];
    if (item->kind == ITEM_BUTTON) {
      launcherActivate(l, item->action);
    } else if (item->kind == ITEM_GAME) {
      launcherPickGame(l, item->game);
    } else if (item->kind == ITEM_CHOICE) {
      // Left half lowers the number, right half raises it
      launcherChangeChoice(item, (event->button.x < item->box.x + item->box.w / 2) ? -1 : 1);
    }
    break;
  default:
    break;
  }
}

// The default of the game option a settings item edits
static int launcherOptionDefault(Launcher *l, LauncherItem *item) {
  return gameOptions[item->value - l->settings.options].defaultValue;
}

static void launcherDrawItem(Launcher *l, int index) {
  LauncherItem *item = &l->items[index];
  int focused = (index == l->focus);
  int color = focused ? COLOR_FOCUS : COLOR_TEXT;
  float textY = item->box.y + (item->box.h - 16) / 2;
  float scale;
  char text[LEN];

  if (item->kind == ITEM_BUTTON) {
    if (focused) gui_outline_rect(item->box.x, item->box.y, item->box.w, item->box.h, COLOR_FOCUS);
    gui_text_centered(textY, 2.0f, color, item->label);
    return;
  }
  if (item->kind == ITEM_GAME) {
    // One line per game: whose it is, where, and how full; dim if we can't join it
    struct foundGame *game = &l->found[item->game];
    char where[96], who[40];
    if (game->host[0]) snprintf(who, sizeof(who), "%.19s's game", game->host);
    else snprintf(who, sizeof(who), "Server");
    snprintf(where, sizeof(where), "%s:%d", strcmp(game->address, "127.0.0.1") ? game->address : "this computer", game->port);
    if (game->compatible) snprintf(text, LEN, "%-24.24s %-24.24s %d/%d players", who, where, game->players, game->maxPlayers);
    else snprintf(text, LEN, "%-24.24s %-24.24s version %s", who, where, game->version);
    if (focused) gui_outline_rect(item->box.x, item->box.y, item->box.w, item->box.h, COLOR_FOCUS);
    gui_text(item->box.x + 6, item->box.y + 5, 1.0f, !game->compatible ? COLOR_DIM : focused ? COLOR_FOCUS : COLOR_TEXT, text);
    return;
  }

  // Label to the left, value in a box
  gui_text(item->box.x - 20 - gui_text_width(2.0f, item->label), textY, 2.0f, color, item->label);
  gui_fill_rect(item->box.x, item->box.y, item->box.w, item->box.h, COLOR_FIELD);
  gui_outline_rect(item->box.x, item->box.y, item->box.w, item->box.h, focused ? COLOR_FOCUS : COLOR_DIM);
  if (item->kind == ITEM_TEXT) {
    // Blinking cursor on the field being typed in
    int cursor = focused && (SDL_GetTicks() / 500) % 2 == 0;
    snprintf(text, LEN, "%s%s", item->text, cursor ? "_" : "");
    // Show the end of text too long for the box
    while (gui_text_width(2.0f, text) > item->box.w - 12) memmove(text, text + 1, strlen(text));
    gui_text(item->box.x + 6, textY, 2.0f, 0xffffff, text);
  } else {
    // Game options may show a name (None, Light...) instead of the number
    if (item->help) snprintf(text, LEN, "<  %s  >", gameOptionText(item->value - l->settings.options, *item->value));
    else snprintf(text, LEN, "<  %d  >", *item->value);
    // Long names ("Medium (1000)") get smaller text to fit the box
    scale = 2.0f;
    while (scale > 1.0f && gui_text_width(scale, text) > item->box.w - 12) scale -= 0.5f;
    // Rules changed from the default stand out
    gui_text(item->box.x + (item->box.w - gui_text_width(scale, text)) / 2,
             item->box.y + (item->box.h - 8 * scale) / 2, scale,
             (item->help && *item->value != launcherOptionDefault(l, item)) ? COLOR_FOCUS : 0xffffff, text);
  }
}

// Which gamepad is connected, and its controls (Options screen)
static void launcherDrawGamepad() {
  // Named the way this controller labels them (A or Cross...)
  int nintendo = gui_gamepad_nintendo();
  const char *controls[][2] = {
    {"Left stick / D-pad", "Turn"},
    {nintendo ? "ZR" : "Right trigger", "Thrust"},
    {gui_pad_role_name(GUI_PAD_BACK), "Thrust"},
    {gui_pad_role_name(GUI_PAD_FIRE), "Fire, choose in menus, fly again"},
    {gui_pad_role_name(GUI_PAD_JUMP), "Hyperjump"},
    {nintendo ? "L / R" : "Left / right bumper", "Power to shields / weapons"},
    {nintendo ? "- (hold)" : "Back (hold)", "Scores"},
    {nintendo ? "+" : "Start", "Leave the game"},
  };
  const char *name = gui_gamepad_name();
  char text[LEN];
  int i;

  snprintf(text, LEN, name ? "Gamepad: %.60s" : "No gamepad connected", name);
  gui_text_centered(270, 1.0f, name ? COLOR_TEXT : COLOR_DIM, text);
  for (i=0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++) {
    float y = 288 + i * 14;
    gui_text(300 - gui_text_width(1.0f, controls[i][0]), y, 1.0f, name ? COLOR_FOCUS : COLOR_DIM, controls[i][0]);
    gui_text(316, y, 1.0f, name ? COLOR_TEXT : COLOR_DIM, controls[i][1]);
  }
}

// The game's version, where it comes from, and the controls
static void launcherDrawAbout() {
  static const char *controls[][2] = {
    {"Left / Right", "Turn"},
    {"Up", "Thrust"},
    {"Space or Left Ctrl", "Fire"},
    {"Tab", "Hyperjump (and fly again after dying)"},
    {"Z / X", "Move energy into / out of shields"},
    {"S (hold)", "Scores and game rules"},
    {"M", "Sound and music off / on"},
    {"Esc", "Leave the game"},
  };
  char text[LEN];
  int i;

  gui_text_centered(40, 3.0f, COLOR_TITLE, "ABOUT");
  snprintf(text, LEN, "NetSpaceWar %s", NSW_VERSION);
  gui_text_centered(90, 2.0f, 0xffffff, text);
  gui_text_centered(116, 1.0f, COLOR_TEXT, "A multiplayer network clone of Spacewar!");
  gui_text_centered(132, 1.0f, COLOR_DIM, "Players need the same first two version numbers.");
  gui_text_centered(156, 1.0f, COLOR_TEXT, "github.com/josephcolton/NetSpaceWar");
  gui_text_centered(172, 1.0f, COLOR_DIM, "Free software under the GNU General Public License v3");

  gui_text_centered(208, 2.0f, COLOR_TITLE, "CONTROLS");
  for (i=0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++) {
    float y = 236 + i * 18;
    gui_text(300 - gui_text_width(1.0f, controls[i][0]), y, 1.0f, COLOR_FOCUS, controls[i][0]);
    gui_text(316, y, 1.0f, COLOR_TEXT, controls[i][1]);
  }
}

static void launcherDraw(Launcher *l) {
  int i, port, room;
  char info[LEN];
  LauncherItem *item;

  gui_draw_backdrop();
  if (l->screen == SCREEN_MAIN) {
    gui_text_centered(80, 5.0f, COLOR_TITLE, "NETSPACEWAR");
    gui_text_centered(140, 1.0f, COLOR_DIM, "Multiplayer Spacewar! over the network");
  } else if (l->screen == SCREEN_SETTINGS) {
    gui_text_centered(36, 3.0f, COLOR_TITLE, "GAME SETTINGS");
  } else if (l->screen == SCREEN_ABOUT) {
    launcherDrawAbout();
  } else if (l->screen == SCREEN_OPTIONS) {
    gui_text_centered(70, 3.0f, COLOR_TITLE, "OPTIONS");
    launcherDrawGamepad();
    gui_text_centered(406, 1.0f, COLOR_DIM, "These are yours: they apply to every game you host or join.");
    gui_text_centered(420, 1.0f, COLOR_DIM, "In a game, M turns all sound off and on.");
  } else {
    gui_text_centered(l->screen == SCREEN_HOST ? 70 : 44, 3.0f, COLOR_TITLE, l->screen == SCREEN_HOST ? "HOST A GAME" : "JOIN A GAME");
  }

  for (i=0; i < l->itemCount; i++) {
    if (!l->items[i].hidden) launcherDrawItem(l, i);
  }
  // Arrows when there are more rows above or below
  if (l->scroll > 0) gui_text(548, LAUNCHER_LIST_TOP + 4, 2.0f, COLOR_DIM, "^");
  if (l->scroll < l->scrollMax) gui_text(548, LAUNCHER_LIST_BOTTOM - 20, 2.0f, COLOR_DIM, "v");

  if (l->screen == SCREEN_JOIN) {
    gui_text(60, LAUNCHER_GAMES_TOP - 16, 1.0f, COLOR_TITLE,
             l->searching ? "Searching this network for games..." :
             l->foundCount ? "Games on this network (choose one to fill in its address):" :
                             "No games found on this network. Enter an address, or Find Games again.");
  }

  if (l->screen == SCREEN_HOST) {
    port = atoi(l->settings.port);
    room = MAX_COUNT_CLIENTS - 1 - l->settings.bots;
    snprintf(info, LEN, "Other players can join at %s port %d", l->localAddress, port);
    gui_text_centered(392, 1.0f, COLOR_TEXT, info);
    if (room == 0) snprintf(info, LEN, "No room for other players with %d bots", l->settings.bots);
    else snprintf(info, LEN, "Room for %d more player%s", room, room == 1 ? "" : "s");
    gui_text_centered(406, 1.0f, COLOR_DIM, info);
  }

  item = &l->items[l->focus];
  if (l->screen == SCREEN_SETTINGS && item->help) {
    snprintf(info, LEN, "%s (default %s)", item->help,
             gameOptionText(item->value - l->settings.options, launcherOptionDefault(l, item)));
    gui_text_centered(418, 1.0f, COLOR_TEXT, info);
  }

  if (l->status[0]) gui_text_centered(430, 1.0f, COLOR_ERROR, l->status);
  gui_text_centered(460, 1.0f, COLOR_DIM,
                    l->screen == SCREEN_MAIN ? "Up/Down or mouse to choose, Enter to select" :
                    l->screen == SCREEN_ABOUT ? "Enter or Esc to go back" :
                    l->screen == SCREEN_SETTINGS ? "Left/Right or click to change, Esc to go back"
                                                 : "Tab to move, Enter to select, Esc to go back");
  SDL_RenderPresent(gui_get_renderer());
}

int launcherRun() {
  static Launcher l; // Static: big, and lives as long as the program
  SDL_Event event;
  int vsync;

  memset(&l, 0, sizeof(l));
  launcherLoadSettings(&l.settings);
  netLocalAddress(l.localAddress, sizeof(l.localAddress));
  l.running = 1;

  gui_init_sdl();
  SDL_StartTextInput(gui_get_window());
  launcherShowScreen(&l, SCREEN_MAIN);

  audioSetVolume(l.settings.sound, l.settings.music);
  while (l.running) {
    // Calm music in the menus (a game switches it, so set it every pass)
    audioMusic(MUSIC_MENU);
    // Collect answers to a search for games, and list them as they come
    if (l.searching) {
      int before = l.foundCount;
      l.searching = discoverPoll(l.found, &l.foundCount);
      if (l.foundCount != before) launcherBuildScreen(&l);
    }
    while (l.running && SDL_PollEvent(&event)) {
      LauncherScreen oldScreen = l.screen;
      int oldFocus = l.focus;
      int oldValue = (l.items[l.focus].kind == ITEM_CHOICE) ? *l.items[l.focus].value : 0;

      launcherHandleEvent(&l, &event);
      // Fewer planets than volcanoes takes the extra volcanoes away
      gameOptionsFix(l.settings.options);
      // An action may have switched screens
      launcherBuildScreen(&l);

      // A blip for a new screen, a tick for moving or changing a value
      audioSetVolume(l.settings.sound, l.settings.music);
      if (l.screen != oldScreen) audioPlay(SOUND_MENU_SELECT);
      else if (l.focus != oldFocus) audioPlay(SOUND_MENU_MOVE);
      else if (l.items[l.focus].kind == ITEM_CHOICE && *l.items[l.focus].value != oldValue) audioPlay(SOUND_MENU_MOVE);
    }
    if (!l.running) break;
    launcherDraw(&l);
    // Presenting waits for the display when vsync works; otherwise don't spin
    if (!SDL_GetRenderVSync(gui_get_renderer(), &vsync) || vsync == 0) SDL_Delay(16);
  }

  launcherSaveSettings(&l.settings);
  return 0;
}
