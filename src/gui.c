#include <math.h>
#include <time.h>
#include <stdio.h>

#include "global.h"
#include "gui.h"
#include "cdata.h"
#include "audio.h"

/*
 * A rectangular region of the window that drawing functions work in, so
 * the radar can be drawn with the same code as the main view. Coordinates
 * passed to the gui_* primitives are relative to the area's top left corner.
 */
typedef struct {
  int x, y, w, h;
} GuiArea;

void gui_draw_ship(GuiArea *buffer, double rel_x, double rel_y, double angle, int eglow, int color);
void gui_draw_missile(GuiArea *buffer, double rel_x, double rel_y, double angle);
void gui_draw_hud(GuiArea *buffer, int shield, int energy);
void gui_draw_radar(GuiArea *buffer);
void gui_draw_stars(GuiArea *buffer, double mv_x, double mv_y);
void gui_draw_planet(GuiArea *buffer, double rel_x, double rel_y, double radius);
static void gui_draw_volcano(GuiArea *buffer, double rel_x, double rel_y, double size);
static void gui_draw_volcano_change(GuiArea *buffer, double rel_x, double rel_y, double progress, int woke);
static void gui_draw_orb(GuiArea *buffer, double rel_x, double rel_y, int life);
static void gui_draw_asteroid(GuiArea *buffer, double rel_x, double rel_y, object_t objectID, int hp);

#define GUI_VOLCANO_CHANGE 1.5 // Seconds a volcano takes to appear or die out
static void gui_draw_score();
static void gui_draw_events();
static void gui_draw_scoreboard();
static void gui_draw_rankings();

// The score that wins at each Match length (0 = Endless)
static const int gui_match_scores[MATCH_LENGTHS] = MATCH_SCORES;

/* Gamepad: the first one connected is used, whenever it is plugged in */
static SDL_Gamepad *gui_pad = NULL;

void gui_gamepad_event(SDL_Event *event) {
  SDL_JoystickID *pads;
  int count;
  if (event->type == SDL_EVENT_GAMEPAD_ADDED && gui_pad == NULL) {
    gui_pad = SDL_OpenGamepad(event->gdevice.which);
  }
  if (event->type == SDL_EVENT_GAMEPAD_REMOVED && gui_pad != NULL && SDL_GetGamepadID(gui_pad) == event->gdevice.which) {
    SDL_CloseGamepad(gui_pad);
    gui_pad = NULL;
    // Switch to another one if there is one
    pads = SDL_GetGamepads(&count);
    if (pads != NULL && count > 0) gui_pad = SDL_OpenGamepad(pads[0]);
    SDL_free(pads);
  }
}

