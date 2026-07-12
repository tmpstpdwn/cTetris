/* [ CTETRIS_C ] */

/*
 * To understand how the engine works, start with the enums, structs, global
 * variables and then go straight to the `ctetris_update` fn which is the core
 * engine fn where everything happens.
 */

/* [ INCLUDES ] */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ctetris.h"

/* [ DEFINES ] */

// All non `TIMER_INACTIVE` timers will be incremented.
#define TIMER_INACTIVE (-1.0f) // Timer won't be incremented.
#define TIMER_START (0.0f)     // resets the timer.

// Event queue size.
#define EVENT_QUEUE_CAP 10

/* [ ENUMS AND STRUCTS ] */

enum EngineState { STATE_INACTIVE, STATE_DROPPING, STATE_LOCKING };
enum MovementDir { LEFT, RIGHT, DOWN };
enum RotationDir { CLOCKWISE, COUNTER_CLOCKWISE };

// Things particular to a single update tick.
struct UpdateContext {
    bool hard_drop;
    bool soft_drop;
    uint8_t moves;
};

/* [ VARIABLES ] */

// standard Tetris shape coordinate definitions, (0, 0) is the pivot.
// A shape's position corresponds to where the pivot is on the grid.
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

// Shape bag of 7.
static struct Shape *shape_bag[N] = {
    [O] = &SHAPE_O, [L] = &SHAPE_L, [J] = &SHAPE_J, [I] = &SHAPE_I,
    [T] = &SHAPE_T, [S] = &SHAPE_S, [Z] = &SHAPE_Z};

// Index of the next shape in the shape bag.
static uint8_t shape_bag_next_i;

// If the shape is airborne then the `engine_state` will be `STATE_DROPPING`, if
// it is grounded then `STATE_LOCKING`; if the engine hasn't been initialized or
// the game is over then `STATE_INACTIVE`.
static enum EngineState engine_state = STATE_INACTIVE;
// Timer for the current `engine_state`.
static double engine_state_timer = TIMER_INACTIVE;

// Tetris grid is made up of cells of the type `enum ShapeType`.
static enum ShapeType grid[ROWS][COLS];

static struct Shape curr_shape;
static double curr_shape_shift_timer; // Timer for auto-repeat shift.
// `curr_shape_y_max` represents the lowest row the current shape has reached.
static uint8_t curr_shape_y_max;
// Has the current shape landed?
static bool curr_shape_landed;
// `curr_shape_land_moves` represents number of moves since the current shape
// has landed.
static uint8_t curr_shape_land_moves;

// Engine game stats.
static uint32_t score;
static uint8_t combo, level, lines;

// Input buffer on which the `ctetris_update` fn will act on.
// Set on `ctetris_input_push` fn.
static struct InputState input_buffer;

// Event queue.
static struct CTetrisEvent event_queue[EVENT_QUEUE_CAP];
static uint8_t eq_head, eq_tail;

/* [ FN DCL ] */

// Event queue.
static void event_push(struct CTetrisEvent ev);

// Randomization.
static void shape_bag_shuffle(void);
static struct Shape shape_bag_next_get(void);

// Shape utilities.
static uint8_t shape_y_max_get(const struct Shape *shape);
static bool shape_collides(const struct Shape *shape,
                           struct Coord *worst_offset);
static bool shape_grounded(const struct Shape *shape);
static bool shape_rotate(struct Shape *shape, enum RotationDir dir);
static bool shape_move(struct Shape *shape, enum MovementDir dir,
                       double last_move_timer, double delay);

// Core.
static void input_buffer_process(struct UpdateContext *ctxt);

static bool curr_shape_rotate(enum RotationDir dir);
static bool curr_shape_shift(enum MovementDir dir, bool delay);

static void handle_new_move(struct UpdateContext *ctxt);
static double get_drop_delay(bool soft_drop);
static bool handle_grounded_shape(bool hard_drop);
static void handle_airborne_shape(bool soft_drop);

static void grid_write_shape(struct Shape *shape);
static void grid_clear_lines(void);
static void grid_clear(void);

static uint16_t scoring_get_bonus(uint8_t val);
static void scoring_score(struct CTetrisEvent *ev);

static void endgame_or_reset(void);

/* [ PUBLIC FN DEF ] */

