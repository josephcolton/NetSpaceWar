#include "global.h"
#include "cdata.h"
#include "gui.h"
#include "game.h"
#include "launcher.h"
#include "options.h"

#define LAUNCHER_DEFAULT_PORT "27960"
#define LAUNCHER_MAX_BOTS     (MAX_COUNT_CLIENTS - 1) // Leave room for the host
#define LAUNCHER_HOST_MAX     64
#define LAUNCHER_PORT_MAX     6
#define LAUNCHER_MAX_ITEMS    (OPTION_COUNT + 4)
#define LAUNCHER_MAX_ARGS     (3 + 2 * OPTION_COUNT) // Server: address, port, --silent, --option value...

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
} LauncherSettings;

typedef enum { SCREEN_MAIN, SCREEN_HOST, SCREEN_JOIN, SCREEN_SETTINGS } LauncherScreen;
typedef enum { ITEM_BUTTON, ITEM_TEXT, ITEM_CHOICE } LauncherItemKind;
typedef enum {
  ACTION_NONE, ACTION_SHOW_HOST, ACTION_SHOW_JOIN, ACTION_QUIT,
  ACTION_START_HOST, ACTION_START_JOIN, ACTION_BACK,
  ACTION_SHOW_SETTINGS, ACTION_RESET_SETTINGS
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
    if ((option = gameOptionFind(line)) >= 0) settings->options[option] = atoi(value);
  }
  fclose(file);
  if (settings->bots < 0) settings->bots = 0;
  if (settings->bots > LAUNCHER_MAX_BOTS) settings->bots = LAUNCHER_MAX_BOTS;
  for (i=0; i < OPTION_COUNT; i++) settings->options[i] = gameOptionClamp(i, settings->options[i]);
}

static void launcherSaveSettings(const LauncherSettings *settings) {
  char *path = launcherSettingsPath();
  FILE *file;
  int i;
  if (path == NULL || (file = fopen(path, "w")) == NULL) return;
  fprintf(file, "name=%s\nhost=%s\nport=%s\nbots=%d\n",
          settings->name, settings->host, settings->port, settings->bots);
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

// Decide what is on the current screen and where each item goes
static void launcherBuildScreen(Launcher *l) {
  int i, dense;
  float y;

  l->itemCount = 0;
  if (l->screen == SCREEN_MAIN) {
    launcherAddButton(l, "Host Game", ACTION_SHOW_HOST);
    launcherAddButton(l, "Join Game", ACTION_SHOW_JOIN);
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
  } else {
    launcherAddText(l, "Your name", l->settings.name, USERNAME_MAX, 0);
    launcherAddText(l, "Server", l->settings.host, LAUNCHER_HOST_MAX, 0);
    launcherAddText(l, "Port", l->settings.port, LAUNCHER_PORT_MAX, 1);
    launcherAddButton(l, "Join Game", ACTION_START_JOIN);
    launcherAddButton(l, "Back", ACTION_BACK);
  }

  // Fields are rows of label + box; buttons are centered below them.
  // The settings screen has many rows, so they are packed closer.
  dense = (l->screen == SCREEN_SETTINGS);
  y = (l->screen == SCREEN_MAIN) ? 220 : dense ? 84 : 124;
  for (i=0; i < l->itemCount; i++) {
    LauncherItem *item = &l->items[i];
    if (item->kind == ITEM_BUTTON) {
      if (i > 0 && l->items[i-1].kind != ITEM_BUTTON) y += dense ? 8 : 16;
      item->box.w = 300;
      item->box.x = (GUI_SCREEN_W - item->box.w) / 2;
    } else {
      item->box.x = 290;
      item->box.w = 240;
    }
    item->box.y = y;
    item->box.h = dense ? 24 : 28;
    y += dense ? 29 : 40;
  }
  if (l->focus >= l->itemCount) l->focus = 0;
}

static void launcherShowScreen(Launcher *l, LauncherScreen screen) {
  l->screen = screen;
  l->focus = 0;
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
    if (SDL_PointInRectFloat(&point, &l->items[i].box)) return i;
  }
  return -1;
}

static void launcherHandleKey(Launcher *l, SDL_Keycode key, SDL_Keymod mod) {
  LauncherItem *item = &l->items[l->focus];

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
  char text[LEN];

  if (item->kind == ITEM_BUTTON) {
    if (focused) gui_outline_rect(item->box.x, item->box.y, item->box.w, item->box.h, COLOR_FOCUS);
    gui_text_centered(textY, 2.0f, color, item->label);
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
    snprintf(text, LEN, "<  %d  >", *item->value);
    // Rules changed from the default stand out
    gui_text(item->box.x + (item->box.w - gui_text_width(2.0f, text)) / 2, textY, 2.0f,
             (item->help && *item->value != launcherOptionDefault(l, item)) ? COLOR_FOCUS : 0xffffff, text);
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
  } else {
    gui_text_centered(70, 3.0f, COLOR_TITLE, l->screen == SCREEN_HOST ? "HOST A GAME" : "JOIN A GAME");
  }

  for (i=0; i < l->itemCount; i++) launcherDrawItem(l, i);

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
    snprintf(info, LEN, "%s (default %d)", item->help, launcherOptionDefault(l, item));
    gui_text_centered(410, 1.0f, COLOR_TEXT, info);
  }

  if (l->status[0]) gui_text_centered(430, 1.0f, COLOR_ERROR, l->status);
  gui_text_centered(460, 1.0f, COLOR_DIM,
                    l->screen == SCREEN_MAIN ? "Up/Down or mouse to choose, Enter to select" :
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

  while (l.running) {
    while (l.running && SDL_PollEvent(&event)) {
      launcherHandleEvent(&l, &event);
      // An action may have switched screens
      launcherBuildScreen(&l);
    }
    if (!l.running) break;
    launcherDraw(&l);
    // Presenting waits for the display when vsync works; otherwise don't spin
    if (!SDL_GetRenderVSync(gui_get_renderer(), &vsync) || vsync == 0) SDL_Delay(16);
  }

  launcherSaveSettings(&l.settings);
  return 0;
}
