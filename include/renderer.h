#ifndef RENDERER_H
#define RENDERER_H

#include <stdbool.h>
#include <stdint.h>

void renderer_init(void);
void renderer_shutdown(void);

bool renderer_input(void);
void renderer_update(void);
void renderer_render(void);

void renderer_set_high_score(uint32_t hs);
uint32_t renderer_get_high_score(void);

#endif
