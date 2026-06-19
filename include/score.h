#ifndef SCORE_H
#define SCORE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * following 2 fns are used to store / retrieve high score on / from the disk.
 * it does so by abstracting the underlying os.
 * so technically should work on windows and unix-like operating systems.
 */

// load stored high_score from the disk. if not found, return 0.
uint32_t hs_load(void);
// save score on to the disk.
bool hs_save(uint32_t score);

#endif
