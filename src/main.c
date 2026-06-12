#include "ctetris.h"
#include "raylib.h"
#include <stdbool.h>
#include <time.h>

#define CELL_SIZE 30
#define RENDER_W (CELL_SIZE * COLS)
#define RENDER_H (CELL_SIZE * ROWS)

// Colors
#define BG (Color){17, 17, 17, 255}
#define COLOR_N (Color){30, 30, 30, 255}
#define COLOR_X (Color){42, 42, 42, 255}
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
    case X:
        return COLOR_X;
    default:
        return COLOR_N;
    }
}

static void draw_board(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            DrawRectangle(c * CELL_SIZE + 1, r * CELL_SIZE + 1, CELL_SIZE - 2,
                          CELL_SIZE - 2,
                          piece_color(ctetris_get_board_cell(r, c)));
}

static void draw_shape(const struct Shape *shape) {
    if (!shape)
        return;
    struct Coord pos = ctetris_get_pos(shape);
    Color col = piece_color(ctetris_get_type(shape));
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        struct Coord off = ctetris_get_offset(shape, i);
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

static void input(void) {
    if (IsKeyPressed(KEY_P))
        paused = !paused;
    if (paused)
        return;

    if (IsKeyPressed(KEY_UP))
        ctetris_rotate_cw();
    else if (IsKeyPressed(KEY_Z))
        ctetris_rotate_acw();

    if (IsKeyPressed(KEY_LEFT))
        ctetris_strafe_left(false);
    else if (IsKeyDown(KEY_LEFT))
        ctetris_strafe_left(true);
    else if (IsKeyPressed(KEY_RIGHT))
        ctetris_strafe_right(false);
    else if (IsKeyDown(KEY_RIGHT))
        ctetris_strafe_right(true);

    if (IsKeyDown(KEY_DOWN))
        ctetris_soft_drop();
    if (IsKeyPressed(KEY_SPACE))
        ctetris_hard_drop();
}

static bool update(void) {
    static double last_time = -1;
    if (last_time < 0)
        last_time = GetTime();
    double now = GetTime();
    double delta = paused ? 0.0 : now - last_time;
    last_time = now;
    if (paused)
        return false;
    return ctetris_update(delta);
}

static void render(void) {
    BeginDrawing();
    ClearBackground(BG);
    draw_board();
    draw_shape(ctetris_get_curr_shadow_shape());
    draw_shape(ctetris_get_curr_shape());
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
        new_game = update();
        render();
    }
    ui_shutdown();
    return 0;
}
