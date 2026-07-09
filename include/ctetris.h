/* [ CTETRIS_H ] */

#ifndef CTETRIS_H
#define CTETRIS_H

/* [ ABOUT ] */

/**
 *
 * CTETRIS ENGINE
 *
 * ABOUT:
 *
 * This is a simple and minimal Tetris engine implementing only the bare minimum
 * of what I think constitutes a clean, lightweight, and fun Tetris game.
 *
 * GAMEPLAY:
 *
 * - The grid is 20×10, shapes are spawned at the top of the grid and drops down
 *   at an interval given by the formula:
 *   pow(0.8 - ((level - 1) * 0.007), level - 1)
 * - The drop interval cannot become shorter than the soft-drop interval
 *   (0.05s).
 *
 * Input mechanics:
 * - Shift (left/right): Individual moves or held input with delay.
 * - Rotate (CW/CCW): Geometric rotation with wall/floor kick collision.
 * - Soft drop: Accelerated fall (sets the drop interval to `SOFT_DROP_DELAY`
 *   when active).
 * - Hard drop: Instant teleportation to shadow position, locks immediately.
 *
 * Landing and locking:
 * - After landing (when not hard dropped), a lock timer of 0.5s runs. When it
 *   expires, the shape auto-locks.
 * - The lock timer is reset by further shift or rotate moves.
 * - If a move after landing puts the shape airborne, lock timer will be
 *   cancelled and further moves will reset the drop timer, pausing the downward
 *   drop mechanism.
 * - Once landed, all moves, whether the shape is grounded or airborne, are
 *   counted against a move budget of 15. When exhausted, a forced hard
 *   drop occurs.
 * - The move count can be reset and the drop mechanism restored to its default
 *   behavior, by letting the shape fall to a row it has never reached before.
 *
 * Line clears:
 * - Completed rows are cleared; rows above compact downward.
 *
 * SCORING AND LEVELING:
 *
 * Score is earned on:
 * - Soft drop (1 point per row dropped)
 * - Hard drop (2 points per row dropped)
 * - Line clears (line clear bonus * level)
 *   The baseline line clear bonus is given by:
 *   - 1 line:  100
 *   - 2 lines: 300
 *   - 3 lines: 500
 *   - 4 lines: 800
 *   - Maintaining a combo / line-clear chain; Score gain for combo is given by
 *     the formula:
 *       level * COMBO_BONUS * combo.
 *   - Level is dependent on the lines cleared and is given by the formula:
 *     (lines / 10) + 1
 *
 * BASIC USAGE:
 *
 * 1. Call ctetris_init() once to set up the game state.
 * 2. Each frame:
 *    - Call ctetris_input_push() to push input to the engine.
 *    - Call ctetris_update() to process the input and step the engine
 *      forward.
 *    - Poll ctetris_event_pop() in a loop until `CTETRIS_EVENT_NONE` to
 *      retrieve events happened in that update tick.
 *    - Render based on events.
 * 3. On game-over, call ctetris_init() again to restart.
 *
 */

/* [ INCLUDES ] */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* [ DEFINES ] */

// Tetris grid dimensions.
#define ROWS 20
#define COLS 10

// Number of offsets used per shape to define them.
// All shapes are made of 4 blocks, meaning 4 offsets.
#define OFFSETS_COUNT 4

#define SOFT_DROP_DELAY 0.05f // Drop delay (seconds).

// Delay before shift auto-repeat kicks in (seconds).
#define SHIFT_DELAY_INITIAL 0.2f
// Delay for shift auto-repeat (seconds).
#define SHIFT_DELAY_REPEAT 0.1f

// Number of moves allowed after a shape has landed before it is automatically
// hard dropped and locked.
#define MOVES_BEFORE_LOCK 15

#define SHAPE_LOCK_DELAY 0.5f // Lock timer duration (seconds).

// Score bonus multiplier for maintaining a consecutive line clear chain.
#define COMBO_BONUS 50
// Combo reward formula macro.
#define CALC_COMBO_POINTS(level, combo) ((level) * COMBO_BONUS * (combo))

/* [ STRUCTS AND ENUMS ] */

// Coord struct is used to represent shape offsets and grid coordinates.
struct Coord {
    int8_t x;
    int8_t y;
};

