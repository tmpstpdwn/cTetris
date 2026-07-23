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

// If the high score has changed, save it.
void high_score_save(uint32_t *old_high_score) {
    uint32_t new_high_score = renderer_high_score_get();
    if (new_high_score > *old_high_score) {
        score_save(new_high_score);
        *old_high_score = new_high_score;
    }
}

int main(void) {
    renderer_init();

    // Load high score and give it to the renderer.
    uint32_t high_score = score_load();
    renderer_high_score_set(high_score);
    bool already_saved = false;

    while (renderer_input()) {
        bool running = renderer_update();
        if (!running && !already_saved) {
            high_score_save(&high_score);
            already_saved = true;
        } else if (running) {
            already_saved = false;
        }
        renderer_render();
    }

    high_score_save(&high_score);

    renderer_shutdown();
    return 0;
}

/* [ END ] */
