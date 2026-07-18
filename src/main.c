/* [ MAIN_C ] */

/*
 * Things handled by main.c:
 * - Loading and saving high score.
 * - running the renderer.
 */

// Version: 1.0.1

/* [ INCLUDES ] */

#include "renderer.h"
#include "score.h"

/* [ MAIN ] */

int main(void) {
    renderer_init();

    // Load high score and give it to the renderer.
    uint32_t high_score = score_load();
    renderer_high_score_set(high_score);

    while (renderer_input()) {
        renderer_update();
        renderer_render();
    }

    // If the high score has changed, save it.
    uint32_t final_score = renderer_high_score_get();
    if (final_score > high_score)
        score_save(final_score);

    renderer_shutdown();
    return 0;
}

/* [ END ] */
