#ifndef MATCH_H
#define MATCH_H

#include "global.h"

/* Matches (server only): with a Match length other than Endless, the first
   player to reach its score wins. The game then pauses on the rankings for
   MATCH_RESULTS_TIME seconds before a fresh match starts. */
extern int matchOver;          // 1 while the rankings are up
extern object_t matchWinner;
int matchSecondsLeft();        // Until the next match, while matchOver
// Once per tick, after physics: look for a winner, or count down to the next match
void matchUpdate();

#endif
