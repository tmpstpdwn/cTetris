#include "ctetris.h"
#include "raylib.h"
#include <stdbool.h>
#include <time.h>

#include <stdio.h>

#define CELL_SIZE 30
#define RENDER_W (CELL_SIZE * COLS)
#define RENDER_H (CELL_SIZE * ROWS)

// Colors
#define BG (Color){17, 17, 17, 255}
#define EMPTY_GRID_CELL_COLOR (Color){30, 30, 30, 255}
#define SHADOW_SHAPE_COLOR (Color){42, 42, 42, 255}

#define COLOR_O (Color){93, 202, 165, 255}
#define COLOR_L (Color){127, 119, 221, 255}
#define COLOR_I (Color){55, 138, 221, 255}
#define COLOR_T (Color){216, 90, 48, 255}
#define COLOR_S (Color){186, 117, 23, 255}
#define COLOR_J (Color){212, 83, 126, 255}
#define COLOR_Z (Color){99, 153, 34, 255}

static bool new_game = true;
static bool paused = true;

static Color piece_color(enum ShapeType t) {
    switch (t) {
    case O:
        return COLOR_O;
    case L:
        return COLOR_L;
    case I:
        return COLOR_I;
    case T:
        return COLOR_T;
    case S:
        return COLOR_S;
    case J:
        return COLOR_J;
    case Z:
        return COLOR_Z;
    default:
        return EMPTY_GRID_CELL_COLOR;
    }
}

static void draw_board(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            DrawRectangle(c * CELL_SIZE + 1, r * CELL_SIZE + 1, CELL_SIZE - 2,
                          CELL_SIZE - 2,
                          piece_color(ctetris_get_grid_cell(r, c)));
}

static void draw_shape(struct Shape shape, bool shadow) {
    struct Coord pos = shape.pos;
    Color col = (shadow) ? SHADOW_SHAPE_COLOR : piece_color(shape.type);
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        struct Coord off = shape.offsets[i];
        DrawRectangle((pos.x + off.x) * CELL_SIZE + 1,
                      (pos.y + off.y) * CELL_SIZE + 1, CELL_SIZE - 2,
                      CELL_SIZE - 2, col);
    }
}

static void ui_init(void) {
    SetRandomSeed(time(NULL));
    InitWindow(RENDER_W, RENDER_H, "cTetris");
    SetTargetFPS(60);
}

static void ui_shutdown(void) { CloseWindow(); }

void input(void) {
    if (IsKeyPressed(KEY_P))
        paused = !paused;
    if (paused)
        return;

    struct InputState input_state = {0};

    input_state.rotate_cw_pressed = IsKeyPressed(KEY_UP);
    input_state.rotate_acw_pressed = IsKeyPressed(KEY_Z);
    input_state.hard_drop_pressed = IsKeyPressed(KEY_SPACE);

    input_state.strafe_left_pressed = IsKeyPressed(KEY_LEFT);
    input_state.strafe_right_pressed = IsKeyPressed(KEY_RIGHT);

    input_state.strafe_left_held = IsKeyDown(KEY_LEFT);
    input_state.strafe_right_held = IsKeyDown(KEY_RIGHT);
    input_state.soft_drop_held = IsKeyDown(KEY_DOWN);

    ctetris_post_input(input_state);
}

const char *ctetris_event_type_str(enum CTetrisEventType type) {
    switch (type) {
    case CTETRIS_EVENT_NONE:
        return "NONE";
    case CTETRIS_EVENT_NEW_GAME:
        return "NEW_GAME";
    case CTETRIS_EVENT_NEW_SHAPE:
        return "NEW_SHAPE";
    case CTETRIS_EVENT_HARD_DROP:
        return "HARD_DROP";
    case CTETRIS_EVENT_SCORE_UPDATE:
        return "SCORE_UPDATE";
    case CTETRIS_EVENT_LINES_UPDATE:
        return "LINES_UPDATE";
    case CTETRIS_EVENT_LEVEL_UPDATE:
        return "LEVEL_UPDATE";
    case CTETRIS_EVENT_COMBO_UPDATE:
        return "COMBO_UPDATE";
    case CTETRIS_EVENT_LOCK_START:
        return "LOCK_START";
    case CTETRIS_EVENT_LOCK_CANCEL:
        return "LOCK_CANCEL";
    case CTETRIS_EVENT_LOCK_DONE:
        return "LOCK_DONE";
    case CTETRIS_EVENT_LINE_CLEAR:
        return "LINE_CLEAR";
    case CTETRIS_EVENT_GRID_COMPACT:
        return "GRID_COMPACT";
    case CTETRIS_EVENT_GAME_OVER:
        return "GAME_OVER";
    default:
        return "UNKNOWN";
    }
}

static void update(void) {
    static double last_time = -1;
    if (last_time < 0)
        last_time = GetTime();
    double now = GetTime();
    double delta = paused ? 0.0 : now - last_time;
    last_time = now;

    if (paused)
        return;

    ctetris_update(delta);

    enum CTetrisEventType ev_t;
    while ((ev_t = ctetris_event_get().type) != CTETRIS_EVENT_NONE) {
        printf("EV: %s\n", ctetris_event_type_str(ev_t));
        if (ev_t == CTETRIS_EVENT_GAME_OVER)
            new_game = true;
    }
}

static void render(void) {
    BeginDrawing();
    ClearBackground(BG);
    draw_board();
    draw_shape(ctetris_get_curr_shadow_shape(), true);
    draw_shape(ctetris_get_curr_shape(), false);
    EndDrawing();
}

int main(void) {
    ui_init();
    while (!WindowShouldClose()) {
        if (new_game) {
            ctetris_init();
            new_game = false;
        }
        input();
        update();
        render();
    }
    ui_shutdown();
    return 0;
}