// Enum representing all 7 standard shapes.
enum ShapeType { O, L, J, I, T, S, Z, N };
// N serves as both the value for "No Shape" and the count of valid
// shape types (7).

// This is the cTetris shape struct.
// A shape is essentially a bunch of offsets set relative to a central position.
// The offsets are defined such that the shape's pivot sits at (0, 0).
struct Shape {
    enum ShapeType type;
    struct Coord
        offsets[OFFSETS_COUNT]; // Each shape requires exactly 4 offsets.
    struct Coord pos;           // Position of the shape within the grid.
};

// This struct represents the input state for a single frame.
// This is how the renderer communicates input events to the engine.
struct InputState {
    bool rotate_cw_pressed;
    bool rotate_ccw_pressed;

    bool shift_left_pressed;
    bool shift_left_held;
    bool shift_right_pressed;
    bool shift_right_held;

    bool soft_drop_held;
    bool hard_drop_pressed;
};

// CTetrisEventType lists all the event types the engine can signal
// back out to the renderer.
// The main use case for this event architecture is to enable clean
// renderer-side animations, and sound effects.
enum CTetrisEventType {
    CTETRIS_EVENT_NONE,
    CTETRIS_EVENT_NEW_GAME,

    CTETRIS_EVENT_ACTIVE_SHAPE_UPDATE,
    CTETRIS_EVENT_SHADOW_SHAPE_UPDATE,
    CTETRIS_EVENT_NEXT_SHAPE_UPDATE,

    CTETRIS_EVENT_HARD_DROP,
    CTETRIS_EVENT_SOFT_DROP,
    CTETRIS_EVENT_ROTATE,
    CTETRIS_EVENT_SHIFT,

    CTETRIS_EVENT_LOCK_START,
    CTETRIS_EVENT_LOCK_RESET,
    CTETRIS_EVENT_LOCK_CANCEL,
    CTETRIS_EVENT_LOCK_DONE,

    CTETRIS_EVENT_LINE_CLEAR,

    CTETRIS_EVENT_GAME_OVER
};

struct CTetrisStats {
    uint32_t score, lines, level, combo;
};

// This struct represents an engine event.
// This is how the engine communicates with the renderer.
struct CTetrisEvent {
    enum CTetrisEventType type;
    union {
        struct Shape shape; // Used by CTETRIS_EVENT_*_SHAPE_UPDATE.
        // CTETRIS_EVENT_(SOFT_DROP, HARD_DROP, LINE_CLEAR).
        struct {
            // SOFT_DROP/HARD_DROP: updates score only.
            // LINE_CLEAR: updates score, lines, level and combo.    struct
            CTetrisStats stats;
            // CTETRIS_EVENT_LINE_CLEAR.
            uint8_t lines_indices[4]; // At max only 4 lines can be cleared.
            uint8_t lines_count;      // cleared lines count.
        } action_ev;

    } data;
};

/* [ FN DCL ] */

/* Sets up the internal Tetris grid and initializes game state variables.
 * This function completely resets engine state and starts a new game.
 * It must be called at least once before calling any other ctetris_*
 * APIs to prevent acting on uninitialized garbage data.
 */
void ctetris_init(void);

/* Pushes the current frame's user inputs to the engine.
 * This should be called with valid input every frame before calling
 * that frame's ctetris_update.
 */
void ctetris_input_push(struct InputState input_state);

/* Steps the engine simulation state forward and populates internal event queue.
 * 'delta' represents the elapsed time since last `ctetris_update` in seconds.
 * Internal timers are incremented with this value. Passing 0.0 or not calling
 * this fn at all freezes the simulation, useful for pausing the engine/game.
 */
void ctetris_update(double delta);

/* Dequeues and returns one `struct CTetrisEvent` per call from the internal
 * event queue. Events are returned sequentially in FIFO order. If the queue is
 * empty then an event of type CTETRIS_EVENT_NONE is returned.
 *
 * NOTE: The Engine can push multiple events in one `ctetris_update` tick.
 * Read ctetris.c to understand engine's event mechanism and which all events
 * CAN be batched together when pushed.
 */
struct CTetrisEvent ctetris_event_pop(void);

#endif

/* [ END ] */
