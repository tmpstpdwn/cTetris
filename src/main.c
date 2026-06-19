#include <time.h>

#include "ctetris.h"
#include "raylib.h"
#include "score.h"

#define FONT_SMALL_SIZE 15
#define FONT_MEDIUM_SIZE 20
#define FONT_BIG_SIZE 25

#define CELL_SIZE 30

#define LHS_PADDING 10
#define RHS_PADDING 20

#define GRID_W (CELL_SIZE * COLS + LHS_PADDING * 2)
#define GRID_H (CELL_SIZE * ROWS + LHS_PADDING * 2)

#define HEADER_HEIGHT (FONT_MEDIUM_SIZE + LHS_PADDING * 2)
#define HEADER_WIDTH GRID_W

#define LHS_WIDTH GRID_W
#define LHS_HEIGHT (HEADER_HEIGHT + GRID_H)

#define RHS_WIDTH 250
#define RHS_HEIGHT LHS_HEIGHT

#define RENDER_H RHS_HEIGHT
#define RENDER_W (LHS_WIDTH + RHS_WIDTH)

#define GRID_X LHS_PADDING
#define GRID_Y (HEADER_HEIGHT + LHS_PADDING)

#define LABEL_BOTTOM_MARGIN 5
#define NEXT_CELL_SIZE 20

#define CREDIT_TXT "By tmpstpdwn@github"

// Colors

#define BG (Color){17, 17, 17, 255}
#define EMPTY_GRID_CELL_COLOR (Color){28, 28, 28, 255}
#define DIVIDER_COLOR (Color){27, 27, 27, 255}
#define SHADOW_SHAPE_COLOR (Color){42, 42, 42, 255}

#define COLOR_O (Color){93, 202, 165, 255}
#define COLOR_L (Color){127, 119, 221, 255}
#define COLOR_I (Color){55, 138, 221, 255}
#define COLOR_T (Color){216, 90, 48, 255}
#define COLOR_S (Color){186, 117, 23, 255}
#define COLOR_J (Color){212, 83, 126, 255}
#define COLOR_Z (Color){99, 153, 34, 255}

// State Variables

static bool new_game = true;
static bool paused = false;

static uint32_t score, high_score, lines, level;

// Fonts

static Font font_small;
static Font font_medium;
static Font font_big;

// Colors

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

// Init / shutdown

static void ui_init(void) {
    SetRandomSeed(time(NULL));
    InitWindow(RENDER_W, RENDER_H, "cTetris");
    SetTargetFPS(60);

    font_small = LoadFontEx("./assets/SauceCodeProNerdFontPropo-Regular.ttf",
                            FONT_SMALL_SIZE, NULL, 0);
    font_medium = LoadFontEx("./assets/SauceCodeProNerdFontPropo-Regular.ttf",
                             FONT_MEDIUM_SIZE, NULL, 0);
    font_big = LoadFontEx("./assets/SauceCodeProNerdFontPropo-Regular.ttf",
                          FONT_BIG_SIZE, NULL, 0);

    SetTextureFilter(font_small.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(font_medium.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(font_big.texture, TEXTURE_FILTER_POINT);

    high_score = hs_load();
    score = level = lines = 0;
}

static void ui_shutdown(void) {
    UnloadFont(font_small);
    UnloadFont(font_medium);
    UnloadFont(font_big);
    CloseWindow();
}

// Input

void input(void) {
    if (IsKeyPressed(KEY_P))
        paused = !paused;
    if (paused)
        return;

    if (IsKeyPressed(KEY_R)) {
        new_game = true;
        return;
    }

    struct InputState s = {0};
    s.rotate_cw_pressed = IsKeyPressed(KEY_UP);
    s.rotate_acw_pressed = IsKeyPressed(KEY_Z);
    s.hard_drop_pressed = IsKeyPressed(KEY_SPACE);
    s.strafe_left_pressed = IsKeyPressed(KEY_LEFT);
    s.strafe_right_pressed = IsKeyPressed(KEY_RIGHT);
    s.strafe_left_held = IsKeyDown(KEY_LEFT);
    s.strafe_right_held = IsKeyDown(KEY_RIGHT);
    s.soft_drop_held = IsKeyDown(KEY_DOWN);
    ctetris_post_input(s);
}

// Update

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

    struct CTetrisEvent ev;
    while ((ev = ctetris_event_get()).type != CTETRIS_EVENT_NONE) {
        if (ev.type == CTETRIS_EVENT_GAME_OVER)
            new_game = true;
        else if (ev.type == CTETRIS_EVENT_SCORE_UPDATE)
            score = ev.val;
        else if (ev.type == CTETRIS_EVENT_LEVEL_UPDATE)
            level = ev.val;
        else if (ev.type == CTETRIS_EVENT_LINES_UPDATE)
            lines = ev.val;
    }
}

// Drawing

static void draw_header(void) {
    // title — left
    Vector2 pos = {LHS_PADDING, (int)((HEADER_HEIGHT - FONT_MEDIUM_SIZE) / 2)};
    DrawTextEx(font_medium, "ctetris", pos, FONT_MEDIUM_SIZE, 2, COLOR_O);

    if (!high_score)
        return;

    // best score — right, same baseline, dim
    const char *hs_text = TextFormat("BEST: %u", high_score);
    Vector2 hs_sz = MeasureTextEx(font_small, hs_text, FONT_SMALL_SIZE, 2);
    Vector2 hs_pos = {LHS_WIDTH - LHS_PADDING - hs_sz.x,
                      (int)((HEADER_HEIGHT - FONT_SMALL_SIZE) / 2)};
    DrawTextEx(font_small, hs_text, hs_pos, FONT_SMALL_SIZE, 2,
               (Color){42, 42, 42, 255});
}

static void draw_dividers(void) {
    DrawLine(0, HEADER_HEIGHT, LHS_WIDTH, HEADER_HEIGHT, DIVIDER_COLOR);
    DrawLine(LHS_WIDTH, 0, LHS_WIDTH, LHS_HEIGHT, DIVIDER_COLOR);
}

static void draw_shape(struct Shape shape, bool shadow, int origin_x,
                       int origin_y, int cell_sz) {
    Color col = shadow ? SHADOW_SHAPE_COLOR : piece_color(shape.type);
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        DrawRectangle(
            origin_x + (shape.pos.x + shape.offsets[i].x) * cell_sz + 1,
            origin_y + (shape.pos.y + shape.offsets[i].y) * cell_sz + 1,
            cell_sz - 2, cell_sz - 2, col);
    }
}

