/* [INCLUDES] */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ctetris.h"

/* [DEFINES] */

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define TIMER_INACTIVE (-1.0)
#define TIMER_START (0.0)

#define EVENT_QUEUE_CAP 32

/* [ENUMS AND STRUCTS] */

enum EngineState { STATE_DROPPING, STATE_LOCKING };
enum MovementDir { LEFT, RIGHT, DOWN };
enum RotationDir { CLOCKWISE, ANTI_CLOCKWISE };

struct CollisionStatus {
    bool hit;
    struct Coord offset;
};

/* [VARIABLES] */

// Shape definitions.

static struct Shape SHAPE_O = {
    .type = O, .offsets = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};
static struct Shape SHAPE_L = {
    .type = L, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {1, 1}}, .pos = {0, 0}};
static struct Shape SHAPE_I = {
    .type = I, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {2, 0}}, .pos = {0, 0}};
static struct Shape SHAPE_T = {
    .type = T, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {0, 1}}, .pos = {0, 0}};
static struct Shape SHAPE_S = {
    .type = S, .offsets = {{-1, 1}, {0, 1}, {0, 0}, {1, 0}}, .pos = {0, 0}};
static struct Shape SHAPE_J = {
    .type = J, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {-1, 1}}, .pos = {0, 0}};
static struct Shape SHAPE_Z = {
    .type = Z, .offsets = {{-1, 0}, {0, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};

static struct Shape *shapes[SHAPE_COUNT] = {
    [O] = &SHAPE_O, [L] = &SHAPE_L, [J] = &SHAPE_J, [I] = &SHAPE_I,
    [T] = &SHAPE_T, [S] = &SHAPE_S, [Z] = &SHAPE_Z};

static int next_shape_index = 0;

// Game state

static enum EngineState engine_state;
static enum ShapeType grid[ROWS][COLS];

static struct Shape curr_shape, shadow_shape;
static double engine_state_timer;
static int lock_move_count, y_max;

static int score, combo, level, lines;

static double curr_shape_strafe_timer;

static bool hard_drop;
static bool soft_drop;

static bool touched_floor;

static int moves;

// Event queue

static struct CTetrisEvent event_queue[EVENT_QUEUE_CAP];
static int eq_head;
static int eq_tail;

/* [FN DCL] */

// event queue.
static void event_push(struct CTetrisEvent ev);
struct CTetrisEvent ctetris_event_get(void);

// grid Utilities.
static void clear_grid(void);
static void write_to_grid(struct Shape *shape);

// randomization and shuffling.
static void shuffle_shapes(void);
static struct Shape get_random_shape(void);

// shape Utilities.
static int get_shape_y_max(const struct Shape *shape);
static struct CollisionStatus shape_collides(const struct Shape *shape);
static bool is_shape_grounded(void);
static struct Shape get_shadow_shape(struct Shape shape);
static bool rotate_shape(struct Shape *shape, enum RotationDir dir);
static bool move_shape(struct Shape *shape, enum MovementDir dir,
                       double last_move_timer, double delay);

// core.
void ctetris_init(void);
static bool curr_shape_rotate(enum RotationDir dir);
static bool curr_shape_strafe(enum MovementDir dir, bool delay);

struct Shape ctetris_get_curr_shape(void);
struct Shape ctetris_get_curr_shadow_shape(void);
struct Shape ctetris_get_next_shape(void);
enum ShapeType ctetris_get_grid_cell(size_t row, size_t col);

void ctetris_post_input(struct InputState input_state);
static void handle_new_move(void);
static double get_drop_delay(void);
static int clear_lines(void);
static void scoring(int lines_cleared);
static void endgame_or_reset(void);
static void handle_grounded_shape(void);
static void handle_airborne_shape(void);
void ctetris_update(double delta);

/* [FN DEF] */

static void event_push(struct CTetrisEvent ev) {
    int next = (eq_tail + 1) % EVENT_QUEUE_CAP;
    if (next == eq_head)
        return; // drop if full
    event_queue[eq_tail] = ev;
    eq_tail = next;
}

struct CTetrisEvent ctetris_event_get(void) {
    if (eq_head == eq_tail)
        return (struct CTetrisEvent){.type = CTETRIS_EVENT_NONE};
    struct CTetrisEvent ev = event_queue[eq_head];
    eq_head = (eq_head + 1) % EVENT_QUEUE_CAP;
    return ev;
}

// Grid Utilities.

static void clear_grid(void) {
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            grid[i][j] = EMPTY_GRID_CELL;
}

static void write_to_grid(struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = shape->pos.x + shape->offsets[i].x;
        int y = shape->pos.y + shape->offsets[i].y;
        grid[y][x] = shape->type;
    }
}

static int get_shape_y_max(const struct Shape *shape) {
    int max_y = 0;
    for (int i = 1; i < OFFSETS_COUNT; i++) {
        int cy = (int)(shape->pos.y + shape->offsets[i].y);
        if (cy > max_y)
            max_y = cy;
    }
    return max_y;
}

static void shuffle_shapes(void) {
    for (int i = 6; i > 0; i--) {
        int j = rand() % (i + 1);
        struct Shape *tmp = shapes[i];
        shapes[i] = shapes[j];
        shapes[j] = tmp;
    }
}

static struct Shape get_random_shape(void) {
    struct Shape shape;

