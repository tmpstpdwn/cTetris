#include "raylib.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

#define ROWS 20
#define COLS 10
#define CELL_SIZE 45

#define OFFSETS_COUNT 4
#define SHAPE_COUNT 9
#define DIR_COUNT 3 // count of shape movement directions;

#define MOVES_BEFORE_LOCK 15
#define SHAPE_LOCK_DELAY 0.5f

#define DROP_TIMEOUT 0.5f
#define SOFT_DROP_TIMEOUT 0.05f
#define MOVE_TIMEOUT 0.2f

#define BOARD_BG (Color){ 17, 17, 17, 255 }

#define COLOR_N  (Color){  30,  30,  30, 255 }
#define COLOR_O  (Color){  93, 202, 165, 255 }
#define COLOR_L  (Color){ 127, 119, 221, 255 }
#define COLOR_I  (Color){  55, 138, 221, 255 }
#define COLOR_T  (Color){ 216,  90,  48, 255 }
#define COLOR_S  (Color){ 186, 117,  23, 255 }
#define COLOR_J  (Color){ 212,  83, 126, 255 }
#define COLOR_Z  (Color){  99, 153,  34, 255 }
#define COLOR_X  (Color){  42,  42,  42, 255 } // SHADOW.

enum MoveStatus {
    CAN_MOVE,
    CANT_MOVE,
    MOVED
};

enum ShapeType {
    N,
    X,
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
};

static enum ShapeType board[ROWS][COLS] = {0};

static Vector2 offsets_o[OFFSETS_COUNT] = {{ 0,  0}, { 0,  1}, { 1,  0}, { 1,  1}};
static Vector2 offsets_l[OFFSETS_COUNT] = {{ 0, -1}, { 0,  0}, { 0,  1}, { 1,  1}};
static Vector2 offsets_i[OFFSETS_COUNT] = {{ 0, -1}, { 0,  0}, { 0,  1}, { 0,  2}};
static Vector2 offsets_t[OFFSETS_COUNT] = {{-1,  0}, { 0, -1}, { 0,  0}, { 1,  0}};
static Vector2 offsets_s[OFFSETS_COUNT] = {{ 0,  0}, { 1,  0}, { 0,  1}, {-1,  1}};
static Vector2 offsets_j[OFFSETS_COUNT] = {{ 0, -1}, { 0,  0}, { 0,  1}, {-1,  1}};
static Vector2 offsets_z[OFFSETS_COUNT] = {{ 0,  0}, {-1,  0}, { 0,  1}, { 1,  1}};

static Vector2 *offsets[SHAPE_COUNT] = {NULL, NULL, offsets_o, offsets_l, offsets_j, offsets_i, offsets_t, offsets_s, offsets_z};
static Color colors[SHAPE_COUNT] = {COLOR_N, COLOR_X, COLOR_O, COLOR_L, COLOR_J, COLOR_I, COLOR_T, COLOR_S, COLOR_Z};

static double last_move_time[DIR_COUNT];

struct Shape get_random_shape(void) {
    struct Shape shape;
    shape.type = GetRandomValue(2, SHAPE_COUNT - 1);

    memcpy(shape.offsets, offsets[shape.type], sizeof(shape.offsets));

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

bool shape_collides(const struct Shape *shape) {
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

    shape.type = X;

    return shape;
}

bool rotate_shape(struct Shape *shape) {
    if (shape->type == O) return false;

    struct Shape rotated_shape = *shape;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = rotated_shape.offsets[i].x;
        int y = rotated_shape.offsets[i].y;

        rotated_shape.offsets[i].x = y;
        rotated_shape.offsets[i].y = -x;
    }

    if (shape_collides(&rotated_shape))
        return false;

    *shape = rotated_shape;
    return true;
}

enum MoveStatus move_shape(struct Shape *shape, enum Dir dir, double timeout) {
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
        if (last_move_time[dir] == -1 || GetTime() - last_move_time[dir] >= timeout) {
            *shape = next_shape;
            last_move_time[dir] = GetTime();
            return MOVED;
        }
        return CAN_MOVE;
    }

    return CANT_MOVE;
}

void draw_shape(const struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        Vector2 offset = shape->offsets[i];

        int x = (shape->pos.x + offset.x) * CELL_SIZE;
        int y = (shape->pos.y + offset.y) * CELL_SIZE;

        DrawRectangle(x+1, y+1, CELL_SIZE - 2, CELL_SIZE - 2, colors[shape->type]);
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

    InitWindow(screenWidth, screenHeight, "cTetris");

    struct Shape curr_shape, shadow_shape;

    double landed = 0;
    int moves_since_landing = 0;

    bool new_game = true;
    bool hard_drop = false;

    while (!WindowShouldClose()) {
        if (new_game) {
            clear_board();
            curr_shape = get_random_shape();
            shadow_shape = get_shadow_shape(curr_shape);
            new_game = false;
            last_move_time[LEFT] = last_move_time[RIGHT] =  -1;
            last_move_time[DOWN] = GetTime();
        }

        bool new_move = false;

        if (IsKeyPressed(KEY_UP)) {
            new_move = new_move || rotate_shape(&curr_shape);
        }

        if (IsKeyDown(KEY_LEFT)) {
            new_move = new_move || (move_shape(&curr_shape, LEFT, MOVE_TIMEOUT) == MOVED);
        } else if (IsKeyDown(KEY_RIGHT)) {
            new_move = new_move || (move_shape(&curr_shape, RIGHT, MOVE_TIMEOUT) == MOVED);
        }

        if (IsKeyPressed(KEY_SPACE)) { // Hard drop
            enum ShapeType curr_t = curr_shape.type;
            curr_shape = shadow_shape;
            curr_shape.type = curr_t;
            hard_drop = true;
        }

        if (IsKeyDown(KEY_DOWN)) { // Soft drop
            move_shape(&curr_shape, DOWN, SOFT_DROP_TIMEOUT);
        }

        if (new_move) {
            shadow_shape = get_shadow_shape(curr_shape);
            if (landed) {
                if (moves_since_landing < MOVES_BEFORE_LOCK - 1) {
                    landed = 0;
                    moves_since_landing++;
                }
            }
        }

        if (move_shape(&curr_shape, DOWN, DROP_TIMEOUT) == CANT_MOVE) {
            if (!landed && !hard_drop) {
                landed = GetTime();
            } else if (GetTime() - landed >= SHAPE_LOCK_DELAY) {
                write_to_board(&curr_shape);
                curr_shape = get_random_shape();
                shadow_shape = get_shadow_shape(curr_shape);

                if (shape_collides(&curr_shape))
                    new_game = true;

                landed = 0;
                moves_since_landing = 0;
                hard_drop = false;
            }
        } else {
            landed = 0;
            moves_since_landing = 0;
        }

        BeginDrawing();
        ClearBackground(BOARD_BG);
        draw_board();
        draw_shape(&shadow_shape);
        draw_shape(&curr_shape);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
