/*
 * To understand how the engine works, start with the enums, structs, global
 * variables and then go straight to the `ctetris_update` fn which is the core
 * engine fn where everything happens.
 */

/* [ INCLUDES ] */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ctetris.h"

/* [ DEFINES ] */

#define SOFT_DROP_DELAY 0.05f // Drop delay (seconds).

#define SHIFT_DELAY_INITIAL 0.2f // Delay before auto-repeat kicks in (seconds).
#define SHIFT_DELAY_REPEAT 0.1f  // Delay for shift auto-repeat (seconds).

// Number of moves allowed after a shape has landed before it is automatically
// hard dropped and locked.
#define MOVES_BEFORE_LOCK 15

#define SHAPE_LOCK_DELAY 0.5f // Lock timer duration (seconds).

// Score bonus multiplier for maintaining a consecutive line clear chain.
#define COMBO_BONUS 50

// All non `TIMER_INACTIVE` timers will be incremented.
#define TIMER_INACTIVE (-1.0) // Timer won't be incremented.
#define TIMER_START (0.0)     // resets the timer.

#define EVENT_QUEUE_CAP 32

/* [ ENUMS AND STRUCTS ] */

enum EngineState { STATE_DROPPING, STATE_LOCKING };
enum MovementDir { LEFT, RIGHT, DOWN };
enum RotationDir { CLOCKWISE, COUNTER_CLOCKWISE };

struct FrameContext {
    bool hard_drop;
    bool soft_drop;
    int moves;
};

struct CollisionStatus {
    bool hit;
    struct Coord offset;
};

/* [ VARIABLES ] */

// standard Tetris shape coordinate definitions.
// (0, 0) is the pivot.
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
static int shape_bag_next_i = 0;

// Game state variables

// If the shape is airborne then the `engine_state` will be `STATE_DROPPING`.
// Otherwise `STATE_LOCKING`.
static enum EngineState engine_state;
static double engine_state_timer; // Timer for the current `engine_state`.

// Tetris grid is made up of cells of the type `enum ShapeType`.
static enum ShapeType grid[ROWS][COLS];

static struct Shape curr_shape, shadow_shape;
static double curr_shape_shift_timer; // Timer for auto-repeat shift.

// `y_max` represents the lowest row the current shape has reached.
static int y_max;

// Has the shape landed?
static bool has_landed;

// `lock_move_count` represents number of moves since the shape has landed.
static int lock_move_count;

static int score, combo, level, lines;

// Input buffer on which the ctetris_update will act on.
// Set on ctetris_input_push.
static struct InputState input_buffer;

// Event queue.
static struct CTetrisEvent event_queue[EVENT_QUEUE_CAP];
static int eq_head;
static int eq_tail;

/* [ FN DCL ] */

// Event queue.
static void event_push(struct CTetrisEvent ev);

// Randomization.
static void shape_bag_shuffle(void);
static struct Shape shape_bag_get_random(void);

// Shape utilities.
static int get_shape_y_max(const struct Shape *shape);
static struct CollisionStatus shape_collides(const struct Shape *shape);
static bool is_shape_grounded(const struct Shape *shape);
static struct Shape get_shadow_shape(struct Shape *shape);
static bool rotate_shape(struct Shape *shape, enum RotationDir dir);
static bool move_shape(struct Shape *shape, enum MovementDir dir,
                       double last_move_timer, double delay);

// Core.
static void process_input_buffer(struct FrameContext *ctxt);

static bool curr_shape_rotate(enum RotationDir dir);
static bool curr_shape_shift(enum MovementDir dir, bool delay);
static void handle_new_move(struct FrameContext *ctxt);

static double get_drop_delay(bool soft_drop);
static bool handle_grounded_shape(bool hard_drop);
static void handle_airborne_shape(bool soft_drop);

static void write_shape_to_grid(struct Shape *shape);
static int clear_lines(void);
static void clear_grid(void);

static int get_line_bonus(uint8_t val);
static void scoring(int lines_cleared);
static void endgame_or_reset(void);

/* [ PUBLIC API FN DEF ] */

void ctetris_init(void) {
    srand((unsigned)time(NULL));

    // Pretty much just setting core variables to valid starting values for a
    // new game.
    eq_head = eq_tail = 0;

    clear_grid();
    shape_bag_shuffle();

    curr_shape = shape_bag_get_random();
    shadow_shape = get_shadow_shape(&curr_shape);
    y_max = get_shape_y_max(&curr_shape);

    engine_state = STATE_DROPPING;
    engine_state_timer = TIMER_START;
    lock_move_count = 0;
    has_landed = false;

    score = combo = lines = 0;
    level = 1;

    curr_shape_shift_timer = TIMER_INACTIVE;

    memset(&input_buffer, 0, sizeof(struct InputState));

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_NEW_GAME});
    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_ACTIVE_SHAPE_UPDATE,
                                     .data.shape = curr_shape});
    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SHADOW_SHAPE_UPDATE,
                                     .data.shape = shadow_shape});
    event_push(
        (struct CTetrisEvent){.type = CTETRIS_EVENT_NEXT_SHAPE_UPDATE,
                              .data.shape = *shape_bag[shape_bag_next_i]});
}

