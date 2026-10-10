#include <SDL3/SDL.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#include "global.h"
#include "gui.h"
#include "cdata.h"

/*
 * A rectangular region of the window that drawing functions work in, so
 * the radar can be drawn with the same code as the main view. Coordinates
 * passed to the gui_* primitives are relative to the area's top left corner.
 */
typedef struct {
  int x, y, w, h;
} GuiArea;

#define GUI_SCREEN_W 640
#define GUI_SCREEN_H 480

void gui_init_sdl();
void gui_draw_ship(GuiArea *buffer, double rel_x, double rel_y, double angle, int eglow, int color);
void gui_draw_missile(GuiArea *buffer, double rel_x, double rel_y, double angle);
void gui_draw_hud(GuiArea *buffer, int shield, int energy);
void gui_draw_radar(GuiArea *buffer);
void gui_draw_stars(GuiArea *buffer, double mv_x, double mv_y);
void gui_draw_planet(GuiArea *buffer, double rel_x, double rel_y, double radius);

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
    printf("clientShip NULL!\n");
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
	if (quit_requested) *flags|=P_GUI_FLAG_QUIT;
	if (time_diff<=0) // No time has passed, so no movement to report.
	{
		return quit_requested ? 0 : -1;
	}

	// Now read the keys and do something based of what they say.
	key=SDL_GetKeyboardState(NULL);

	if (key[SDL_SCANCODE_LEFT]) // turn
	{
		*angle_change=-( seconds_diff * P_GUI_ANGLE_PER_SECOND );
	}

	if (key[SDL_SCANCODE_RIGHT])
	{
		*angle_change=( seconds_diff * P_GUI_ANGLE_PER_SECOND );
	}

	if (key[SDL_SCANCODE_UP]) // accel
	{
		*accel_amount=( seconds_diff * P_GUI_ACCEL_PER_SECOND );
	}

	if (key[SDL_SCANCODE_SPACE]||key[SDL_SCANCODE_LCTRL]) // fire
	{
		*flags|=P_GUI_FLAG_FIRE;
	}
	if (key[SDL_SCANCODE_TAB]) // hyper
	{
		*flags|=P_GUI_FLAG_HYPER;
	}
	if (key[SDL_SCANCODE_ESCAPE]) // quit
	{
		*flags|=P_GUI_FLAG_QUIT;
	}

	if (key[SDL_SCANCODE_Z]) // energy to shield
	{
		*energy_to_shields=( seconds_diff * P_GUI_ENERGY_MOVE_PER_SECOND );
		energy_smoothing_buffer+=( seconds_diff * P_GUI_ENERGY_MOVE_PER_SECOND )-(double)(*energy_to_shields);
		if (energy_smoothing_buffer>1)
		{
			++*energy_to_shields;
			--energy_smoothing_buffer;
		}
	}

	if (key[SDL_SCANCODE_X]) // energy from shield
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

	if (FULLSCREEN) flags |= SDL_WINDOW_FULLSCREEN;

	if (!SDL_Init(SDL_INIT_VIDEO) ||
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
    printf("clientShip NULL!\n");
    return;
  }

  // Draw Planets
  for (i=0; i < cdataCountPlanets; i++) {
    if (cdataArrayPlanets[i] == NULL) continue;
    xcoord = cdataArrayPlanets[i]->xcoord;
    ycoord = cdataArrayPlanets[i]->ycoord;
    objectID = cdataArrayPlanets[i]->objectID;
    gui_circlefill(buffer,
	       (xcoord-SPACE_MIN_XCOORD)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w ,
	       (ycoord-SPACE_MIN_YCOORD)/(SPACE_MAX_YCOORD-SPACE_MIN_YCOORD)*buffer->h ,
	       (P_GUI_DEFAULT_PLANET_SIZE)/(SPACE_MAX_XCOORD-SPACE_MIN_XCOORD)*buffer->w ,
	       planet_col);

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
    printf("Initializing stars\n");
    srand(time(0));
    initp=1;
    for (i=0;i<P_GUI_STAR_COUNT;++i) {
      stars[i][0]=(float)(rand()%buffer->w);
      stars[i][1]=(float)(rand()%buffer->h);
      printf("Star %f, %f\n",stars[i][0],stars[i][1]);
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
