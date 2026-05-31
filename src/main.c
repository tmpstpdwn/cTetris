#include "raylib.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

#define ROWS 20
#define COLS 10

#define OFFSETS_COUNT 4
#define CELL_SIZE 45

#define SHAPE_COUNT 6

#define SHADOW_SHAPE_COLOR (Color){ 42, 42, 42, 255 }
#define BOARD_BG (Color){ 17, 17, 17, 255 }

#define COLOR_N  (Color){  30,  30,  30, 255 }
#define COLOR_O  (Color){  93, 202, 165, 255 }
#define COLOR_L  (Color){ 127, 119, 221, 255 }
#define COLOR_I  (Color){  55, 138, 221, 255 }
#define COLOR_T  (Color){ 216,  90,  48, 255 }
#define COLOR_S  (Color){ 186, 117,  23, 255 }

enum ShapeType {
    N,
    O,
    L,
    I,
    T,
    S,
};

enum Dir {
    LEFT,
    RIGHT, 
    DOWN
};

struct Shape {
    enum ShapeType type;
    Vector2 offsets[OFFSETS_COUNT];
    Vector2 pos;
    Color color;
};

static enum ShapeType board[ROWS][COLS] = {0};

static Vector2 offsets_o[OFFSETS_COUNT] = {{ 0,  0}, { 0,  1}, { 1,  0}, { 1,  1}};
static Vector2 offsets_l[OFFSETS_COUNT] = {{ 0, -1}, { 0,  0}, { 0,  1}, { 1,  1}};
static Vector2 offsets_i[OFFSETS_COUNT] = {{ 0, -1}, { 0,  0}, { 0,  1}, { 0,  2}};
static Vector2 offsets_t[OFFSETS_COUNT] = {{-1,  0}, { 0, -1}, { 0,  0}, { 1,  0}};
static Vector2 offsets_s[OFFSETS_COUNT] = {{ 0,  0}, { 1,  0}, { 0,  1}, {-1,  1}};

static Vector2 *offsets[SHAPE_COUNT] = {NULL, offsets_o, offsets_l, offsets_i, offsets_t, offsets_s};
static Color colors[SHAPE_COUNT] = {COLOR_N, COLOR_O, COLOR_L, COLOR_I, COLOR_T, COLOR_S};

struct Shape get_random_shape(void) {
    struct Shape shape;
    shape.type = GetRandomValue(1, SHAPE_COUNT - 1);

    memcpy(shape.offsets, offsets[shape.type], sizeof(shape.offsets));

    shape.color = colors[shape.type];

    int y = 0;
    int x_min = 0; int x_max = 0;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        if (shape.offsets[i].y <     y)     y = shape.offsets[i].y;
        if (shape.offsets[i].x < x_min) x_min = shape.offsets[i].x;
        if (shape.offsets[i].x > x_max) x_max = shape.offsets[i].x;
    }

    int d = x_max - x_min;
    int w = (d < 0)? -d: d;

    shape.pos.y = -y;
    shape.pos.x = (int)((COLS - w) / 2);

    return shape;
}

bool shape_collides(struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        Vector2 offset = shape->offsets[i];

        int x = shape->pos.x + offset.x;
        int y = shape->pos.y + offset.y;

        if (x < 0 || x >= COLS) return true;
        if (y < 0 || y >= ROWS) return true;
        if (board[y][x] != N) return true;
    }

    return false;
}

struct Shape get_shadow_shape(struct Shape shape) {
    while (!shape_collides(&shape))
        shape.pos.y++;

    shape.pos.y--;

    shape.color = SHADOW_SHAPE_COLOR;

    return shape;
}

void rotate_shape(struct Shape *shape) {
    if (shape->type == O) return;

    struct Shape rotated_shape = *shape;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = rotated_shape.offsets[i].x;
        int y = rotated_shape.offsets[i].y;

        rotated_shape.offsets[i].x = y;
        rotated_shape.offsets[i].y = -x;
    }

    if (shape_collides(&rotated_shape))
        return;

    *shape = rotated_shape;
}

bool move_shape(struct Shape *shape, enum Dir dir) {
    struct Shape next_shape = *shape;

    switch (dir) {
        case DOWN: next_shape.pos.y++;
            break;
        case LEFT: next_shape.pos.x--;
            break;
        case RIGHT: next_shape.pos.x++;
            break;
        default:
            break;
    };

    if (!shape_collides(&next_shape)) {
        *shape = next_shape;
        return true;
    }

    return false;
}

bool drop_shape(struct Shape *shape, double timeout) {
    static double last_time = -1;
    if (last_time == -1) last_time = GetTime();

    bool move_status = true;

    struct Shape new_shape = *shape;
    move_status = move_shape(&new_shape, DOWN);

    if (GetTime() - last_time >= timeout) {
        *shape = new_shape;
        last_time = GetTime();
    }

    return move_status;
}

void draw_shape(struct Shape shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        struct Vector2 offsets = shape.offsets[i];

        int x = (shape.pos.x + offsets.x) * CELL_SIZE;
        int y = (shape.pos.y + offsets.y) * CELL_SIZE;

        DrawRectangle(x+1, y+1, CELL_SIZE - 2, CELL_SIZE - 2, shape.color);
    }
}; 

void clear_board(void) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            board[i][j] = N;
        }
    }
}

void write_to_board(struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        Vector2 offset = shape->offsets[i];

        int x = shape->pos.x + offset.x;
        int y = shape->pos.y + offset.y;

        board[y][x] = shape->type;
    }
}

void draw_board(void) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            DrawRectangle(j * CELL_SIZE + 1, i * CELL_SIZE + 1, CELL_SIZE - 2, CELL_SIZE - 2, colors[board[i][j]]);
        }
    }
}

int main(void) {
    const int screenWidth = COLS * CELL_SIZE;
    const int screenHeight = ROWS * CELL_SIZE;

    SetRandomSeed(time(NULL));

    InitWindow(screenWidth, screenHeight, "Tetris");

    struct Shape curr_shape, shadow_shape;

    bool new_game = true;

    while (!WindowShouldClose()) {
        if (new_game) {
            clear_board();
            curr_shape = get_random_shape();
            shadow_shape = get_shadow_shape(curr_shape);
            new_game = false;
        }

        if (IsKeyPressed(KEY_UP)) {
            rotate_shape(&curr_shape);
            shadow_shape = get_shadow_shape(curr_shape);

        } else if (IsKeyPressed(KEY_LEFT)) {
            move_shape(&curr_shape, LEFT);
            shadow_shape = get_shadow_shape(curr_shape);

        } else if (IsKeyPressed(KEY_RIGHT)) {
            move_shape(&curr_shape, RIGHT);
            shadow_shape = get_shadow_shape(curr_shape);

        } else if (IsKeyPressed(KEY_SPACE)) { // Hard drop
            curr_shape = shadow_shape;
            curr_shape.color = colors[curr_shape.type];
        }

        if (IsKeyDown(KEY_DOWN)) {
            drop_shape(&curr_shape, 0.1);
        }

        if (!drop_shape(&curr_shape, 1)) {
            write_to_board(&curr_shape);

            curr_shape = get_random_shape();
            shadow_shape = get_shadow_shape(curr_shape);

            if (shape_collides(&curr_shape))
                new_game = true;
        }

        BeginDrawing();
        ClearBackground(BOARD_BG);
        draw_board();
        draw_shape(shadow_shape);
        draw_shape(curr_shape);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