void ctetris_input_push(struct InputState input_state) {
    input_buffer = input_state;
}

void ctetris_update(double delta) {
    // Increment active timers.
    if (engine_state_timer != TIMER_INACTIVE)
        engine_state_timer += delta;
    if (curr_shape_shift_timer != TIMER_INACTIVE)
        curr_shape_shift_timer += delta;

    struct FrameContext ctxt = {0};

    // Process buffered input.
    process_input_buffer(&ctxt);

    // In case there are new moves.
    handle_new_move(&ctxt);

    // In case the shape is grounded.
    if (!handle_grounded_shape(ctxt.hard_drop))
        // If it is airborne.
        handle_airborne_shape(ctxt.soft_drop);
}

struct CTetrisEvent ctetris_event_pop(void) {
    // If the queue is empty, return `CTETRIS_EVENT_NONE`.
    if (eq_head == eq_tail)
        return (struct CTetrisEvent){.type = CTETRIS_EVENT_NONE};
    struct CTetrisEvent ev = event_queue[eq_head];
    eq_head = (eq_head + 1) % EVENT_QUEUE_CAP;
    return ev;
}

/* [ INTERNAL CORE FN DEF ] */

// Push an event to the event queue.
static void event_push(struct CTetrisEvent ev) {
    int next = (eq_tail + 1) % EVENT_QUEUE_CAP;
    if (next == eq_head)
        return; // When the queue overflows, just don't push.
    event_queue[eq_tail] = ev;
    eq_tail = next;
}

// Get the lowest row on the grid the given shape is occupying.
static int get_shape_y_max(const struct Shape *shape) {
    int max_y = 0;
    for (int i = 1; i < OFFSETS_COUNT; i++) {
        int cy = (int)(shape->pos.y + shape->offsets[i].y);
        if (cy > max_y)
            max_y = cy;
    }
    return max_y;
}

// Shuffle the shape bag.
static void shape_bag_shuffle(void) {
    for (int i = 6; i > 0; i--) {
        int j = rand() % (i + 1);
        struct Shape *tmp = shape_bag[i];
        shape_bag[i] = shape_bag[j];
        shape_bag[j] = tmp;
    }
    shape_bag_next_i = 0;
}

// Get a random shape from the shape bag.
static struct Shape shape_bag_get_random(void) {
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
    int y = 0;
    for (int i = 0; i < OFFSETS_COUNT; i++)
        y = (shape.offsets[i].y < y) ? shape.offsets[i].y : y;

    shape.pos = (struct Coord){.y = -y, .x = (int)(COLS / 2)};
    return shape;
}

// Takes in a shape and returns if the shape is in active collision or not.
// Being out of bounds is also considered as collision and is given more
// priority than in bounds collision.
// In case if there is a collision then this fn returns a `struct
// CollisionStatus` including a worst colliding offset.
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

            int dist = (dx > dy) ? dx : dy;
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

        } else if (!out_of_bounds && grid[y][x] != N) {
            status.hit = true;
            status.offset = offset;
        }
    }
    return status;
}

// Returns whether the given shape is grounded or not.
static bool is_shape_grounded(const struct Shape *shape) {
    struct Shape down = *shape;
    down.pos.y++;
    return shape_collides(&down).hit;
}

// Returns a shadow shape for the given shape.
static struct Shape get_shadow_shape(struct Shape *shape) {
    int max_drop = ROWS;

    struct Shape shadow_shape = *shape;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = shape->pos.x + shape->offsets[i].x;
        int y = shape->pos.y + shape->offsets[i].y;

        int dist = 0;
        for (int check_y = y + 1; check_y < ROWS; check_y++) {
            if (grid[check_y][x] != N)
                break;
            dist++;
        }

        if (dist < max_drop) {
            max_drop = dist;
        }
    }

    shadow_shape.pos.y += max_drop;
    return shadow_shape;
}

// Executes a geometric rotation of the given shape in the specified direction.
//
// If the rotated orientation causes an immediate collision, a recovery offset
// (Wall/Floor Kick) is attempted using the worst colliding offset returned by
// `shape_collides`.
//
// If the shape remains in a collision state even after applying this offset,
// the entire rotation is cancelled.
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
        case COUNTER_CLOCKWISE:
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

// Move the given shape in the given direction obeying both the given timer and
// delay. in case if the timer is `TIMER_INACTIVE` then make the move right
// away. Move only happens if there are no collisions. Also return whether the
// move happened or not.
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

