#ifndef GUI_H
#define GUI_H

#include <math.h>
#include <SDL3/SDL.h>
#include "global.h"
#include "cdata.h"

// Everything is drawn at this size and scaled to fit the window
#define GUI_SCREEN_W 640
#define GUI_SCREEN_H 480

void gui_update_display();

int gui_get_input(double *angle_change, double *accel_amount, int *energy_to_shields, int *flags); // This will have different arguments in the future
/*
	Flags are:
bit 0:
	Player tried to fire.
bit 1:
	Player tried to hyperspace. see above for questions.
bit 2:
	Player pressed Escape to leave the game.
bit 3:
	Player closed the window (P_GUI_FLAG_CLOSE).


This function will return 0 in most cases.

This function will return -1 if there was an non-fatal error (such as clock wraparound). This return value means that the
information in angle_change, accel_amount, energy_to_shields, and flags should be discarded.

If there is a failure initializing SDL then this function will not return, since it calls gui_init_sdl() which calls exit(1) on an error.

*/

// Pass every SDL event here so gamepads plugged in or out are picked up
void gui_gamepad_event(SDL_Event *event);
// The connected gamepad's name, or NULL
const char *gui_gamepad_name();
// Gamepad face buttons by job, matched to the labels printed on them
#define GUI_PAD_FIRE 0 // A (Cross): fire, and choose in menus
#define GUI_PAD_BACK 1 // B (Circle): back in menus
#define GUI_PAD_JUMP 2 // Y (Triangle): hyperjump
SDL_GamepadButton gui_pad_role(int role);
const char *gui_pad_role_name(int role);
// Open the window. Safe to call more than once.
void gui_init_sdl();
SDL_Window *gui_get_window();
SDL_Renderer *gui_get_renderer();

/* Screens and text for menus. Coordinates are in GUI_SCREEN_W x GUI_SCREEN_H
   units; scale multiplies the 8x8 pixel font. Colors are 0xRRGGBB. */
void gui_draw_backdrop();  // Clear to space with slowly drifting stars
void gui_text(float x, float y, float scale, int color, const char *text);
void gui_text_centered(float y, float scale, int color, const char *text);
float gui_text_width(float scale, const char *text);
void gui_fill_rect(float x, float y, float w, float h, int color);
void gui_outline_rect(float x, float y, float w, float h, int color);
// Draw a full-screen message (title and an optional detail line) and show it
void gui_draw_status(const char *title, const char *detail);

#endif