int gui_gamepad_nintendo() {
  SDL_GamepadType type = gui_pad ? SDL_GetGamepadType(gui_pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
  return type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO || type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR ||
         type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT || type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT;
}

const char *gui_gamepad_name() {
  return gui_pad ? SDL_GetGamepadName(gui_pad) : NULL;
}

static int gui_pad_button(SDL_GamepadButton button) {
  return gui_pad != NULL && SDL_GetGamepadButton(gui_pad, button);
}

// A stick or trigger from -1 to 1 (triggers 0 to 1), ignoring a small dead zone
static double gui_pad_axis(SDL_GamepadAxis axis) {
  double value;
  if (gui_pad == NULL) return 0.0;
  value = SDL_GetGamepadAxis(gui_pad, axis) / 32767.0;
  if (fabs(value) < 0.25) return 0.0;
  value = (value > 0 ? value - 0.25 : value + 0.25) / 0.75;
  return value > 1.0 ? 1.0 : value < -1.0 ? -1.0 : value;
}

// The face button for a job (GUI_PAD_FIRE...), found by the label printed on
// it, since controllers put A, B, X and Y in different places (Xbox has A
// at the bottom, Nintendo on the right). PlayStation's Cross, Circle and
// Triangle count as A, B and Y. A controller with unknown labels gets the
// Xbox positions.
SDL_GamepadButton gui_pad_role(int role) {
  static const SDL_GamepadButton faces[4] = {
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH
  };
  static const SDL_GamepadButtonLabel wanted[3][2] = {
    {SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_CROSS},
    {SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE},
    {SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE},
  };
  static const SDL_GamepadButton fallback[3] = {
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_NORTH
  };
  SDL_GamepadButtonLabel label;
  int i;
  if (gui_pad != NULL) {
    for (i=0; i < 4; i++) {
      label = SDL_GetGamepadButtonLabel(gui_pad, faces[i]);
      if (label == wanted[role][0] || label == wanted[role][1]) return faces[i];
    }
  }
  return fallback[role];
}

// What the button for a job is called on this controller ("A", "Cross"...)
const char *gui_pad_role_name(int role) {
  static const char *names[3][2] = {{"A", "Cross"}, {"B", "Circle"}, {"Y", "Triangle"}};
  SDL_GamepadButtonLabel label = gui_pad ? SDL_GetGamepadButtonLabel(gui_pad, gui_pad_role(role)) : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
  int playstation = (label == SDL_GAMEPAD_BUTTON_LABEL_CROSS || label == SDL_GAMEPAD_BUTTON_LABEL_CIRCLE ||
                     label == SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE);
  return names[role][playstation];
}

SDL_Window *gui_window = NULL;
SDL_Renderer *gui_renderer = NULL;
GuiArea screen_area = {0, 0, GUI_SCREEN_W, GUI_SCREEN_H};
GuiArea radar_area = {GUI_SCREEN_W - GUI_SCREEN_W/5 - 1, 1, GUI_SCREEN_W/5, GUI_SCREEN_H/5};
GuiArea *scr_buff = &screen_area;
GuiArea *radar = &radar_area;
int initalized_sdl=0; // Global so it doesn't matter whether the display routine or the input routine is called first.
int planetcolor;

/* Drawing primitives, matching the Allegro calls this file was written with */
static int gui_makecol(int r, int g, int b) {
  return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
}

static void gui_set_color(int color) {
  SDL_SetRenderDrawColor(gui_renderer, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 255);
}

static void gui_line(GuiArea *a, double x1, double y1, double x2, double y2, int color) {
  gui_set_color(color);
  SDL_RenderLine(gui_renderer, a->x + (int)x1, a->y + (int)y1, a->x + (int)x2, a->y + (int)y2);
}

static void gui_hline(GuiArea *a, double x1, double y, double x2, int color) {
  gui_line(a, x1, y, x2, y, color);
}

static void gui_vline(GuiArea *a, double x, double y1, double y2, int color) {
  gui_line(a, x, y1, x, y2, color);
}

static void gui_putpixel(GuiArea *a, double x, double y, int color) {
  gui_set_color(color);
  SDL_RenderPoint(gui_renderer, a->x + (int)x, a->y + (int)y);
}

// Midpoint circle outline
static void gui_circle(GuiArea *a, double cx, double cy, double radius, int color) {
  int x0 = a->x + (int)cx, y0 = a->y + (int)cy;
  int r = (int)radius;
  int x = r, y = 0, err = 1 - r;
  SDL_FPoint pts[8];

  gui_set_color(color);
  while (x >= y) {
    pts[0].x = x0 + x; pts[0].y = y0 + y;
    pts[1].x = x0 + y; pts[1].y = y0 + x;
    pts[2].x = x0 - y; pts[2].y = y0 + x;
    pts[3].x = x0 - x; pts[3].y = y0 + y;
    pts[4].x = x0 - x; pts[4].y = y0 - y;
    pts[5].x = x0 - y; pts[5].y = y0 - x;
    pts[6].x = x0 + y; pts[6].y = y0 - x;
    pts[7].x = x0 + x; pts[7].y = y0 - y;
    SDL_RenderPoints(gui_renderer, pts, 8);
    y++;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

static void gui_circlefill(GuiArea *a, double cx, double cy, double radius, int color) {
  int x0 = a->x + (int)cx, y0 = a->y + (int)cy;
  int r = (int)radius;
  int dy, dx;

  gui_set_color(color);
  for (dy = -r; dy <= r; dy++) {
    dx = (int)sqrt((double)(r * r - dy * dy));
    SDL_RenderLine(gui_renderer, x0 - dx, y0 + dy, x0 + dx, y0 + dy);
  }
}

// Fill the area with black
static void gui_clear(GuiArea *a) {
  SDL_FRect rect = {a->x, a->y, a->w, a->h};
  SDL_SetRenderDrawColor(gui_renderer, 0, 0, 0, 255);
  SDL_RenderFillRect(gui_renderer, &rect);
}

void gui_update_display() {
  double xcoord = 0.0;
  double ycoord = 0.0;
  double orientation = 0.0;
  object_t objectID = 0;
  struct dataNodeShips *clientShip = NULL;
  double player_x = 0.0;
  double player_y = 0.0;
  static double player_oldx = 0.0;
  static double player_oldy = 0.0;
  int player_col = gui_makecol(160,160,160);
  int other_col = gui_makecol(255,160,160);
  int i;
  double exp_norm,fade;
  double since; // Seconds since a planet's volcano changed

  if (DEBUG) printf("gui_update_display()\n");

  // make sure SDL has been initialized.
  if (initalized_sdl==0) {
    // Since the client is single threaded this stuff is atomic.
    gui_init_sdl();
    initalized_sdl=1;
  }

  gui_clear(scr_buff);


  // -- set player_x and player_y from the player's position
  clientShip = cdataLookupShips(clientObjectID);
  if (clientShip == NULL) {
    if (DEBUG) printf("clientShip NULL!\n");
    return;
  } else {
    player_x = clientShip->xcoord;
    player_y = clientShip->ycoord;
  }

  gui_draw_stars(scr_buff,player_x-player_oldx,player_y-player_oldy);

  if (DEBUG) printf("NDEBUG1: before pox=%f, poy=%f\n", player_oldx, player_oldy);
  if (DEBUG) printf("NDEBUG1: before pcx=%f, pcy=%f\n", player_x, player_y);

  player_oldx=player_x;
  player_oldy=player_y;

  if (DEBUG) printf("NDEBUG1: after pox=%f, poy=%f\n", player_oldx, player_oldy);
  if (DEBUG) printf("NDEBUG1: after pcx=%f, pcy=%f\n", player_x, player_y);


  // Draw Planets
  for (i=0; i < cdataCountPlanets; i++) {
    if (cdataArrayPlanets[i] == NULL) continue;
    xcoord = cdataArrayPlanets[i]->xcoord;
    ycoord = cdataArrayPlanets[i]->ycoord;
    objectID = cdataArrayPlanets[i]->objectID;

    xcoord -= player_x;
    if (xcoord < SPACE_MIN_XCOORD) xcoord += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    if (xcoord > SPACE_MAX_XCOORD) xcoord -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    ycoord -= player_y;
    if (ycoord < SPACE_MIN_YCOORD) ycoord += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    if (ycoord > SPACE_MAX_YCOORD) ycoord -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);


    gui_draw_planet(scr_buff, xcoord ,ycoord, P_GUI_DEFAULT_PLANET_SIZE);
    since = cdataPlanetChanged[i] > 0 ? platformNow() - cdataPlanetChanged[i] : GUI_VOLCANO_CHANGE;
    if (since < GUI_VOLCANO_CHANGE) {
      gui_draw_volcano_change(scr_buff, xcoord, ycoord, since / GUI_VOLCANO_CHANGE, cdataArrayPlanets[i]->volcano);
    } else if (cdataArrayPlanets[i]->volcano) {
      gui_draw_volcano(scr_buff, xcoord, ycoord, 1.0);
    }
  }

  // Draw Asteroids
  for (i=0; i < cdataCountAsteroids; i++) {
    xcoord = cdataAsteroids[i].xcoord - player_x;
    if (xcoord < SPACE_MIN_XCOORD) xcoord += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    if (xcoord > SPACE_MAX_XCOORD) xcoord -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    ycoord = cdataAsteroids[i].ycoord - player_y;
    if (ycoord < SPACE_MIN_YCOORD) ycoord += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    if (ycoord > SPACE_MAX_YCOORD) ycoord -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    gui_draw_asteroid(scr_buff, xcoord, ycoord, cdataAsteroids[i].objectID, cdataAsteroids[i].hp);
  }

  // Draw Ships
  for (i=0; i < cdataCountShips; i++) {
    if (cdataArrayShips[i] == NULL) continue;
    if (cdataArrayShips[i]->status==0) continue;
    xcoord = cdataArrayShips[i]->xcoord;
    ycoord = cdataArrayShips[i]->ycoord;
    objectID = cdataArrayShips[i]->objectID;
    orientation = cdataArrayShips[i]->orientation;

    if (objectID == clientObjectID) {
      // This is the client's ship, color appropriately
      gui_draw_ship(scr_buff,
		    0,
		    0,
		    orientation,
		    0,
		    player_col);



    } else {
      // This is not the client's ship
      // Figure out the adjusted 
      xcoord -= player_x;
      if (xcoord < SPACE_MIN_XCOORD) xcoord += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
      if (xcoord > SPACE_MAX_XCOORD) xcoord -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
      ycoord -= player_y;
      if (ycoord < SPACE_MIN_YCOORD) ycoord += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
      if (ycoord > SPACE_MAX_YCOORD) ycoord -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);

      gui_draw_ship(scr_buff,
		    xcoord,
		    ycoord,
		    orientation,
		    0,
		    other_col);
    }
  }

  
  // Draw Missiles
  for (i=0; i < cdataCountMissiles; i++) {
    if (cdataArrayMissiles[i] == NULL) continue;
    xcoord = cdataArrayMissiles[i]->xcoord;
    ycoord = cdataArrayMissiles[i]->ycoord;
    orientation = cdataArrayMissiles[i]->orientation;

    if (DEBUG) printf("NDEBUG3:%f\t%f\t%f\t%f\t%f\n",
	   (xcoord-player_x)*(xcoord-player_x)+(ycoord-player_y)*(ycoord-player_y),
	   xcoord,ycoord,
	   player_x,player_y);


    // Adjust missile position
    xcoord -= player_x;
    if (xcoord < SPACE_MIN_XCOORD) xcoord += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    if (xcoord > SPACE_MAX_XCOORD) xcoord -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    ycoord -= player_y;
    if (ycoord < SPACE_MIN_YCOORD) ycoord += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    if (ycoord > SPACE_MAX_YCOORD) ycoord -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);

    // Draw the missile
    gui_draw_missile(scr_buff, xcoord, ycoord, orientation);
  }

  // Draw recharge orbs
  for (i=0; i < cdataCountOrbs; i++) {
    xcoord = cdataOrbs[i].xcoord - player_x;
    if (xcoord < SPACE_MIN_XCOORD) xcoord += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    if (xcoord > SPACE_MAX_XCOORD) xcoord -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    ycoord = cdataOrbs[i].ycoord - player_y;
    if (ycoord < SPACE_MIN_YCOORD) ycoord += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    if (ycoord > SPACE_MAX_YCOORD) ycoord -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    gui_draw_orb(scr_buff, xcoord, ycoord, cdataOrbs[i].life);
  }

  // Draw Explosions
  if (DEBUG) printf("Draw Explosions: %i\n", cdataCountExplosions);
  for (i=0; i < cdataCountExplosions; i++) {
    if (cdataArrayExplosions[i] == NULL) continue;
    xcoord = cdataArrayExplosions[i]->xcoord;
    ycoord = cdataArrayExplosions[i]->ycoord;

    xcoord -= player_x;
    if (xcoord < SPACE_MIN_XCOORD) xcoord += (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    if (xcoord > SPACE_MAX_XCOORD) xcoord -= (SPACE_MAX_XCOORD - SPACE_MIN_XCOORD);
    ycoord -= player_y;
    if (ycoord < SPACE_MIN_YCOORD) ycoord += (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);
    if (ycoord > SPACE_MAX_YCOORD) ycoord -= (SPACE_MAX_YCOORD - SPACE_MIN_YCOORD);

    fade=((double)cdataArrayExplosions[i]->timeToLive)/EXPLOSION_TTL;
    exp_norm=1.0-fade;
    
    //    fade=sqrt(sqrt(fade));

    gui_circle(scr_buff, 
	   scr_buff->w/2 + xcoord*P_GUI_RADIUS_FUNC(scr_buff)/P_GUI_SHIP_RADIUS_INTERNAL ,
	   scr_buff->h/2 + ycoord*P_GUI_RADIUS_FUNC(scr_buff)/P_GUI_SHIP_RADIUS_INTERNAL  ,
	   exp_norm*P_GUI_EXPLOSION_RADIUS_INT*P_GUI_RADIUS_FUNC(scr_buff)/P_GUI_SHIP_RADIUS_INTERNAL
	   ,gui_makecol(fade*255,fade*160,0));

    cdataArrayExplosions[i]->timeToLive-=1;
    if (cdataArrayExplosions[i]->timeToLive <=0) 
    {
      fprintf(stderr,"%i,%i\n",i,cdataCountExplosions);
      cdataDeleteExplosion(i);
      i-=1;
    }
  }
  gui_draw_hud(scr_buff,clientShip->shields,clientShip->energy);
  gui_draw_score();

  // At the end of a match, the final rankings take over the screen
  if (cdataMatchOver) {
    gui_draw_rankings();
    SDL_RenderPresent(gui_renderer);
    return;
  }

  gui_draw_events();

  // Show everyone's scores while S is held, and while waiting to respawn
  if (clientShip->status == 0) {
    gui_text_centered(80, 2.0f, 0xffffff, "Your ship was destroyed");
    if (gui_pad) {
      char again[LEN];
      snprintf(again, LEN, "Press %s or %s to fly again", gui_pad_role_name(GUI_PAD_FIRE), gui_pad_role_name(GUI_PAD_JUMP));
      gui_text_centered(104, 1.0f, 0xc0c0c0, again);
    } else {
      gui_text_centered(104, 1.0f, 0xc0c0c0, "Press Space or Tab to fly again");
    }
  }
  if (clientShip->status == 0 || SDL_GetKeyboardState(NULL)[SDL_SCANCODE_S] || gui_pad_button(SDL_GAMEPAD_BUTTON_BACK)) {
    gui_draw_scoreboard();
  }

  SDL_RenderPresent(gui_renderer);
}







int gui_get_input(double *angle_change, double *accel_amount, int *energy_to_shields, int *flags)
{
	static Uint64 last_time;
	static int first_call=0;
	static double energy_smoothing_buffer=0;
	static int quit_requested=0;
	static long long int time_to_hyper=0,time_to_fire=0;
	Uint64 now;
	long long int time_diff;
	double seconds_diff;
	const bool *key;
	SDL_Event event;
	double turn, thrust;

	if (DEBUG) printf("gui_get_input(%.2f, %.2f, %d, %d)\n",
			  *angle_change, *accel_amount, *energy_to_shields, *flags);

	if (initalized_sdl==0) // Since we need SDL for input, make sure it has been initalized.
	{
		gui_init_sdl(); // Since the client is single threaded this stuff is atomic.
		initalized_sdl=1;
	}

	if (first_call==0)
	{
		first_call=1;
		last_time=SDL_GetTicks();
	}

	// Process window events; closing the window quits like ESC does
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) quit_requested=1;
		gui_gamepad_event(&event);
		// M turns all sound and music off and on
		if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_M && !event.key.repeat) audioToggleMute();
	}

	// How long has it been since we were last called?
	// we will assume that the user pressed any keys that are down right after the last call.
	now=SDL_GetTicks();
	time_diff=(long long int)(now-last_time);
	last_time=now;
	seconds_diff=((double)time_diff)/(1000.0);

	if (DEBUG) printf("Seconds since last update: %f\n", seconds_diff);

	*angle_change=0;
	*accel_amount=0;
	*energy_to_shields=0;
	*flags=0;
	if (quit_requested) *flags|=P_GUI_FLAG_CLOSE;
	if (time_diff<=0) // No time has passed, so no movement to report.
	{
		return quit_requested ? 0 : -1;
	}

	// Now read the keys and do something based of what they say.
	key=SDL_GetKeyboardState(NULL);

	// Turning and thrust can be partial on a gamepad's stick and trigger;
	// the keys and the D-pad are all or nothing
	turn = gui_pad_axis(SDL_GAMEPAD_AXIS_LEFTX);
	if (key[SDL_SCANCODE_LEFT] || gui_pad_button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) turn = -1;
	if (key[SDL_SCANCODE_RIGHT] || gui_pad_button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) turn = 1;
	*angle_change = turn * seconds_diff * P_GUI_ANGLE_PER_SECOND;

	thrust = gui_pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
	// B (back in the menus) also thrusts, beside A's fire
	if (key[SDL_SCANCODE_UP] || gui_pad_button(SDL_GAMEPAD_BUTTON_DPAD_UP) || gui_pad_button(gui_pad_role(GUI_PAD_BACK))) thrust = 1;
	*accel_amount = thrust * seconds_diff * P_GUI_ACCEL_PER_SECOND;

	if (key[SDL_SCANCODE_SPACE]||key[SDL_SCANCODE_LCTRL]||gui_pad_button(gui_pad_role(GUI_PAD_FIRE))) // fire
	{
		*flags|=P_GUI_FLAG_FIRE;
	}
	if (key[SDL_SCANCODE_TAB]||gui_pad_button(gui_pad_role(GUI_PAD_JUMP))) // hyper
	{
		*flags|=P_GUI_FLAG_HYPER;
	}
	if (key[SDL_SCANCODE_ESCAPE]||gui_pad_button(SDL_GAMEPAD_BUTTON_START)) // quit
	{
		*flags|=P_GUI_FLAG_QUIT;
	}

	if (key[SDL_SCANCODE_Z]||gui_pad_button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) // energy to shield
	{
		*energy_to_shields=( seconds_diff * P_GUI_ENERGY_MOVE_PER_SECOND );
		energy_smoothing_buffer+=( seconds_diff * P_GUI_ENERGY_MOVE_PER_SECOND )-(double)(*energy_to_shields);
		if (energy_smoothing_buffer>1)
		{
			++*energy_to_shields;
			--energy_smoothing_buffer;
		}
	}

	if (key[SDL_SCANCODE_X]||gui_pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) // energy from shield
	{
		*energy_to_shields=-( seconds_diff * P_GUI_ENERGY_MOVE_PER_SECOND );
		energy_smoothing_buffer-=( seconds_diff * P_GUI_ENERGY_MOVE_PER_SECOND )-(double)(*energy_to_shields);
		if (energy_smoothing_buffer<-1)
		{
			--*energy_to_shields;
			++energy_smoothing_buffer;
		}
	}

	if (time_to_hyper>0) {
	  *flags &= ~P_GUI_FLAG_HYPER;
	  time_to_hyper-=time_diff;
	}

	if (time_to_fire>0) {
	  *flags&= ~P_GUI_FLAG_FIRE;
	  time_to_fire-=time_diff;
	}

	if (*flags&P_GUI_FLAG_HYPER) {
	  time_to_hyper=P_GUI_SECONDS_PER_JUMP*1000;
	}
	
	if (*flags&P_GUI_FLAG_FIRE) {
	  time_to_fire=P_GUI_SECONDS_PER_SHOT*1000;
	}
	
	return 0;
}


