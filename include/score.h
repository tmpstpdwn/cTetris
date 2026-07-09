/* [ SCORE_H ] */

#ifndef SCORE_H
#define SCORE_H

/* [ INCLUDES ] */

#include <stdbool.h>
#include <stdint.h>

/* [ FN DCL ] */

/*
 * following 2 fns are used to save / load score on / from the disk.
 * it does so by abstracting away the underlying os.
 * so technically this should work on windows and unix-like operating systems.
 */

// load saved score from the disk. if not previously saved, return 0.
uint32_t score_load(void);
// save score on to the disk.
void score_save(uint32_t score);

#endif

/* [ END ] */