void ctetris_init(void) {
    srand((unsigned)time(NULL));

    // Pretty much just setting core variables to valid starting values for a
    // new game.

    eq_head = eq_tail = 0;

    engine_state = STATE_DROPPING;
    engine_state_timer = TIMER_START;

    grid_clear();
    shape_bag_shuffle();

    curr_shape = shape_bag_next_get();
    curr_shape_y_max = shape_y_max_get(&curr_shape);
    curr_shape_land_moves = 0;
    curr_shape_landed = false;
    curr_shape_shift_timer = TIMER_INACTIVE;

    score = combo = lines = 0;
    level = 1;

    memset(&input_buffer, 0, sizeof(struct InputState));

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_NEW_SHAPE,
                                     .shape = curr_shape,
                                     .engine_inactive = false});
}

void ctetris_input_push(struct InputState input_state) {
    if (engine_state == STATE_INACTIVE)
        return;

    input_buffer = input_state;
}

void ctetris_update(double delta) {
    if (engine_state == STATE_INACTIVE)
        return;

    // Increment active timers.
    if (engine_state_timer != TIMER_INACTIVE)
        engine_state_timer += delta;
    if (curr_shape_shift_timer != TIMER_INACTIVE)
        curr_shape_shift_timer += delta;

    struct UpdateContext ctxt = {0};

    // Process buffered input.
    input_buffer_process(&ctxt);

    // In case there are new moves.
    handle_new_move(&ctxt);

    // In case the shape is grounded.
    if (!handle_grounded_shape(ctxt.hard_drop))
        // If it is airborne.
        handle_airborne_shape(ctxt.soft_drop);
}

struct CTetrisEvent ctetris_event_pop(void) {
    // If the queue is empty, return `CTETRIS_EVENT_NONE`.
    if (eq_head == eq_tail) {
        return (struct CTetrisEvent){.type = CTETRIS_EVENT_NONE};
    }
    struct CTetrisEvent ev = event_queue[eq_head];
    eq_head = (eq_head + 1) % EVENT_QUEUE_CAP;
    return ev;
}

struct Shape ctetris_shape_proj_get(void) {
    if (engine_state == STATE_INACTIVE)
        return (struct Shape){.type = N};

    uint8_t max_drop = ROWS;

    struct Shape proj_shape = curr_shape;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        int8_t x = curr_shape.pos.x + curr_shape.offsets[i].x;
        int8_t y = curr_shape.pos.y + curr_shape.offsets[i].y;

        uint8_t dist = 0;
        for (uint8_t check_y = y + 1; check_y < ROWS; check_y++) {
            if (grid[check_y][x] != N)
                break;
            dist++;
        }

        if (dist < max_drop) {
            max_drop = dist;
        }
    }

    proj_shape.pos.y += max_drop;
    return proj_shape;
}

struct Shape ctetris_shape_next_get(void) {
    if (engine_state == STATE_INACTIVE)
        return (struct Shape){.type = N};

    return *shape_bag[shape_bag_next_i];
}

/* [ INTERNAL FN DEF ] */

// Push an event to the event queue.
static void event_push(struct CTetrisEvent ev) {
    uint8_t next = (eq_tail + 1) % EVENT_QUEUE_CAP;
    if (next == eq_head)
        return; // When the queue overflows, just don't push.
    event_queue[eq_tail] = ev;
    eq_tail = next;
}

// Get the lowest row on the grid the given shape is occupying.
static uint8_t shape_y_max_get(const struct Shape *shape) {
    uint8_t max_y = 0;
    for (uint8_t i = 1; i < OFFSETS_COUNT; i++) {
        uint8_t cy = shape->pos.y + shape->offsets[i].y;
        if (cy > max_y)
            max_y = cy;
    }
    return max_y;
}

// Shuffle the shape bag.
static void shape_bag_shuffle(void) {
    for (uint8_t i = 6; i > 0; i--) {
        uint8_t j = rand() % (i + 1);
        struct Shape *tmp = shape_bag[i];
        shape_bag[i] = shape_bag[j];
        shape_bag[j] = tmp;
    }
    shape_bag_next_i = 0;
}

// Get a random shape from the shape bag.
static struct Shape shape_bag_next_get(void) {
    struct Shape shape;

    if (shape_bag_next_i == N - 1) {
        // In case on the last index then return it and then shuffle the bag.
        shape = *shape_bag[shape_bag_next_i];
        shape_bag_shuffle();
    } else {
        shape = *shape_bag[shape_bag_next_i++];
    }

    // Compute the shape's starting position so that it is horizontally centered
    // and vertically in the first 2 rows.
    int8_t y = 0;
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++)
        y = (shape.offsets[i].y < y) ? shape.offsets[i].y : y;

    shape.pos = (struct Coord){.y = -y, .x = COLS / 2};
    return shape;
}