// Process the buffered input.
static void process_input_buffer(struct FrameContext *ctxt) {
    // Rotation.
    if (input_buffer.rotate_cw_pressed) {
        if (curr_shape_rotate(CLOCKWISE))
            ctxt->moves++;
    } else if (input_buffer.rotate_ccw_pressed) {
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
    input_buffer.rotate_cw_pressed = false;
    input_buffer.rotate_ccw_pressed = false;
    input_buffer.shift_left_pressed = false;
    input_buffer.shift_right_pressed = false;
    input_buffer.shift_left_held = false;
    input_buffer.shift_right_held = false;
    input_buffer.hard_drop_pressed = false;
    input_buffer.soft_drop_held = false;
}

// Rotate the current shape and return true if the rotation was committed.
static bool curr_shape_rotate(enum RotationDir dir) {
    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_ROTATE});

    return rotate_shape(&curr_shape, dir);
}

// shift the current shape in the given direction and return true if the move
// was committed. `delay` is set to true when shift is to be auto-repeated.
static bool curr_shape_shift(enum MovementDir dir, bool delay) {
    static bool repeat = false;

    if (dir != LEFT && dir != RIGHT)
        return false;

    double delay_time, timer;

    if (delay) {
        if (repeat) {
            delay_time = SHIFT_DELAY_REPEAT;
        } else {
            delay_time = SHIFT_DELAY_INITIAL;
        }
        timer = curr_shape_shift_timer;
    } else {
        repeat = false;
        delay_time = SHIFT_DELAY_REPEAT;
        timer = TIMER_INACTIVE;
    }

    if (move_shape(&curr_shape, dir, timer, delay_time)) {
        curr_shape_shift_timer = TIMER_START;
        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SHIFT});
        if (delay_time == SHIFT_DELAY_INITIAL)
            repeat = true;
        return true;
    }

    return false;
}

// In case there are new moves this frame then update the engine state
// accordingly.
static void handle_new_move(struct FrameContext *ctxt) {
    if (ctxt->moves == 0)
        return;

    // As the there was a move, set a new shadow shape.
    shadow_shape = get_shadow_shape(&curr_shape);

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_ACTIVE_SHAPE_UPDATE,
                                     .data.shape = curr_shape});

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SHADOW_SHAPE_UPDATE,
                                     .data.shape = shadow_shape});

    if (engine_state == STATE_DROPPING && !has_landed) {
        return;
    }

    // if the control flow reaches here then the engine_state is either
    // `STATE_LOCKING` or it is (`STATE_DROPPING`` and `has_landed` is true). In
    // which case count the moves and if the move budget of 15 is extinguished
    // then do a forced hard drop.
    if (lock_move_count < MOVES_BEFORE_LOCK - 1) {
        engine_state_timer = TIMER_START;
        lock_move_count += ctxt->moves;
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
    if (!is_shape_grounded(&curr_shape) && !hard_drop)
        return false;

    bool timer_expired = (engine_state == STATE_LOCKING &&
                          engine_state_timer >= SHAPE_LOCK_DELAY);

    if (!hard_drop && !timer_expired) {
        if (engine_state != STATE_LOCKING) {
            engine_state = STATE_LOCKING;
            engine_state_timer = TIMER_START;
            has_landed = true;
            event_push((struct CTetrisEvent){
                .type = CTETRIS_EVENT_LOCK_START,
            });
        }
        return true;
    }

    if (hard_drop) {
        struct Coord from = curr_shape.pos;
        int rows_dropped = shadow_shape.pos.y - curr_shape.pos.y;
        score += rows_dropped * 2;
        curr_shape.pos = shadow_shape.pos;

        event_push((struct CTetrisEvent){
            .type = CTETRIS_EVENT_HARD_DROP,
        });

        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SCORE_UPDATE,
                                         .data.score = score});

        event_push(
            (struct CTetrisEvent){.type = CTETRIS_EVENT_ACTIVE_SHAPE_UPDATE,
                                  .data.shape = curr_shape});

        event_push(
            (struct CTetrisEvent){.type = CTETRIS_EVENT_SHADOW_SHAPE_UPDATE,
                                  .data.shape = shadow_shape});
    }

    write_shape_to_grid(&curr_shape);

    event_push((struct CTetrisEvent){
        .type = CTETRIS_EVENT_LOCK_DONE,
    });

    scoring(clear_lines());
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

    // The core drop mechanism.
    struct Shape next_possible_shape = curr_shape;
    double drop_delay = get_drop_delay(soft_drop);
    if (move_shape(&next_possible_shape, DOWN, engine_state_timer,
                   drop_delay)) {
        if (soft_drop) {
            score++;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SCORE_UPDATE,
                                             .data.score = score});
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SOFT_DROP});
        }

        curr_shape = next_possible_shape;
        engine_state_timer = TIMER_START;

        event_push(
            (struct CTetrisEvent){.type = CTETRIS_EVENT_ACTIVE_SHAPE_UPDATE,
                                  .data.shape = curr_shape});

        event_push(
            (struct CTetrisEvent){.type = CTETRIS_EVENT_SHADOW_SHAPE_UPDATE,
                                  .data.shape = shadow_shape});
    }

    // If the shape now occupies a row lower than what was previously recorded
    // then:
    // - Record the new y_max.
    // - reset both `lock_move_count` and `has_landed`.
    int current_max_y = get_shape_y_max(&curr_shape);
    if (current_max_y > y_max) {
        y_max = current_max_y;
        lock_move_count = 0;
        has_landed = false;
    }
}