    if (next_shape_index == SHAPE_COUNT - 1) {
        shape = *shapes[next_shape_index];
        shuffle_shapes();
        next_shape_index = 0;
    } else {
        shape = *shapes[next_shape_index++];
    }

    int y = 0;
    for (int i = 0; i < OFFSETS_COUNT; i++)
        y = (shape.offsets[i].y < y) ? shape.offsets[i].y : y;

    shape.pos = (struct Coord){.y = -y, .x = (int)(COLS / 2)};
    return shape;
}

static struct CollisionStatus shape_collides(const struct Shape *shape) {
    struct CollisionStatus status = {0};
    bool out_of_bounds = false;
    int worst = 0;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        struct Coord offset = shape->offsets[i];
        int x = shape->pos.x + offset.x;
        int y = shape->pos.y + offset.y;

        if (x < 0 || y < 0 || x >= COLS || y >= ROWS) {
            int dx = 0, dy = 0;
            if (x < 0)
                dx = -x;
            if (x >= COLS)
                dx = x - (COLS - 1);
            if (y < 0)
                dy = -y;
            if (y >= ROWS)
                dy = y - (ROWS - 1);

            int dist = MAX(dx, dy);
            if (!out_of_bounds || dist > worst) {
                worst = dist;
                status.offset = offset;
                if (dx >= dy)
                    status.offset.y = 0;
                else
                    status.offset.x = 0;
            }
            status.hit = true;
            out_of_bounds = true;

        } else if (!out_of_bounds && grid[y][x] != EMPTY_GRID_CELL) {
            status.hit = true;
            status.offset = offset;
        }
    }
    return status;
}

static bool is_shape_grounded(void) {
    struct Shape down = curr_shape;
    down.pos.y++;
    return shape_collides(&down).hit;
}

static struct Shape get_shadow_shape(struct Shape shape) {
    int max_drop = ROWS;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = shape.pos.x + shape.offsets[i].x;
        int y = shape.pos.y + shape.offsets[i].y;

        int dist = 0;
        for (int check_y = y + 1; check_y < ROWS; check_y++) {
            if (grid[check_y][x] != -1)
                break;
            dist++;
        }

        if (dist < max_drop) {
            max_drop = dist;
        }
    }

    shape.pos.y += max_drop;
    return shape;
}

void ctetris_init(void) {
    clear_grid();
    eq_head = eq_tail = 0;

    shuffle_shapes();
    curr_shape = get_random_shape();
    shadow_shape = get_shadow_shape(curr_shape);
    y_max = get_shape_y_max(&curr_shape);

    engine_state = STATE_DROPPING;
    engine_state_timer = TIMER_START;
    lock_move_count = 0;
    touched_floor = false;

    moves = 0;

    hard_drop = soft_drop = false;

    score = combo = lines = 0;
    level = 1;

    curr_shape_strafe_timer = TIMER_INACTIVE;

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_NEW_GAME});
    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_NEW_SHAPE});
}

// Rotation.

static bool rotate_shape(struct Shape *shape, enum RotationDir dir) {
    if (shape->type == O)
        return true;

    struct Shape rotated = *shape;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = rotated.offsets[i].x;
        int y = rotated.offsets[i].y;
        switch (dir) {
        case CLOCKWISE:
            rotated.offsets[i].x = -y;
            rotated.offsets[i].y = x;
            break;
        case ANTI_CLOCKWISE:
            rotated.offsets[i].x = y;
            rotated.offsets[i].y = -x;
            break;
        }
    }

    struct CollisionStatus status = shape_collides(&rotated);
    if (!status.hit) {
        *shape = rotated;
        return true;
    }

    rotated.pos.x -= status.offset.x;
    rotated.pos.y -= status.offset.y;
    if (!shape_collides(&rotated).hit) {
        *shape = rotated;
        return true;
    }

    return false;
}