// Returns if the shape is in a collision or not.
// Being out of bounds is also considered as collision and is given more
// priority than in bounds collision.
// In case if there is a collision then this fn returns true and populates the
// given `struct Coord *worst_offset` with the worst colliding offset.
static bool shape_collides(const struct Shape *shape,
                           struct Coord *worst_offset) {
    bool hit = false;
    bool out_of_bounds = false;
    int8_t worst_dist = 0;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        struct Coord offset = shape->offsets[i];
        int8_t x = shape->pos.x + offset.x;
        int8_t y = shape->pos.y + offset.y;

        if (x < 0 || y < 0 || x >= COLS || y >= ROWS) {
            if (!worst_offset)
                return true;

            int8_t dx = 0, dy = 0;

            if (x < 0)
                dx = -x;
            else if (x >= COLS)
                dx = x - (COLS - 1);
            if (y < 0)
                dy = -y;
            else if (y >= ROWS)
                dy = y - (ROWS - 1);

            int8_t dist = (dx > dy) ? dx : dy;

            if (!out_of_bounds || dist > worst_dist) {
                worst_dist = dist;
                *worst_offset = offset;
                if (dx >= dy)
                    worst_offset->y = 0;
                else
                    worst_offset->x = 0;
            }
            hit = true;
            out_of_bounds = true;
        } else if (!out_of_bounds && grid[y][x] != N) {
            if (!worst_offset)
                return true;

            hit = true;
            *worst_offset = offset;
        }
    }
    return hit;
}

// Returns whether the given shape is grounded or not.
static bool shape_grounded(const struct Shape *shape) {
    struct Shape down = *shape;
    down.pos.y++;
    return shape_collides(&down, NULL);
}

// Executes a geometric rotation of the given shape in the specified direction.
// If the rotated orientation causes a collision, a recovery offset
// (Wall/Floor Kick) is attempted using the worst colliding offset returned by
// `shape_collides` fn.
// If the shape remains in a collision state even after applying this offset,
// the entire rotation is cancelled.
static bool shape_rotate(struct Shape *shape, enum RotationDir dir) {
    if (shape->type == O)
        return true;

    struct Shape rotated = *shape;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        int8_t x = rotated.offsets[i].x;
        int8_t y = rotated.offsets[i].y;
        switch (dir) {
        case CLOCKWISE:
            rotated.offsets[i].x = -y;
            rotated.offsets[i].y = x;
            break;
        case COUNTER_CLOCKWISE:
            rotated.offsets[i].x = y;
            rotated.offsets[i].y = -x;
            break;
        }
    }

    struct Coord worst_offset = {0};
    if (!shape_collides(&rotated, &worst_offset)) {
        *shape = rotated;
        return true;
    }

    rotated.pos.x -= worst_offset.x;
    rotated.pos.y -= worst_offset.y;
    if (!shape_collides(&rotated, &worst_offset)) {
        *shape = rotated;
        return true;
    }

    return false;
}

// Move the given shape in the given direction obeying both the given timer and
// delay.
// In case if the timer is `TIMER_INACTIVE` then make the move right
// away.
// Move only happens if there are no collisions. Also return whether the
// move happened or not.
static bool shape_move(struct Shape *shape, enum MovementDir dir,
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

    if (!shape_collides(&next, NULL)) {
        if (last_move_timer == TIMER_INACTIVE || last_move_timer >= delay) {
            *shape = next;
            return true;
        }
    }
    return false;
}

// Process the buffered input.
static void input_buffer_process(struct UpdateContext *ctxt) {
    // Rotation.
    if (input_buffer.rotate_right_pressed) {
        if (curr_shape_rotate(CLOCKWISE))
            ctxt->moves++;
    } else if (input_buffer.rotate_left_pressed) {
        if (curr_shape_rotate(COUNTER_CLOCKWISE))
            ctxt->moves++;
    }

    // Shift.
    if (input_buffer.shift_left_pressed) {
        if (curr_shape_shift(LEFT, false))
            ctxt->moves++;
    } else if (input_buffer.shift_left_held) {
        if (curr_shape_shift(LEFT, true))
            ctxt->moves++;
    } else if (input_buffer.shift_right_pressed) {
        if (curr_shape_shift(RIGHT, false))
            ctxt->moves++;
    } else if (input_buffer.shift_right_held) {
        if (curr_shape_shift(RIGHT, true))
            ctxt->moves++;
    }

    ctxt->soft_drop = input_buffer.soft_drop_held;
    ctxt->hard_drop = input_buffer.hard_drop_pressed;

    // Reset everything.
    input_buffer.rotate_right_pressed = false;
    input_buffer.rotate_left_pressed = false;
    input_buffer.shift_left_pressed = false;
    input_buffer.shift_right_pressed = false;
    input_buffer.shift_left_held = false;
    input_buffer.shift_right_held = false;
    input_buffer.hard_drop_pressed = false;
    input_buffer.soft_drop_held = false;
}

