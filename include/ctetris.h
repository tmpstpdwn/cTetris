#ifndef CTETRIS_H
#define CTETRIS_H

#include <stdbool.h>
#include <stddef.h>

#define ROWS 20
#define COLS 10
#define OFFSETS_COUNT 4
#define SHAPE_COUNT 7

#define MOVES_BEFORE_LOCK 15
#define SHAPE_LOCK_DELAY 0.5f

#define SOFT_DROP_DELAY 0.05f
#define STRAFE_DELAY 0.2f

#define COMBO_BONUS 50

#define EMPTY_GRID_CELL -1

struct Coord {
    int x;
    int y;
};

enum ShapeType { O, L, J, I, T, S, Z };

struct InputState {
    bool rotate_cw_pressed;
    bool rotate_acw_pressed;

    bool strafe_left_pressed;
    bool strafe_left_held;
    bool strafe_right_pressed;
    bool strafe_right_held;

    bool soft_drop_held;
    bool hard_drop_pressed;
};

struct Shape {
    enum ShapeType type;
    struct Coord offsets[OFFSETS_COUNT];
    struct Coord pos;
};

struct LineClear {
    int row_index;
    enum ShapeType row[COLS];
};

struct LineMove {
    int row_from_index;
    int row_to_index;
    enum ShapeType row[COLS];
};

enum CTetrisEventType {
    CTETRIS_EVENT_NONE,

    CTETRIS_EVENT_NEW_GAME,
    CTETRIS_EVENT_NEW_SHAPE,

    CTETRIS_EVENT_HARD_DROP,

    CTETRIS_EVENT_SCORE_UPDATE,
    CTETRIS_EVENT_LINES_UPDATE,
    CTETRIS_EVENT_LEVEL_UPDATE,
    CTETRIS_EVENT_COMBO_UPDATE,

    CTETRIS_EVENT_LOCK_START,
    CTETRIS_EVENT_LOCK_CANCEL,
    CTETRIS_EVENT_LOCK_DONE,

    CTETRIS_EVENT_LINE_CLEAR,
    CTETRIS_EVENT_GRID_COMPACT,

    CTETRIS_EVENT_GAME_OVER
};

struct CTetrisEvent {
    enum CTetrisEventType type;
    // hard_drop.
    struct Coord from;
    struct Coord to;

    // new_shape, hard_drop, lock_timer.
    struct Shape shape;

    int val;

    struct LineClear line_clears[4];
    struct LineMove line_moves[ROWS];
    int line_count;
};

void ctetris_init(void);
void ctetris_post_input(struct InputState input_state);

enum ShapeType ctetris_get_grid_cell(size_t row, size_t col);
struct Shape ctetris_get_curr_shape(void);
struct Shape ctetris_get_curr_shadow_shape(void);
struct Shape ctetris_get_next_shape(void);

void ctetris_update(double delta);
struct CTetrisEvent ctetris_event_get(void);

#endif