// Write a shape to the grid.
static void write_shape_to_grid(struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = shape->pos.x + shape->offsets[i].x;
        int y = shape->pos.y + shape->offsets[i].y;
        grid[y][x] = shape->type;
    }
}

static bool row_is_empty(int row) {
    for (int c = 0; c < COLS; c++) {
        if (grid[row][c] != N)
            return false;
    }

    return true;
}

// Clear completed lines and settle the grid.
// Returns the count of lines cleared.
static int clear_lines(void) {
    struct CTetrisEvent clear_ev = {
        .type = CTETRIS_EVENT_LINE_CLEAR,
        .data.line_ev.lines = 0,
    };

    struct CTetrisEvent move_ev = {
        .type = CTETRIS_EVENT_LINE_MOVE,
        .data.line_ev.lines = 0,
    };

    int write = ROWS - 1;

    for (int read = ROWS - 1; read >= 0; read--) {
        bool is_full = true;

        for (int c = 0; c < COLS; c++) {
            if (grid[read][c] == N) {
                is_full = false;
                break;
            }
        }

        if (is_full) {
            clear_ev.data.line_ev.info[clear_ev.data.line_ev.lines++] =
                (struct Coord){
                    .x = read,
                    .y = read,
                };

            continue;
        }

        if (write != read) {
            if (!row_is_empty(read)) {
                move_ev.data.line_ev.info[move_ev.data.line_ev.lines++] =
                    (struct Coord){
                        .x = read,
                        .y = write,
                    };
            }

            memcpy(grid[write], grid[read], sizeof(grid[read]));
        }

        write--;
    }

    if (clear_ev.data.line_ev.lines == 0)
        return 0;

    for (int r = write; r >= 0; r--) {
        for (int c = 0; c < COLS; c++) {
            grid[r][c] = N;
        }
    }

    event_push(clear_ev);

    if (move_ev.data.line_ev.lines > 0)
        event_push(move_ev);

    return clear_ev.data.line_ev.lines;
}

// Clear the grid.
static void clear_grid(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            grid[r][c] = N;
}

// Helper to return line clear bonus based on line count.
static int get_line_bonus(uint8_t val) {
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
static void scoring(int lines_cleared) {
    if (lines_cleared) {
        int bonus = get_line_bonus(lines_cleared);
        score += bonus * level;
        lines += lines_cleared;

        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_LINES_UPDATE,
                                         .data.lines = lines});

        int new_possible_level = (lines / 10) + 1;
        if (new_possible_level != level) {
            level = new_possible_level;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_LEVEL_UPDATE,
                                             .data.level = level});
        }
        if (combo > 0) {
            score += combo * COMBO_BONUS * level;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_COMBO_UPDATE,
                                             .data.combo = combo});
        }
        combo++;

        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SCORE_UPDATE,
                                         .data.score = score});
    } else {
        if (combo) {
            combo = 0;
            event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_COMBO_UPDATE,
                                             .data.combo = combo});
        }
    }
}

static void endgame_or_reset(void) {
    // Get a new shape and its shadow.
    curr_shape = shape_bag_get_random();
    shadow_shape = get_shadow_shape(&curr_shape);

    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_ACTIVE_SHAPE_UPDATE,
                                     .data.shape = curr_shape});
    event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_SHADOW_SHAPE_UPDATE,
                                     .data.shape = shadow_shape});
    event_push(
        (struct CTetrisEvent){.type = CTETRIS_EVENT_NEXT_SHAPE_UPDATE,
                              .data.shape = *shape_bag[shape_bag_next_i]});

    // Else reset engine state variables.
    engine_state = STATE_DROPPING;
    engine_state_timer = TIMER_START;
    lock_move_count = 0;
    y_max = get_shape_y_max(&curr_shape);
    curr_shape_shift_timer = TIMER_INACTIVE;
    has_landed = false;

    // If the new shape collides, then the game is over.
    if (shape_collides(&curr_shape).hit) {
        event_push((struct CTetrisEvent){.type = CTETRIS_EVENT_GAME_OVER});
        return;
    }
}

/* [ END ] */
