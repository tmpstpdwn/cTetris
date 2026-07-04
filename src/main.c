#include "renderer.h"
#include "score.h"

int main(void) {
    renderer_init();

    uint32_t high_score = hs_load();
    renderer_set_high_score(high_score);

    while (renderer_input()) {
        renderer_update();
        renderer_render();
    }

    uint32_t final_score = renderer_get_high_score();
    if (final_score > high_score)
        hs_save(final_score);

    renderer_shutdown();
    return 0;
}