void gui_init_sdl()
{
	SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;

	if (gui_renderer != NULL) return;
	initalized_sdl=1;

	if (FULLSCREEN) flags |= SDL_WINDOW_FULLSCREEN;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) ||
	    !SDL_CreateWindowAndRenderer("NetSpaceWar", GUI_SCREEN_W, GUI_SCREEN_H, flags, &gui_window, &gui_renderer))
	{ /* The display wouldn't set up properly. Die horribly. */
		fprintf(stdout,"Display Error!!\n Error: %s\n", SDL_GetError());
		SDL_Quit();
		exit(1);
	}

	// Draw at 640x480 and let SDL scale that to whatever size the window is
	SDL_SetRenderLogicalPresentation(gui_renderer, GUI_SCREEN_W, GUI_SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	SDL_SetRenderVSync(gui_renderer, 1);
	planetcolor=gui_makecol(140,140,100);
	audioInit();
}


void gui_draw_planet(GuiArea *buffer, double rel_x, double rel_y, double radius)
{

	// I have internal units x I want pixels. shipr=x/P_GUI_SHIP_RADIUS_INTERNAL, pix=shipr*P_GUI_RADIUS_FUNC(buffer)
	// pix=P_GUI_RADIUS_FUNC*x/P_GUI_RADIUS_INTERNAL

	gui_circlefill(buffer,
	       buffer->w/2 + rel_x*P_GUI_RADIUS_FUNC(buffer)/P_GUI_SHIP_RADIUS_INTERNAL ,
	       buffer->h/2 + rel_y*P_GUI_RADIUS_FUNC(buffer)/P_GUI_SHIP_RADIUS_INTERNAL  ,
	       radius*P_GUI_RADIUS_FUNC(buffer)/P_GUI_SHIP_RADIUS_INTERNAL , planetcolor);



}

