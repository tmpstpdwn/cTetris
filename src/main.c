#include "raylib.h"
#include <time.h>
#include <stdio.h>

#define ROWS 20
#define COLS 15

#define SHAPE_COUNT 5
#define CELL_SIZE 32

struct Shape {
    Vector2 offsets[4];
    int height, width;
};

struct UIShape {
    struct Shape *shape;
    Vector2 pos;
    int rotation;
};

struct Shape shape_box = {
    .offsets = {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    .height = 2, .width = 2
};

struct Shape shape_l = {
    .offsets = {{0, 0}, {0, 1}, {0, 2}, {1, 2}},
    .height = 3, .width = 2
};

struct Shape shape_i = {
    .offsets = {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
    .height = 4, .width = 1
};

struct Shape shape_t = {
    .offsets = {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
    .height = 2, .width = 3
};

struct Shape shape_s = {
    .offsets = {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
    .height = 2, .width = 3
};

struct Shape *shapes[SHAPE_COUNT] = {&shape_box, &shape_l, &shape_i, &shape_t, &shape_s};

struct UIShape ui_shape = {
    .shape = NULL,
    .pos = {0, 0},
    .rotation = 0
};

void select_random_shape(void) {
    int index = GetRandomValue(0, SHAPE_COUNT - 1);
    ui_shape.shape = shapes[index];
}

void draw_shape(void) {
    for (int i = 0; i < 4; i++) {
        int offset_x = ui_shape.shape->offsets[i].x;
        int offset_y = ui_shape.shape->offsets[i].y;

        for (int j = 0; j < ui_shape.rotation; j++) {
            int t = offset_x;
            offset_x = offset_y;
            offset_y = -t;
        }

        int x = ui_shape.pos.x + CELL_SIZE * offset_x;
        int y = ui_shape.pos.y + CELL_SIZE * offset_y;

        DrawRectangleV((Vector2){x, y}, (Vector2){CELL_SIZE, CELL_SIZE}, LIGHTGRAY);
    }
}; 

int main(void)
{
    const int screenWidth = COLS * CELL_SIZE;
    const int screenHeight = ROWS * CELL_SIZE;

    SetRandomSeed(time(NULL));
    select_random_shape();

    InitWindow(screenWidth, screenHeight, "Tetris");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_RIGHT)) ui_shape.pos.x += CELL_SIZE;
        if (IsKeyPressed(KEY_LEFT)) ui_shape.pos.x -= CELL_SIZE;
        if (IsKeyPressed(KEY_UP)) ui_shape.pos.y -= CELL_SIZE;
        if (IsKeyPressed(KEY_DOWN)) ui_shape.pos.y += CELL_SIZE;

        if (IsKeyPressed(KEY_SPACE)) select_random_shape();

        if (IsKeyPressed(KEY_R)) {
            ui_shape.rotation  = (ui_shape.rotation + 1) % 4;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_shape();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