static bool move_shape(struct Shape *shape, enum MovementDir dir,
                       double last_move_timer, double delay) {

    struct Shape next = *shape;

    switch (dir) {
    case DOWN:
        next.pos.y++;
        break;
    case LEFT:
        next.pos.x--;
        break;
    case RIGHT:
        next.pos.x++;
        break;
    }

    if (!shape_collides(&next).hit) {
        if (last_move_timer == TIMER_INACTIVE || last_move_timer >= delay) {
            *shape = next;
            return true;
        }
    }
    return false;
}

static bool curr_shape_rotate(enum RotationDir dir) {
    return rotate_shape(&curr_shape, dir);
}

static bool curr_shape_strafe(enum MovementDir dir, bool delay) {
    static enum MovementDir last_dir = -1;

    if (dir != LEFT && dir != RIGHT)
        return false;

    double timer = (last_dir == -1 || last_dir != dir || !delay)
                       ? TIMER_INACTIVE
                       : curr_shape_strafe_timer;

    if (move_shape(&curr_shape, dir, timer, STRAFE_DELAY)) {
        curr_shape_strafe_timer = TIMER_START;
        last_dir = dir;
        return true;
    }

    return false;
}

static int clear_lines(void) {
    struct CTetrisEvent clear_ev = {.type = CTETRIS_EVENT_LINE_CLEAR,
                                    .line_count = 0};

    for (int r = 0; r < ROWS; r++) {
        bool full = true;
        for (int c = 0; c < COLS; c++)
            if (grid[r][c] == EMPTY_GRID_CELL) {
                full = false;
                break;
            }

        if (full) {
            int i = clear_ev.line_count++;
            clear_ev.line_clears[i].row_index = r;
            memcpy(clear_ev.line_clears[i].row, grid[r],
                   COLS * sizeof(enum ShapeType));
        }
    }

    int rows_cleared = clear_ev.line_count;
    if (rows_cleared == 0)
        return 0;

    event_push(clear_ev);

    struct CTetrisEvent compact_ev = {.type = CTETRIS_EVENT_GRID_COMPACT,
                                      .line_count = 0};

    int write = ROWS - 1;
    for (int read = ROWS - 1; read >= 0; read--) {
        bool full = false;
        for (int i = 0; i < rows_cleared; i++)
            if (clear_ev.line_clears[i].row_index == read) {
                full = true;
                break;
            }

        if (!full) {
            if (write != read) {
                int m = compact_ev.line_count++;
                compact_ev.line_moves[m].row_from_index = read;
                compact_ev.line_moves[m].row_to_index = write;
                memcpy(compact_ev.line_moves[m].row, grid[read],
                       COLS * sizeof(enum ShapeType));

                memcpy(grid[write], grid[read], COLS * sizeof(enum ShapeType));
            }
            write--;
        }
    }

    for (int r = write; r >= 0; r--)
        memset(grid[r], EMPTY_GRID_CELL, COLS * sizeof(enum ShapeType));

    event_push(compact_ev);

    return rows_cleared;
}

static int get_line_bonus(uint8_t n) {
    switch (n) {
    case 1:
        return 100;
    case 2:
        return 300;
    case 3:
        return 500;
    case 4:
        return 800;
    default:
        return 0;
    }
}

static void scoring(int lines_cleared) {
    if (lines_cleared) {
        int bonus = get_line_bonus(lines_cleared);

        score += bonus * level;

        lines += lines_cleared;
        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_LINES_UPDATE,
                                         .val = lines});

        int new_possible_level = (lines / 10) + 1;
        if (new_possible_level != level) {
            level = new_possible_level;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_LEVEL_UPDATE,
                                             .val = level});
        }
        if (combo > 0) {
            score += combo * COMBO_BONUS * level;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_COMBO_UPDATE,
                                             .val = combo});
        }
        combo++;

        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SCORE_UPDATE,
                                         .val = score});

    } else {
        if (combo) {
            combo = 0;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_COMBO_UPDATE,
                                             .val = combo});
        }
    }
}

enum ShapeType ctetris_get_grid_cell(size_t row, size_t col) {
    if (row < ROWS && col < COLS)
        return grid[row][col];
    return EMPTY_GRID_CELL;
}

struct Shape ctetris_get_curr_shape(void) { return curr_shape; }
struct Shape ctetris_get_curr_shadow_shape(void) { return shadow_shape; }
struct Shape ctetris_get_next_shape(void) { return *shapes[next_shape_index]; }