// A volcano in the middle of a planet, seen from above: lava cracks running
// out from a dark crater rim around slowly pulsing lava
// size (0 to 1) shrinks it, for the appear and die out animation.
static void gui_draw_volcano(GuiArea *buffer, double rel_x, double rel_y, double size) {
  double scale = P_GUI_RADIUS_FUNC(buffer) / P_GUI_SHIP_RADIUS_INTERNAL; // Pixels per internal unit
  double cx = buffer->w/2 + rel_x*scale, cy = buffer->h/2 + rel_y*scale;
  double s = scale * size;
  double glow = 0.5 + 0.5 * sin(SDL_GetTicks() / 1000.0 * M_PI); // Two second pulse
  int lava = gui_makecol(255, 90 + (int)(90*glow), (int)(40*glow));
  int crack = gui_makecol(190, 60 + (int)(50*glow), 0);
  double a;
  int k;

  if (size <= 0.0) return;
  // Each crack is two jagged segments
  for (k=0; k < 6; k++) {
    a = k * M_PI / 3 + 0.3;
    gui_line(buffer, cx + cos(a)*1.6*s, cy + sin(a)*1.6*s,
	     cx + cos(a + 0.18)*2.7*s, cy + sin(a + 0.18)*2.7*s, crack);
    gui_line(buffer, cx + cos(a + 0.18)*2.7*s, cy + sin(a + 0.18)*2.7*s,
	     cx + cos(a - 0.05)*3.7*s, cy + sin(a - 0.05)*3.7*s, crack);
  }
  gui_circlefill(buffer, cx, cy, 2.0*s, gui_makecol(85, 60, 40));
  gui_circle(buffer, cx, cy, 2.0*s, gui_makecol(55, 38, 25));
  gui_circlefill(buffer, cx, cy, 1.2*s, lava);
}

// An asteroid strike just changed this planet. progress runs 0 to 1.
// Waking: a hot shockwave spreads over the planet as the volcano grows.
// Dying out: the volcano shrinks under a spreading ring of gray ash.
static void gui_draw_volcano_change(GuiArea *buffer, double rel_x, double rel_y, double progress, int woke) {
  double scale = P_GUI_RADIUS_FUNC(buffer) / P_GUI_SHIP_RADIUS_INTERNAL;
  double cx = buffer->w/2 + rel_x*scale, cy = buffer->h/2 + rel_y*scale;
  double fade = 1.0 - progress;
  double radius;

  if (woke) {
    gui_draw_volcano(buffer, rel_x, rel_y, progress);
    radius = (1.0 + progress * 9.0) * scale;
    gui_circle(buffer, cx, cy, radius, gui_makecol((int)(255*fade), (int)(150*fade), 0));
    gui_circle(buffer, cx, cy, radius * 0.8, gui_makecol((int)(255*fade), (int)(220*fade), (int)(80*fade)));
  } else {
    gui_draw_volcano(buffer, rel_x, rel_y, fade);
    radius = (1.5 + progress * 5.0) * scale;
    gui_circle(buffer, cx, cy, radius, gui_makecol((int)(170*fade), (int)(170*fade), (int)(170*fade)));
    gui_circle(buffer, cx, cy, radius * 0.7, gui_makecol((int)(120*fade), (int)(120*fade), (int)(120*fade)));
  }
}

