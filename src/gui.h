#ifndef GUI_H
#define GUI_H

#include <math.h>
#include "global.h"
#include "cdata.h"



void gui_update_display();

int gui_get_input(double *angle_change, double *accel_amount, int *energy_to_shields, int *flags); // This will have different arguments in the future
/*
	Flags are:
bit 0:
	Player tried to fire.
bit 1:
	Player tried to hyperspace. see above for questions.
bit 2:
	Player tried to quit.
bit 3:


This function will return 0 in most cases.

This function will return -1 if there was an non-fatal error (such as clock wraparound). This return value means that the 
information in angle_change, accel_amount, energy_to_shields, and flags should be discarded.

If there is a failure initializing SDL then this function will not return, since it calls gui_init_sdl() which calls exit(1) on an error.

*/



#endif
