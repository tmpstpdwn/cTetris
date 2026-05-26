#include "raylib.h"
#include <time.h>
#include <string.h>

#define ROWS 20
#define COLS 15

#define SHAPE_COUNT 5
#define OFFSETS_COUNT 4
#define CELL_SIZE 32

enum ShapeType {
    O,
    L,
    I,
    T,
    S,
};

enum Dir {
    LEFT,
    RIGHT, 
    UP, 
    DOWN
};

struct Shape {
    enum ShapeType type;
    Vector2 offsets[OFFSETS_COUNT];
    Vector2 pos;
    Color color;
};

static bool board[ROWS][COLS];

Vector2 offsets_o[OFFSETS_COUNT] = {{ 0,  0}, { 0,  1}, { 1,  0}, { 1,  1}};
Vector2 offsets_l[OFFSETS_COUNT] = {{ 0, -1}, { 0,  0}, { 0,  1}, { 1,  1}};
Vector2 offsets_i[OFFSETS_COUNT] = {{ 0, -1}, { 0,  0}, { 0,  1}, { 0,  2}};
Vector2 offsets_t[OFFSETS_COUNT] = {{-1,  0}, { 0, -1}, { 0,  0}, { 1,  0}};
Vector2 offsets_s[OFFSETS_COUNT] = {{ 0,  0}, { 1,  0}, { 0,  1}, {-1,  1}};

struct Shape get_random_shape(void) {
    static Vector2 *offsets[SHAPE_COUNT] = {offsets_o, offsets_l, offsets_i, offsets_t, offsets_s};
    static Color colors[SHAPE_COUNT] = {RED, GREEN, BLUE, ORANGE, VIOLET};

    int index = GetRandomValue(0, SHAPE_COUNT - 1);

    struct Shape shape;

    shape.type = index;
    memcpy(shape.offsets, offsets[index], sizeof(shape.offsets));
    shape.pos = (Vector2){5, 5};
    shape.color = colors[index];

    return shape;
}

bool shape_collides(struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        Vector2 offset = shape->offsets[i];

        int next_offset_x = shape->pos.x + offset.x;
        int next_offset_y = shape->pos.y + offset.y;

        if (next_offset_x < 0 || next_offset_x >= COLS) return true;
        if (next_offset_y < 0 || next_offset_y >= ROWS) return true;
        if (board[next_offset_y][next_offset_x]) return true;
    }

    return false;
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
        case UP: next_shape.pos.y--;
            break;
        case DOWN: next_shape.pos.y++;
            break;
        case LEFT: next_shape.pos.x--;
            break;
        case RIGHT: next_shape.pos.x++;
            break;
    };

    if (!shape_collides(&next_shape)) {
        *shape = next_shape;
        return true;
    }

    return false;
}

bool drop_shape(struct Shape *shape) {
    static double last_time = -1;
    if (last_time == -1) last_time = GetTime();

    bool move_status = true;

    if (GetTime() - last_time >= 0.5) {
        move_status = move_shape(shape, DOWN);
        last_time = GetTime();
    }

    return move_status;
}

void write_shape_to_board(struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        Vector2 offset = shape->offsets[i];

        int x = shape->pos.x + offset.x;
        int y = shape->pos.y + offset.y;

        board[y][x] = true;
    }
}

void draw_shape(struct Shape shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        struct Vector2 offsets = shape.offsets[i];

        int x = (shape.pos.x + offsets.x) * CELL_SIZE;
        int y = (shape.pos.y + offsets.y) * CELL_SIZE;

        DrawRectangle(x, y, CELL_SIZE, CELL_SIZE, shape.color);
    }
}; 

void draw_board(void) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (board[i][j])
                DrawRectangle(j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE, GRAY);
        }
    }
}

int main(void)
{
    const int screenWidth = COLS * CELL_SIZE;
    const int screenHeight = ROWS * CELL_SIZE;

    SetRandomSeed(time(NULL));

    InitWindow(screenWidth, screenHeight, "Tetris");
    SetTargetFPS(60);

    struct Shape curr_shape = get_random_shape();

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_UP)) move_shape(&curr_shape, UP);
        if (IsKeyPressed(KEY_LEFT)) move_shape(&curr_shape, LEFT);
        if (IsKeyPressed(KEY_RIGHT)) move_shape(&curr_shape, RIGHT);
        if (IsKeyPressed(KEY_SPACE)) curr_shape = get_random_shape();
        if (IsKeyPressed(KEY_R)) rotate_shape(&curr_shape);

        if (!drop_shape(&curr_shape)) {
            write_shape_to_board(&curr_shape);
            curr_shape = get_random_shape();
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_shape(curr_shape);
        draw_board();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