// A lumpy rock. Its shape and spin come from its objectID, so every client
// draws it the same; cracks spread across it as it takes damage.
static void gui_draw_asteroid(GuiArea *buffer, double rel_x, double rel_y, object_t objectID, int hp) {
  double scale = P_GUI_RADIUS_FUNC(buffer) / P_GUI_SHIP_RADIUS_INTERNAL;
  double cx = buffer->w/2 + rel_x*scale, cy = buffer->h/2 + rel_y*scale;
  double spin = ((objectID * 37) % 11 - 5) * 0.12; // Radians per second, either way
  double rotation = SDL_GetTicks() / 1000.0 * spin + objectID;
  double px[10], py[10], r;
  int rock = gui_makecol(165, 155, 140), crack = gui_makecol(95, 85, 75);
  int k, cracks;

  for (k=0; k < 10; k++) {
    r = ASTEROID_RADIUS * (0.78 + 0.32 * ((objectID * 7919 + k * 104729) % 97) / 96.0);
    px[k] = cx + cos(rotation + k * M_PI / 5) * r * scale;
    py[k] = cy + sin(rotation + k * M_PI / 5) * r * scale;
  }
  for (k=0; k < 10; k++) gui_line(buffer, px[k], py[k], px[(k + 1) % 10], py[(k + 1) % 10], rock);

  // One crack per quarter of its health lost, from near the middle to the edge
  cracks = (ASTEROID_HP - hp) * 4 / ASTEROID_HP;
  for (k=0; k < cracks && k < 4; k++) {
    int v = (objectID + k * 3) % 10;
    gui_line(buffer, cx + (px[v] - cx) * 0.15, cy + (py[v] - cy) * 0.15, px[v], py[v], crack);
  }
}

static void gui_octagon(GuiArea *buffer, double cx, double cy, double radius, double rotation, int color) {
  int k;
  for (k=0; k < 8; k++) {
    double a1 = rotation + k * M_PI / 4, a2 = a1 + M_PI / 4;
    gui_line(buffer, cx + cos(a1)*radius, cy + sin(a1)*radius, cx + cos(a2)*radius, cy + sin(a2)*radius, color);
  }
}

// A recharge orb: two octagons turning opposite ways, slowly strobing
// between the energy (yellow) and shield (cyan) colors. It blinks when it
// is about to fade, faster at the very end.
static void gui_draw_orb(GuiArea *buffer, double rel_x, double rel_y, int life) {
  double scale = P_GUI_RADIUS_FUNC(buffer) / P_GUI_SHIP_RADIUS_INTERNAL;
  double cx = buffer->w/2 + rel_x*scale, cy = buffer->h/2 + rel_y*scale;
  double t = SDL_GetTicks() / 1000.0;
  double mix = 0.5 + 0.5 * sin(t * 2 * M_PI / 1.6);
  int color = gui_makecol((int)(255*(1 - mix)), 255, (int)(255*mix));

  if (life < ORB_TICKS(10) && (int)(t * (life < ORB_TICKS(3) ? 8 : 4)) % 2) return;
  gui_octagon(buffer, cx, cy, ORB_RADIUS*scale, t * 0.6, color);
  gui_octagon(buffer, cx, cy, ORB_RADIUS*0.55*scale, -t * 0.9, color);
  gui_putpixel(buffer, cx, cy, gui_makecol(255, 255, 255));
}