// Rotate the current shape and return true if the rotation was committed.
static bool curr_shape_rotate(enum RotationDir dir) {
    if (shape_rotate(&curr_shape, dir)) {
        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_ROTATE,
                                         .shape = curr_shape});
        return true;
    }
    return false;
}

// Shift the current shape in the given direction and return true if the move
// was committed.
// `repeat` is set to true when shift is to be auto-repeated.
static bool curr_shape_shift(enum MovementDir dir, bool repeat) {
    static bool already_repeating = false;

    if (dir != LEFT && dir != RIGHT)
        return false;

    double delay_time, timer;

    if (repeat) {
        if (already_repeating) {
            delay_time = SHIFT_DELAY_REPEAT;
        } else {
            delay_time = SHIFT_DELAY_INITIAL;
        }
        timer = curr_shape_shift_timer;
    } else {
        already_repeating = false;
        delay_time = SHIFT_DELAY_REPEAT;
        timer = TIMER_INACTIVE;
    }

    if (shape_move(&curr_shape, dir, timer, delay_time)) {
        curr_shape_shift_timer = TIMER_START;
        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SHIFT,
                                         .shape = curr_shape});
        if (delay_time == SHIFT_DELAY_INITIAL)
            already_repeating = true;
        return true;
    }

    return false;
}

// In case there are new moves this frame then update the engine state
// accordingly.
static void handle_new_move(struct UpdateContext *ctxt) {
    if (ctxt->moves == 0)
        return;

    if (engine_state == STATE_DROPPING && !curr_shape_landed) {
        return;
    }

    // If the control flow reaches here then the engine_state is either
    // `STATE_LOCKING` or it is `STATE_DROPPING`` and `curr_shape_landed` is
    // true. In which case count the moves and if the move budget of 15 is
    // exhausted then do a forced hard drop.
    if (curr_shape_land_moves < MOVES_BEFORE_LOCK - 1) {
        engine_state_timer = TIMER_START;
        curr_shape_land_moves += ctxt->moves;
        if (engine_state == STATE_LOCKING)
            event_push((struct CTetrisEvent){
                .type = CTETRIS_EVENT_LOCK_RESET,
            });

    } else {
        ctxt->hard_drop = true;
    }
}

// Returns the active drop delay.
// Soft drop is considered.
// The drop delay is computed from a formula that depends on the current level.
// The drop delay is capped at soft drop delay.
static double get_drop_delay(bool soft_drop) {
    if (soft_drop)
        return SOFT_DROP_DELAY;
    double delay = pow(0.8 - ((level - 1) * 0.007), level - 1);
    return delay < SOFT_DROP_DELAY ? SOFT_DROP_DELAY : delay;
}

// In case the shape is grounded this frame then update the engine state
// accordingly.
static bool handle_grounded_shape(bool hard_drop) {
    if (!shape_grounded(&curr_shape) && !hard_drop)
        return false;

    bool timer_expired = (engine_state == STATE_LOCKING &&
                          engine_state_timer >= SHAPE_LOCK_DELAY);

    if (!hard_drop && !timer_expired) {
        if (engine_state != STATE_LOCKING) {
            engine_state = STATE_LOCKING;
            engine_state_timer = TIMER_START;
            curr_shape_landed = true;
            event_push((struct CTetrisEvent){
                .type = CTETRIS_EVENT_LOCK_START,
            });
        }
        return true;
    }

    if (hard_drop) {
        struct Shape proj_shape = ctetris_shape_proj_get();

        uint8_t rows_dropped = proj_shape.pos.y - curr_shape.pos.y;
        score += rows_dropped * 2;
        curr_shape = proj_shape;

        if (engine_state == STATE_LOCKING) {
            event_push((struct CTetrisEvent){
                .type = CTETRIS_EVENT_LOCK_CANCEL,
            });
        }

        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_HARD_DROP,
                                         .shape = curr_shape,
                                         .score = score});
    }

    grid_write_shape(&curr_shape);

    event_push((struct CTetrisEvent){
        .type = CTETRIS_EVENT_LOCK_DONE,
    });

    grid_clear_lines();
    endgame_or_reset();

    return true;
}