static double get_drop_delay(void) {
    if (soft_drop)
        return SOFT_DROP_DELAY;
    double delay = pow(0.8 - ((level - 1) * 0.007), level - 1);
    return delay < SOFT_DROP_DELAY ? SOFT_DROP_DELAY : delay;
}

static void handle_new_move(void) {
    if (moves == 0)
        return;

    shadow_shape = get_shadow_shape(curr_shape);

    if (engine_state == STATE_DROPPING && !touched_floor) {
        return;
    }

    if (lock_move_count < MOVES_BEFORE_LOCK - 1) {
        engine_state_timer = TIMER_START;
        lock_move_count += moves;
    } else {
        hard_drop = true;
    }

    moves = 0;
}

static void endgame_or_reset(void) {
    curr_shape = get_random_shape();
    shadow_shape = get_shadow_shape(curr_shape);

    if (shape_collides(&curr_shape).hit) {
        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_GAME_OVER});
        return;
    }

    engine_state = STATE_DROPPING;
    engine_state_timer = TIMER_START;
    lock_move_count = 0;
    y_max = get_shape_y_max(&curr_shape);
    curr_shape_strafe_timer = TIMER_INACTIVE;
    touched_floor = false;

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_NEW_SHAPE});
}

static void handle_grounded_shape(void) {
    bool timer_expired = (engine_state == STATE_LOCKING &&
                          engine_state_timer >= SHAPE_LOCK_DELAY);

    if (!hard_drop && !timer_expired) {
        if (engine_state != STATE_LOCKING) {
            engine_state = STATE_LOCKING;
            engine_state_timer = TIMER_START;
            touched_floor = true;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_LOCK_START,
                                             .shape = curr_shape});
        }
        return;
    }

    if (hard_drop) {
        struct Coord from = curr_shape.pos;
        score += (shadow_shape.pos.y - curr_shape.pos.y) * 2;

        curr_shape.pos = shadow_shape.pos;

        hard_drop = false;

        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_HARD_DROP,
                                         .from = from,
                                         .to = curr_shape.pos,
                                         .shape = curr_shape});

        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SCORE_UPDATE,
                                         .val = score});
    }

    write_to_grid(&curr_shape);

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_LOCK_DONE,
                                     .shape = curr_shape});

    scoring(clear_lines());
    endgame_or_reset();
}

static void handle_airborne_shape(void) {
    if (engine_state == STATE_LOCKING) {
        engine_state = STATE_DROPPING;
        engine_state_timer = TIMER_START;
        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_LOCK_CANCEL,
                                         .shape = curr_shape});
    }

    struct Shape next_possible_shape = curr_shape;
    double drop_delay = get_drop_delay();
    if (move_shape(&next_possible_shape, DOWN, engine_state_timer,
                   drop_delay)) {
        if (soft_drop) {
            score++;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SCORE_UPDATE,
                                             .val = score});
        }

        curr_shape = next_possible_shape;
        engine_state_timer = TIMER_START;
    }

    int current_max_y = get_shape_y_max(&curr_shape);
    if (current_max_y > y_max) {
        y_max = current_max_y;
        lock_move_count = 0;
        touched_floor = false;
    }
}

void ctetris_post_input(struct InputState input_state) {
    moves = 0;

    if (input_state.rotate_cw_pressed) {
        if (curr_shape_rotate(CLOCKWISE))
            moves++;
    } else if (input_state.rotate_acw_pressed) {
        if (curr_shape_rotate(ANTI_CLOCKWISE))
            moves++;
    }

    if (input_state.strafe_left_pressed) {
        if (curr_shape_strafe(LEFT, false))
            moves++;
    } else if (input_state.strafe_left_held) {
        if (curr_shape_strafe(LEFT, true))
            moves++;
    } else if (input_state.strafe_right_pressed) {
        if (curr_shape_strafe(RIGHT, false))
            moves++;
    } else if (input_state.strafe_right_held) {
        if (curr_shape_strafe(RIGHT, true))
            moves++;
    }

    soft_drop = input_state.soft_drop_held;
    hard_drop = input_state.hard_drop_pressed;
}

void ctetris_update(double delta) {
    if (engine_state_timer != TIMER_INACTIVE)
        engine_state_timer += delta;
    if (curr_shape_strafe_timer != TIMER_INACTIVE)
        curr_shape_strafe_timer += delta;

    handle_new_move();

    if (is_shape_grounded() || hard_drop)
        handle_grounded_shape();
    else
        handle_airborne_shape();
}

/* [END] */
