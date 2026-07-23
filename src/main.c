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

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

/* [ MAIN ] */

#if defined(__EMSCRIPTEN__)

// On the web the main loop is driven by the browser and never returns, so
// keep the running high score around and persist it whenever it improves
// (rather than once at exit, as the native build does).
static uint32_t web_high_score;

static void web_frame(void) {
    renderer_input();
    renderer_update();
    renderer_render();

    uint32_t hs = renderer_high_score_get();
    if (hs > web_high_score) {
        web_high_score = hs;
        score_save(hs);
    }
}

#endif

int main(void) {
    renderer_init();

    // Load high score and give it to the renderer.
    uint32_t high_score = score_load();
    renderer_high_score_set(high_score);

#if defined(__EMSCRIPTEN__)
    web_high_score = high_score;
    // 0 fps => use requestAnimationFrame; 1 => simulate an infinite loop
    // (this call does not return).
    emscripten_set_main_loop(web_frame, 0, 1);
#else
    while (renderer_input()) {
        renderer_update();
        renderer_render();
    }

    // If the high score has changed, save it.
    uint32_t final_score = renderer_high_score_get();
    if (final_score > high_score)
        score_save(final_score);

    renderer_shutdown();
#endif

    return 0;
}

/* [ END ] */