static void draw_grid(void) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            DrawRectangle(GRID_X + c * CELL_SIZE + 1,
                          GRID_Y + r * CELL_SIZE + 1, CELL_SIZE - 2,
                          CELL_SIZE - 2,
                          piece_color(ctetris_get_grid_cell(r, c)));
        }
    }
    draw_shape(ctetris_get_curr_shadow_shape(), true, GRID_X, GRID_Y,
               CELL_SIZE);
    draw_shape(ctetris_get_curr_shape(), false, GRID_X, GRID_Y, CELL_SIZE);
}

static int draw_next_shape(int x, int y, int cell_size) {
    struct Shape shape = ctetris_get_next_shape();
    int grid_px = 4 * cell_size;

    DrawRectangleLines(x, y, grid_px, grid_px, (Color){36, 36, 36, 255});
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            DrawRectangle(x + c * cell_size + 1, y + r * cell_size + 1,
                          cell_size - 2, cell_size - 2, EMPTY_GRID_CELL_COLOR);

    int min_x = shape.offsets[0].x, max_x = shape.offsets[0].x;
    int min_y = shape.offsets[0].y, max_y = shape.offsets[0].y;
    for (int i = 1; i < OFFSETS_COUNT; i++) {
        if (shape.offsets[i].x < min_x)
            min_x = shape.offsets[i].x;
        if (shape.offsets[i].x > max_x)
            max_x = shape.offsets[i].x;
        if (shape.offsets[i].y < min_y)
            min_y = shape.offsets[i].y;
        if (shape.offsets[i].y > max_y)
            max_y = shape.offsets[i].y;
    }

    shape.pos.x = (4 - (max_x - min_x + 1)) / 2 - min_x;
    shape.pos.y = (4 - (max_y - min_y + 1)) / 2 - min_y;
    draw_shape(shape, false, x, y, cell_size);

    return grid_px;
}