// In case the shape is airborne this frame then update the engine state
// accordingly.
static void handle_airborne_shape(bool soft_drop) {
    // If the shape is airborne but the engine state was previously in
    // `STATE_LOCKING` then put it in `STATE_DROPPING`.
    if (engine_state == STATE_LOCKING) {
        engine_state = STATE_DROPPING;
        engine_state_timer = TIMER_START;
        event_push((struct CTetrisEvent){
            .type = CTETRIS_EVENT_LOCK_CANCEL,
        });
    }

    struct Shape next_possible_shape = curr_shape;
    double drop_delay = get_drop_delay(soft_drop);
    if (shape_move(&next_possible_shape, DOWN, engine_state_timer,
                   drop_delay)) {

        curr_shape = next_possible_shape;
        engine_state_timer = TIMER_START;

        if (soft_drop) {
            score++;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SOFT_DROP,
                                             .shape = curr_shape,
                                             .score = score});
        } else {
            event_push((struct CTetrisEvent){
                .type = CTETRIS_EVENT_DROP,
                .shape = curr_shape,
            });
        }
    }

    // If the shape now occupies a row lower than what was previously recorded
    // then:
    // - Record the new `curr_shape_y_max`.
    // - reset both `curr_shape_land_moves` and `curr_shape_landed`.
    uint8_t current_y_max = shape_y_max_get(&curr_shape);
    if (current_y_max > curr_shape_y_max) {
        curr_shape_y_max = current_y_max;
        curr_shape_land_moves = 0;
        curr_shape_landed = false;
    }
}

// Write a shape on to the grid.
static void grid_write_shape(struct Shape *shape) {
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        uint8_t x = shape->pos.x + shape->offsets[i].x;
        uint8_t y = shape->pos.y + shape->offsets[i].y;
        grid[y][x] = shape->type;
    }
}

// Clear completed lines and settle the grid.
static void grid_clear_lines(void) {
    struct CTetrisEvent clear_ev = {0};
    clear_ev.type = CTETRIS_EVENT_LINE_CLEAR;

    uint8_t write = ROWS - 1;
    for (int8_t read = ROWS - 1; read >= 0; read--) {
        bool is_full = true;
        for (uint8_t c = 0; c < COLS; c++) {
            if (grid[read][c] == N) {
                is_full = false;
                break;
            }
        }

        if (is_full) {
            clear_ev.lines_cleared_indices[clear_ev.lines_cleared_count++] =
                (uint8_t)read;
            continue;
        }

        if (write != read) {
            memcpy(grid[write], grid[read], sizeof(grid[read]));
        }
        write--;
    }

    // Clear empty rows at the top after settling the grid.
    for (int8_t r = write; r >= 0; r--) {
        for (uint8_t c = 0; c < COLS; c++) {
            grid[r][c] = N;
        }
    }

    scoring_score(&clear_ev);

    if (clear_ev.lines == 0)
        return;

    event_push(clear_ev);
}

// Clear the grid.
static void grid_clear(void) {
    for (uint8_t r = 0; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS; c++) {
            grid[r][c] = N;
        }
    }
}

// Helper to return line clear bonus based on line count.
static uint16_t scoring_get_bonus(uint8_t val) {
    switch (val) {
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

// Adjust scoring based on number of lines cleared.
static void scoring_score(struct CTetrisEvent *ev) {
    if (ev->lines_cleared_count) {
        uint16_t bonus = scoring_get_bonus(ev->lines_cleared_count);
        score += bonus * level;
        score += CALC_COMBO_POINTS(level, combo);
        lines += ev->lines_cleared_count;
        level = (lines / 10) + 1;
        ev->score = score;
        ev->level = level;
        ev->lines = lines;
        ev->combo = combo;
        combo++;
    } else if (combo > 0) {
        combo = 0;
    }
}

static void endgame_or_reset(void) {
    // Get a new shape.
    curr_shape = shape_bag_next_get();

    struct CTetrisEvent ev = {.type = CTETRIS_EVENT_NEW_SHAPE,
                              .shape = curr_shape,
                              .engine_inactive = false};

    // If the new shape collides, then the engine goes inactive.
    if (shape_collides(&curr_shape, NULL)) {
        engine_state = STATE_INACTIVE;
        engine_state_timer = TIMER_INACTIVE;
        ev.engine_inactive = true;
    } else {
        // Else reset engine state for the next shape.
        engine_state = STATE_DROPPING;
        engine_state_timer = TIMER_START;
        curr_shape_land_moves = 0;
        curr_shape_y_max = shape_y_max_get(&curr_shape);
        curr_shape_shift_timer = TIMER_INACTIVE;
        curr_shape_landed = false;
    }

    event_push(ev);
}

/* [ END ] */