void gui_draw_missile(GuiArea *buffer, double rel_x, double rel_y, double angle)
/*
	Draws a missile at a location relative to the player (center of screen) by rel_x, and rel_y.
	The ship is inscribed in a circle of radius 1 ship radius. There are P_GUI_SHIP_RADIUS_PER_SCRH
	ship radii from the top of the screen to the bottom of the screen. The units of rel_x and rel_y
	are what I call "internal units" There are P_GUI_SHOT_RADIUS_INTERNAL internal units per missile radius.
*/
{
	double xx,xy,yx,yy;
	int cw=gui_makecol(128,128,128);
	xx=cos(angle);
	xy=-sin(angle);
	yx=-xy;
	yy=xx;

	//	fprintf(stdout,"angle: %f\n", angle);

		gui_line(buffer,
			buffer->w/2+((xx*(-1)+xy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-1)+yy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->w/2+((xx*(1)+xy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(1)+yy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			cw);

		gui_line(buffer,
			buffer->w/2+((xx*(-.75)+xy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.75)+yy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->w/2+((xx*(-.866)+xy*(.5))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.866)+yy*(.5))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			cw);

		gui_line(buffer,
			buffer->w/2+((xx*(-.75)+xy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.75)+yy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->w/2+((xx*(-.866)+xy*(-.5))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.866)+yy*(-.5))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			cw);


/*
		gui_line(buffer,
			buffer->w/2+((xx*(-.75)+xy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.75)+yy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->w/2+((xx*(-.9848)+xy*(.1736))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.9848)+yy*(.1736))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			cw);

		gui_line(buffer,
			buffer->w/2+((xx*(-.75)+xy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.75)+yy*(0))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->w/2+((xx*(-.9848)+xy*(-.1736))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			buffer->h/2+((yx*(-.9848)+yy*(-.1736))*P_GUI_SHOT_RADIUS_INTERNAL/P_GUI_SHIP_RADIUS_INTERNAL
			 + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
			cw);
*/

}




/*
 * Draws a ship at a location relative to the player (center of screen) by rel_x, and rel_y.
 * The ship is inscribed in a circle of radius 1 ship radius. There are P_GUI_SHIP_RADIUS_PER_SCRH
 * ship radii from the top of the screen to the bottom of the screen. The units of rel_x and rel_y
 * are what I call "internal units" There are P_GUI_SHIP_RADIUS_INTERNAL internal units per ship radius.
 * The ship is drawn in color, color, and the engine glow is drawn in color eglow, unless eglow is 0 in which
 * case the engine glow is not drawn.
 */
void gui_draw_ship(GuiArea *buffer, double rel_x, double rel_y, double angle, int eglow, int color) {
  double xx,xy,yx,yy;
  xx=cos(angle);
  xy=-sin(angle);
  yx=sin(angle);
  yy=cos(angle);

  // Draw the engine glow
  if (eglow != 0) {
    gui_line(buffer,
	 buffer->w/2+(xx*(-.5)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.5)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.25)+xy*(.433) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.25)+yy*(.433) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 eglow);
    gui_line(buffer,
	 buffer->w/2+(xx*(-.5)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.5)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.25)+xy*(-.433) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.25)+yy*(-.433) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 eglow);
    gui_line(buffer,
	 buffer->w/2+(xx*(0)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(0)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-1)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-1)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 eglow);
    gui_line(buffer,
	 buffer->w/2+(xx*(0)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(0)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.9848)+xy*(.1736) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.9848)+yy*(.1736) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 eglow);
    gui_line(buffer,
	 buffer->w/2+(xx*(0)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(0)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.9848)+xy*(-.1736) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.9848)+yy*(-.1736) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 eglow);
  }

  {
    gui_line(buffer,
	 buffer->w/2+(xx*(0)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(0)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(1)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(1)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 color);
    gui_line(buffer,
	 buffer->w/2+(xx*(1)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(1)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.5)+xy*(.866) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.5)+yy*(.866) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 color);
    gui_line(buffer,
	 buffer->w/2+(xx*(1)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(1)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.5)+xy*(-.866) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.5)+yy*(-.866) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 color);
    gui_line(buffer,
	 buffer->w/2+(xx*(0)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(0)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.5)+xy*(-.866) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.5)+yy*(-.866) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 color);
    gui_line(buffer,
	 buffer->w/2+(xx*(0)+xy*(0) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(0)+yy*(0) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->w/2+(xx*(-.5)+xy*(.866) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 buffer->h/2+(yx*(-.5)+yy*(.866) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	 color);
    gui_putpixel(buffer,
	     buffer->w/2+(xx*(.3)+xy*(.15) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	     buffer->h/2+(yx*(.3)+yy*(.15) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	     color);
    gui_putpixel(buffer,
	     buffer->w/2+(xx*(.3)+xy*(-.15) + rel_x/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	     buffer->h/2+(yx*(.3)+yy*(-.15) + rel_y/P_GUI_SHIP_RADIUS_INTERNAL)*P_GUI_RADIUS_FUNC(buffer),
	     color);
  }
}

// This function draws the HUD into buffer. shield and energy can both go up to P_GUI_MAX_ENERGY.
void gui_draw_hud(GuiArea *buffer, int shield, int energy) {
  int startx,ey,sy;
  int wcol=gui_makecol(255,255,255);

  startx=.9*buffer->w;
  ey=buffer->h*.9;
  sy=buffer->h*.95;

  // Energy line
  if (energy >=0)
  {
    gui_hline(buffer, startx, ey, startx-(.25*buffer->w*energy)/P_GUI_MAX_ENERGY, gui_makecol(255,255,0));
    gui_vline(buffer, startx-.25*buffer->w, ey-2, ey+2, wcol);
  }

  // Energy symbol
  gui_line(buffer, startx+.01*buffer->w, ey, startx+.02*buffer->w, ey-.005*buffer->h, gui_makecol(255,255,0));
  gui_line(buffer, startx+.04*buffer->w, ey+.005*buffer->h, startx+.02*buffer->w, ey-.005*buffer->h, gui_makecol(255,255,0));
  gui_line(buffer, startx+.05*buffer->w, ey, startx+.04*buffer->w, ey+.005*buffer->h, gui_makecol(255,255,0));

  // Shield line
  if (shield >=0 ) 
  {
    gui_circle(buffer, buffer->w/2, buffer->h/2,2+P_GUI_RADIUS_FUNC(buffer),gui_makecol(0,(255*shield)/P_GUI_MAX_ENERGY,(255*shield)/P_GUI_MAX_ENERGY));
    gui_hline(buffer, startx, sy,startx-(.25*buffer->w*shield)/P_GUI_MAX_ENERGY, gui_makecol(0,255,255));
    gui_vline(buffer, startx-.25*buffer->w, sy-2, sy+2, wcol);
  }

  // Shield symbol
  gui_line(buffer, startx+.01*buffer->w, sy-.02*buffer->h, startx+.04*buffer->w, sy-.02*buffer->h, gui_makecol(0,255,255));
  gui_line(buffer, startx+.01*buffer->w, sy-.02*buffer->h, startx+.01*buffer->w, sy+.01*buffer->h, gui_makecol(0,255,255));
  gui_line(buffer, startx+.04*buffer->w, sy-.02*buffer->h, startx+.04*buffer->w, sy+.01*buffer->h, gui_makecol(0,255,255));
  gui_line(buffer, startx+.025*buffer->w, sy+.02*buffer->h, startx+.01*buffer->w, sy+.01*buffer->h, gui_makecol(0,255,255));
  gui_line(buffer, startx+.025*buffer->w, sy+.02*buffer->h, startx+.04*buffer->w, sy+.01*buffer->h, gui_makecol(0,255,255));


  gui_hline(buffer,0,0,buffer->w-1,wcol);
  gui_hline(buffer,0,buffer->h-1,buffer->w-1,wcol);
  gui_vline(buffer,0,0,buffer->h-1,wcol);
  gui_vline(buffer,buffer->w-1,0,buffer->h-1,wcol);

  gui_vline(buffer,buffer->w-radar->w-2,0,radar->h+1,wcol);
  gui_hline(buffer,buffer->w-radar->w-2,radar->h+1,buffer->w,wcol);


  gui_draw_radar(radar);
}

/*
  This function draws the radar map of the whole universe.
 */
void gui_draw_radar(GuiArea *buffer)
{
  double xcoord = 0.0;
  double ycoord = 0.0;
  object_t objectID = 0;
  struct dataNodeShips *clientShip = NULL;
  int player_col  =   gui_makecol(0,255,0);
  int other_col   =   gui_makecol(255,60,60);
  int missile_col =   gui_makecol(255,255,0);
  int planet_col  =   gui_makecol(128,90,0);
  int volcano_col =   gui_makecol(255,120,0);
  int orb_col     =   gui_makecol(0,255,255);
  int asteroid_col =  gui_makecol(200,200,200);
  SDL_Rect clip;
  int wrapX, wrapY;

  int i;

  if (DEBUG) printf("gui_draw_radar()\n");

  // make sure SDL has been initialized.
  if (initalized_sdl==0) {
    // Since the client is single threaded this stuff is atomic.
    gui_init_sdl();
    initalized_sdl=1;
  }
  
  gui_clear(buffer);


  // Nothing to show until we have a ship
  clientShip = cdataLookupShips(clientObjectID);
  if (clientShip == NULL) {
    if (DEBUG) printf("clientShip NULL!\n");
    return;
  }

  // Keep everything inside the radar box
  clip.x = buffer->x;
  clip.y = buffer->y;
  clip.w = buffer->w;
  clip.h = buffer->h;
  SDL_SetRenderClipRect(gui_renderer, &clip);

  // Draw Planets. Space wraps around, so a planet on an edge also shows on
  // the opposite edge, as it does in the main view.
  for (i=0; i < cdataCountPlanets; i++) {
    if (cdataArrayPlanets[i] == NULL) continue;
    xcoord = cdataArrayPlanets[i]->xcoord;
    ycoord = cdataArrayPlanets[i]->ycoord;
    objectID = cdataArrayPlanets[i]->objectID;
    for (wrapX = -1; wrapX <= 1; wrapX++) {
      for (wrapY = -1; wrapY <= 1; wrapY++) {
	gui_circlefill(buffer,
		       (xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w + wrapX*buffer->w ,
		       (ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h + wrapY*buffer->h ,
		       (P_GUI_DEFAULT_PLANET_SIZE)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w ,
		       planet_col);
      }
    }
    if (cdataArrayPlanets[i]->volcano) {
      gui_putpixel(buffer,
		   (xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w ,
		   (ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h ,
		   volcano_col);
    }
  }

  // Draw Ships
  for (i=0; i < cdataCountShips; i++) {
    if (cdataArrayShips[i] == NULL) continue;
    if (cdataArrayShips[i]->status==0) continue;
    xcoord = cdataArrayShips[i]->xcoord;
    ycoord = cdataArrayShips[i]->ycoord;
    objectID = cdataArrayShips[i]->objectID;

    if (objectID == clientObjectID) {

      // This is the client's ship, color appropriately
    gui_putpixel(buffer,
	       (xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w ,
	       (ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h ,
	       player_col);      
    } else {

    gui_putpixel(buffer,
	       (xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w ,
	       (ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h ,
	       other_col);
    }
  }

  
  // Draw Missiles
  for (i=0; i < cdataCountMissiles; i++) {
    if (cdataArrayMissiles[i] == NULL) continue;
    xcoord = cdataArrayMissiles[i]->xcoord;
    ycoord = cdataArrayMissiles[i]->ycoord;

    gui_putpixel(buffer,
	       (xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w ,
	       (ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h ,
	       missile_col);
  }

  // Asteroids are a small square, bigger than a ship's dot
  for (i=0; i < cdataCountAsteroids; i++) {
    xcoord = (cdataAsteroids[i].xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w;
    ycoord = (cdataAsteroids[i].ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h;
    gui_hline(buffer, xcoord, ycoord, xcoord + 1, asteroid_col);
    gui_hline(buffer, xcoord, ycoord + 1, xcoord + 1, asteroid_col);
  }

  // Recharge orbs are a small cross, so they are easy to find
  for (i=0; i < cdataCountOrbs; i++) {
    xcoord = (cdataOrbs[i].xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w;
    ycoord = (cdataOrbs[i].ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h;
    gui_hline(buffer, xcoord - 1, ycoord, xcoord + 1, orb_col);
    gui_vline(buffer, xcoord, ycoord - 1, ycoord + 1, orb_col);
  }

  SDL_SetRenderClipRect(gui_renderer, NULL);
}




/*
 * This Function draws stars in the background. These stars will not be consistent with location.
 * The stars will be moved as though the _player_ had moved by mv_x and mv_y.
 */
void gui_draw_stars(GuiArea *buffer, double mv_x, double mv_y) {
  static int initp=0;
  static float stars[P_GUI_STAR_COUNT][2];
  int i;

  if (DEBUG) {
    printf("gui_draw_stars(scr_buff, %.2f, %.2f)\ninitp=%i\n",mv_x, mv_y,initp);
  }

  // || mv_x > P_GUI_SHIP_RADIUS_PER_SCRH || mv_y > P_GUI_SHIP_RADIUS_PER_SCRH)
  if (initp == 0) {
    if (DEBUG) printf("Initializing stars\n");
    srand(time(0));
    initp=1;
    for (i=0;i<P_GUI_STAR_COUNT;++i) {
      stars[i][0]=(float)(rand()%buffer->w);
      stars[i][1]=(float)(rand()%buffer->h);
      if (DEBUG) printf("Star %f, %f\n",stars[i][0],stars[i][1]);
    }
    if (DEBUG) gui_line(buffer, 10, 0, 20, 10, gui_makecol(255,0,0));
  }

  for (i=0;i<P_GUI_STAR_COUNT;++i) {
    stars[i][0] -= ((mv_x/P_GUI_SHIP_RADIUS_INTERNAL) / P_GUI_SHIP_RADIUS_PER_SCRH)*buffer->h;
    stars[i][1] -= ((mv_y/P_GUI_SHIP_RADIUS_INTERNAL) / P_GUI_SHIP_RADIUS_PER_SCRH)*buffer->h;

    if (stars[i][0] < 0 || stars[i][0] > buffer->w || stars[i][1] < 0 || stars[i][1] > buffer->h) {
      int tx, ty;
      float fx, fy;
      tx = (int)stars[i][0];
      ty = (int)stars[i][1];
      fx = stars[i][0] - tx;
      fy = stars[i][1] - ty;
      tx = ((tx%buffer->w) + buffer->w)%buffer->w;
      ty = ((ty%buffer->h) + buffer->h)%buffer->h;
      stars[i][0] = tx + fx;
      stars[i][1] = ty + fy;
    }
	 
    gui_putpixel(buffer, stars[i][0], stars[i][1], gui_makecol(255,255,255));
    //gui_line(buffer,stars[i][0],stars[i][1],320,240,gui_makecol(255,255,255));
  }
}


SDL_Window *gui_get_window() {
  gui_init_sdl();
  return gui_window;
}

SDL_Renderer *gui_get_renderer() {
  gui_init_sdl();
  return gui_renderer;
}

void gui_draw_backdrop() {
  gui_init_sdl();
  SDL_SetRenderDrawColor(gui_renderer, 0, 0, 0, 255);
  SDL_RenderClear(gui_renderer);
  // Pretend the player is drifting so the stars move
  gui_draw_stars(scr_buff, 0.04, 0.015);
}

void gui_text(float x, float y, float scale, int color, const char *text) {
  gui_set_color(color);
  SDL_SetRenderScale(gui_renderer, scale, scale);
  SDL_RenderDebugText(gui_renderer, x / scale, y / scale, text);
  SDL_SetRenderScale(gui_renderer, 1.0f, 1.0f);
}

float gui_text_width(float scale, const char *text) {
  return (float)strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

void gui_text_centered(float y, float scale, int color, const char *text) {
  gui_text((GUI_SCREEN_W - gui_text_width(scale, text)) / 2, y, scale, color, text);
}

void gui_fill_rect(float x, float y, float w, float h, int color) {
  SDL_FRect rect = {x, y, w, h};
  gui_set_color(color);
  SDL_RenderFillRect(gui_renderer, &rect);
}

void gui_outline_rect(float x, float y, float w, float h, int color) {
  SDL_FRect rect = {x, y, w, h};
  gui_set_color(color);
  SDL_RenderRect(gui_renderer, &rect);
}

void gui_draw_status(const char *title, const char *detail) {
  gui_draw_backdrop();
  gui_text_centered(GUI_SCREEN_H / 2 - 24, 2.0f, 0xffffff, title);
  if (detail != NULL && detail[0] != 0) {
    gui_text_centered(GUI_SCREEN_H / 2 + 8, 1.0f, 0xa0a0a0, detail);
  }
  SDL_RenderPresent(gui_renderer);
}

// The scoreboard entry for our own ship, or NULL before the first scoreboard
static struct wireScore *gui_own_score() {
  int i;
  for (i=0; i < cdataCountScores; i++) {
    if (cdataScores[i].objectID == clientObjectID) return &cdataScores[i];
  }
  return NULL;
}

// Our score in the top left corner, clear of the radar
static void gui_draw_score() {
  char text[LEN];
  struct wireScore *own = gui_own_score();
  int target = gui_match_scores[CLIENT_OPTION(MATCH)];
  // In a match, show the score that wins too
  if (target > 0) snprintf(text, LEN, "Score %d / %d", own ? own->score : 0, target);
  else snprintf(text, LEN, "Score %d", own ? own->score : 0);
  gui_text(8, 8, 2.0f, 0xffffff, text);
  gui_text(8, 30, 1.0f, 0x808080, "Hold S for scores and rules");
  if (audioMuted()) gui_text(8, 44, 1.0f, 0x808080, "Sound off (M)");
}

#define GUI_EVENT_SHOW 6.0 // Seconds an event line stays on screen...
#define GUI_EVENT_FADE 1.5 // ...fading out over the last of them

// Recent events (who destroyed whom, who joined or left) at the bottom of
// the screen, newest lowest, fading away. Lines naming us are yellow.
static void gui_draw_events() {
  double now = platformNow(), age, fade;
  float y = 410;
  int i, color;

  for (i = cdataCountEvents - 1; i >= 0; i--) {
    age = now - cdataEvents[i].time;
    if (age >= GUI_EVENT_SHOW) continue;
    fade = (GUI_EVENT_SHOW - age) / GUI_EVENT_FADE;
    if (fade > 1.0) fade = 1.0;
    if (clientUsername[0] && strstr(cdataEvents[i].text, clientUsername)) {
      color = gui_makecol((int)(255*fade), (int)(255*fade), (int)(64*fade));
    } else {
      color = gui_makecol((int)(220*fade), (int)(220*fade), (int)(220*fade));
    }
    gui_text_centered(y, 1.0f, color, cdataEvents[i].text);
    y -= 14;
  }
}

static int gui_compare_scores(const void *a, const void *b) {
  const struct wireScore *sa = a, *sb = b;
  if (sa->score != sb->score) return sb->score - sa->score;
  if (sa->kills != sb->kills) return sb->kills - sa->kills;
  return sa->deaths - sb->deaths;
}

// Everyone's scores, best first, over the game
static void gui_draw_scoreboard() {
  struct wireScore sorted[MAX_COUNT_CLIENTS];
  char text[LEN];
  int i, color, count = cdataCountScores;
  int ruleRows = (OPTION_COUNT + 1) / 2;
  float w = 420, rowH = 22, ruleH = 14;
  float rulesY = 70 + count * rowH; // Game rules go under the scores
  float h = rulesY + 26 + ruleRows * ruleH + 8;
  float x = (GUI_SCREEN_W - w) / 2, y = 140;
  float colName = x + 20, colScore = x + 230, colKills = x + 300, colDeaths = x + 360;

  memcpy(sorted, cdataScores, sizeof(struct wireScore) * count);
  qsort(sorted, count, sizeof(struct wireScore), gui_compare_scores);

  // See-through panel so the game stays visible behind it
  SDL_SetRenderDrawBlendMode(gui_renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(gui_renderer, 0, 0, 20, 200);
  {
    SDL_FRect panel = {x, y, w, h};
    SDL_RenderFillRect(gui_renderer, &panel);
  }
  SDL_SetRenderDrawBlendMode(gui_renderer, SDL_BLENDMODE_NONE);
  gui_outline_rect(x, y, w, h, 0x60c0ff);

  gui_text_centered(y + 12, 2.0f, 0x60c0ff, "SCORES");
  gui_text(colName, y + 40, 1.0f, 0x808080, "Player");
  gui_text(colScore, y + 40, 1.0f, 0x808080, "Score");
  gui_text(colKills, y + 40, 1.0f, 0x808080, "Kills");
  gui_text(colDeaths, y + 40, 1.0f, 0x808080, "Deaths");

  for (i=0; i < count; i++) {
    float rowY = y + 58 + i * rowH;
    // Our own line stands out
    color = (sorted[i].objectID == clientObjectID) ? 0xffff40 : 0xffffff;
    snprintf(text, LEN, "%.12s", sorted[i].username); // Long names would run into the numbers
    gui_text(colName, rowY, 2.0f, color, text);
    snprintf(text, LEN, "%d", sorted[i].score);
    gui_text(colScore, rowY, 2.0f, color, text);
    snprintf(text, LEN, "%d", sorted[i].kills);
    gui_text(colKills, rowY, 2.0f, color, text);
    snprintf(text, LEN, "%d", sorted[i].deaths);
    gui_text(colDeaths, rowY, 2.0f, color, text);
  }

  // The server's rules in two columns; ones changed from the default stand out
  gui_text_centered(y + rulesY + 6, 1.0f, 0x60c0ff, cdataOptionsReceived ? "GAME RULES" : "GAME RULES (waiting for server)");
  for (i=0; i < OPTION_COUNT; i++) {
    float colX = x + 20 + (i / ruleRows) * (w / 2);
    float rowY = y + rulesY + 26 + (i % ruleRows) * ruleH;
    color = (cdataOptions[i] != gameOptions[i].defaultValue) ? 0xffff40 : 0xc0c0c0;
    gui_text(colX, rowY, 1.0f, 0x808080, gameOptions[i].label);
    snprintf(text, LEN, "%s", gameOptionText(i, cdataOptions[i]));
    gui_text(colX + w / 2 - 40 - gui_text_width(1.0f, text), rowY, 1.0f, color, text);
  }
}

// End of a match: everyone ranked by score, the winner highlighted in
// gold, and a countdown to the next match
static void gui_draw_rankings() {
  struct wireScore sorted[MAX_COUNT_CLIENTS];
  char text[LEN];
  int i, color, count = cdataCountScores;
  float x = 90, w = GUI_SCREEN_W - 180, rowH = 30, top = 190;
  float colRank = x + 16, colName = x + 60, colScore = x + 290, colKills = x + 370, colDeaths = x + 420;
  const char *winner = NULL;

  memcpy(sorted, cdataScores, sizeof(struct wireScore) * count);
  qsort(sorted, count, sizeof(struct wireScore), gui_compare_scores);
  for (i=0; i < count; i++) {
    if (sorted[i].objectID == cdataMatchWinner) winner = sorted[i].username;
  }

  // Dark panel over the frozen game
  SDL_SetRenderDrawBlendMode(gui_renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(gui_renderer, 0, 0, 20, 220);
  {
    SDL_FRect panel = {x - 20, 50, w + 40, 380};
    SDL_RenderFillRect(gui_renderer, &panel);
  }
  SDL_SetRenderDrawBlendMode(gui_renderer, SDL_BLENDMODE_NONE);
  gui_outline_rect(x - 20, 50, w + 40, 380, 0xffd700);

  gui_text_centered(72, 3.0f, 0x60c0ff, "MATCH OVER");
  if (winner != NULL) {
    snprintf(text, LEN, "%.19s wins!", winner);
    gui_text_centered(118, 2.0f, 0xffd700, text);
  }

  gui_text(colRank, top - 22, 1.0f, 0x808080, "Rank");
  gui_text(colName, top - 22, 1.0f, 0x808080, "Player");
  gui_text(colScore, top - 22, 1.0f, 0x808080, "Score");
  gui_text(colKills, top - 22, 1.0f, 0x808080, "Kills");
  gui_text(colDeaths, top - 22, 1.0f, 0x808080, "Deaths");
  for (i=0; i < count; i++) {
    float rowY = top + i * rowH;
    if (sorted[i].objectID == cdataMatchWinner) {
      color = 0xffd700;
      gui_outline_rect(x, rowY - 6, w, rowH - 2, 0xffd700);
    } else {
      color = (sorted[i].objectID == clientObjectID) ? 0xffff40 : 0xffffff;
    }
    snprintf(text, LEN, "%d", i + 1);
    gui_text(colRank, rowY, 2.0f, color, text);
    snprintf(text, LEN, "%.12s", sorted[i].username);
    gui_text(colName, rowY, 2.0f, color, text);
    snprintf(text, LEN, "%d", sorted[i].score);
    gui_text(colScore, rowY, 2.0f, color, text);
    snprintf(text, LEN, "%d", sorted[i].kills);
    gui_text(colKills, rowY, 1.0f, color, text);
    snprintf(text, LEN, "%d", sorted[i].deaths);
    gui_text(colDeaths, rowY, 1.0f, color, text);
  }

  snprintf(text, LEN, "Next match in %d", cdataNextMatch);
  gui_text_centered(400, 1.0f, 0xc0c0c0, text);
}
