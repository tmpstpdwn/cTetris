#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stdint.h>

// Init and shutdown.
void renderer_init(void);
void renderer_shutdown(void);

// Core.
bool renderer_input(void); // Returns false signaling game exit.
void renderer_update(void);
void renderer_render(void);

// Pass high score value on to the renderer.
void renderer_set_high_score(uint32_t hs);
// Retrieve high score value from the renderer.
// Returned value will be greater than or equal to the value passed on to the
// renderer.
uint32_t renderer_get_high_score(void);

#endif