static void draw_sidebar(void) {
    int x = LHS_WIDTH + RHS_PADDING;

    // SCORE
    int y = RHS_PADDING;
    DrawTextEx(font_small, "SCORE", (Vector2){x, y}, FONT_SMALL_SIZE, 2,
               (Color){56, 56, 56, 255});
    y += FONT_SMALL_SIZE + LABEL_BOTTOM_MARGIN;
    DrawTextEx(font_big, TextFormat("%u", score), (Vector2){x, y},
               FONT_BIG_SIZE, 2, (Color){238, 238, 238, 255});
    y += FONT_BIG_SIZE + RHS_PADDING;
    DrawLine(x, y, RENDER_W - RHS_PADDING, y, DIVIDER_COLOR);

    // LEVEL / LINES
    y += RHS_PADDING;
    int split_x = x + RHS_WIDTH / 2;

    DrawTextEx(font_small, "LEVEL", (Vector2){x, y}, FONT_SMALL_SIZE, 2,
               (Color){56, 56, 56, 255});
    DrawTextEx(font_small, "LINES", (Vector2){split_x, y}, FONT_SMALL_SIZE, 2,
               (Color){56, 56, 56, 255});
    y += FONT_SMALL_SIZE + LABEL_BOTTOM_MARGIN;
    DrawTextEx(font_big, TextFormat("%u", level), (Vector2){x, y},
               FONT_BIG_SIZE, 2, (Color){238, 238, 238, 255});
    DrawTextEx(font_big, TextFormat("%u", lines), (Vector2){split_x, y},
               FONT_BIG_SIZE, 2, (Color){238, 238, 238, 255});
    DrawLine(split_x - RHS_PADDING, y - LABEL_BOTTOM_MARGIN,
             split_x - RHS_PADDING, y + FONT_BIG_SIZE, DIVIDER_COLOR);
    y += FONT_BIG_SIZE + RHS_PADDING;
    DrawLine(x, y, RENDER_W - RHS_PADDING, y, DIVIDER_COLOR);

    // NEXT SHAPE PREVIEW
    y += RHS_PADDING;
    DrawTextEx(font_small, "NEXT", (Vector2){x, y}, FONT_SMALL_SIZE, 2,
               (Color){56, 56, 56, 255});
    y += FONT_SMALL_SIZE + LABEL_BOTTOM_MARGIN;
    y += draw_next_shape(x, y, NEXT_CELL_SIZE);
    y += RHS_PADDING;
    DrawLine(x, y, RENDER_W - RHS_PADDING, y, DIVIDER_COLOR);

    // CONTROLS
    y += RHS_PADDING;
    struct {
        const char *key;
        const char *desc;
    } binds[] = {
        {"< >", "move"},       {"UP", "rotate cw"},  {"Z", "rotate ccw"},
        {"DOWN", "soft drop"}, {"SPC", "hard drop"}, {"P", "pause"},
        {"R", "restart"},
    };
    int n_binds = 7;
    int key_row_h = FONT_SMALL_SIZE + 8; // extra spacing between rows
    int key_box_h = FONT_SMALL_SIZE + 2;

    DrawTextEx(font_small, "CONTROLS", (Vector2){x, y}, FONT_SMALL_SIZE, 2,
               (Color){46, 46, 46, 255});
    y += FONT_SMALL_SIZE + LABEL_BOTTOM_MARGIN;

    for (int i = 0; i < n_binds; i++) {
        Vector2 key_sz =
            MeasureTextEx(font_small, binds[i].key, FONT_SMALL_SIZE, 2);
        int kw = (int)key_sz.x + 8;

        DrawRectangle(x, y, kw, key_box_h, (Color){22, 22, 22, 255});
        DrawRectangleLines(x, y, kw, key_box_h, (Color){36, 36, 36, 255});
        DrawTextEx(font_small, binds[i].key, (Vector2){x + 4, y + 1},
                   FONT_SMALL_SIZE, 2, (Color){56, 56, 56, 255});
        DrawTextEx(font_small, binds[i].desc, (Vector2){x + kw + 8, y + 1},
                   FONT_SMALL_SIZE, 2, (Color){40, 40, 40, 255});

        y += key_row_h;
    }

    y += RHS_PADDING;

    // CREDIT
    Vector2 credit_sz =
        MeasureTextEx(font_small, CREDIT_TXT, FONT_SMALL_SIZE, 2);
    int sidebar_w = RENDER_W - LHS_WIDTH - 2 * RHS_PADDING;
    int credit_x = x + (sidebar_w - (int)credit_sz.x) / 2;
    int credit_y = RENDER_H - RHS_PADDING - FONT_SMALL_SIZE;
    DrawTextEx(font_small, CREDIT_TXT, (Vector2){credit_x, credit_y},
               FONT_SMALL_SIZE, 2, (Color){34, 34, 34, 255});
}

static void draw_pause_overlay(void) {
    DrawRectangle(0, 0, RENDER_W, RENDER_H, (Color){17, 17, 17, 190});
}

static void render(void) {
    BeginDrawing();
    ClearBackground(BG);

    draw_header();
    draw_dividers();
    draw_grid();
    draw_sidebar();

    if (paused)
        draw_pause_overlay();

    EndDrawing();
}

int main(void) {
    ui_init();

    while (!WindowShouldClose()) {
        if (new_game) {
            ctetris_init();
            if (score > high_score) {
                hs_save(score);
                high_score = score;
            }
            score = lines = level = 0;
            new_game = false;
        }
        input();
        update();
        render();
    }

    if (score > high_score)
        hs_save(score);

    ui_shutdown();
    return 0;
}
