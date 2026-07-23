/* [ RENDERER_H ] */

#ifndef RENDERER_H
#define RENDERER_H

/* [ INCLUDES ] */

#include <stdbool.h>
#include <stdint.h>

/* [ FN DCL ] */

// Init and shutdown.
void renderer_init(void);
void renderer_shutdown(void);

// Core.

// The following 3 fns are to be called in the order they are listed.
bool renderer_input(void);  // Returns false signaling game exit.
bool renderer_update(void); // Returns false signaling game over.
void renderer_render(void);

// Pass high score value on to the renderer.
void renderer_high_score_set(uint32_t hs);
// Retrieve high score value from the renderer.
// Returned value will be greater than or equal to the value passed on to the
// renderer.
uint32_t renderer_high_score_get(void);

#endif

/* [ END ] */
