#ifndef CTETRIS_H
#define CTETRIS_H

/**
 * CTERTIS ENGINE
 *
 * ABOUT:
 *
 * This is a simple and minimal Tetris engine implementing only the bare minimum
 * of what i think constitutes a clean, lightweight, and fun Tetris game.
 *
 * GAMEPLAY:
 *
 * - The grid is 20×10, pieces are spawned at the top of the grid and falls down
 *   at an interval given by the formula:
 *   pow(0.8 - ((level - 1) * 0.007), level - 1)
 * - The drop interval is capped at the soft drop interval (0.05s).
 *
 * Input mechanics:
 * - Strafe (left/right): Individual moves or held input with delay.
 * - Rotate (CW/ACW): Geometric rotation with wall/floor kick collision.
 * - Soft drop: Accelerated fall (sets the drop interval to 0.05s when active).
 * - Hard drop: Instant teleportation to shadow position, locks immediately.
 *
 * Landing and locking:
 * - After landing, a lock timer of 0.5s runs. When it expires,
 *   the shape auto-locks.
 * - The lock timer is reset by further strafe or rotate moves.
 * - If a move after landing puts the shape airborne, lock timer will be
 *   cancelled and further moves will reset the drop timer, pausing the downward
 *   drop mechanism.
 * - Once landed, all moves whether the shape is grounded or airborne are
 *   counted against a move budget of 15. When exhausted, a forced hard
 *   drop occurs.
 * - The move count can be reset, and the drop mechanism set back to default
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
 *   - Maintaining a combo / line-clear chain (50 points per successive line
 *     clear without breaking the chain).
 *   - Level is dependant on the lines cleared and is given by the formula:
 *     (lines / 10) + 1
 *
 * BASIC USAGE:
 *
 * 1. Call ctetris_init() once to set up the game state.
 * 2. Each frame:
 *    - Call ctetris_input_push(state) to push input to the engine.
 *    - Call ctetris_update(delta_time) to process the input and step the engine
 *      forward.
 *    - Poll ctetris_event_get() in a loop till `CTETRIS_EVENT_NONE` to retrieve
 *      events happened in that update tick.
 *    - Render based on events.
 * 3. On game-over, call ctetris_init() again to restart.
 *
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Tetris grid dimensions.
#define ROWS 20
#define COLS 10

// Number of offsets used per shape to define them.
// All shapes are made of 4 blocks, meaning 4 offsets.
#define OFFSETS_COUNT 4

/* [ CORE STRUCTS AND ENUMS ] */

// Coord struct is used to represent shape offsets and coordinates.
struct Coord {
    int x;
    int y;
};

// Enum representing all 7 standard shapes.
enum ShapeType { O, L, J, I, T, S, Z, N };
// N represents both number of shapes and "No Shape".

// This is the cTetris shape struct.
// A shape is essentially a bunch of offsets set relative to a central position.
// The offsets are defined such that the shape pivot sits at (0, 0).
struct Shape {
    enum ShapeType type; // Shape type index.
    struct Coord
        offsets[OFFSETS_COUNT]; // Each shape requires exactly 4 offsets.
    struct Coord pos;           // Position of the shape within the grid.
};

// This struct represents the input state for a single frame.
// This is how the renderer communicates input events to the engine.
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
    CTETRIS_EVENT_STRAFE,

    CTETRIS_EVENT_SCORE_UPDATE,
    CTETRIS_EVENT_LINES_UPDATE,
    CTETRIS_EVENT_LEVEL_UPDATE,
    CTETRIS_EVENT_COMBO_UPDATE,

    CTETRIS_EVENT_LOCK_START,
    CTETRIS_EVENT_LOCK_RESET,
    CTETRIS_EVENT_LOCK_CANCEL,
    CTETRIS_EVENT_LOCK_DONE,

    CTETRIS_EVENT_LINE_CLEAR,
    CTETRIS_EVENT_LINE_MOVE,

    CTETRIS_EVENT_GAME_OVER
};

struct LineEvInfo {
    uint8_t from, to;
};

// This struct represents an engine event.
// This is how the engine communicated with the renderer.
struct CTetrisEvent {
    enum CTetrisEventType type;
    union {
        struct Shape shape;
        int score;
        int combo;
        int lines;
        int level;
        struct {
            struct LineEvInfo info[ROWS];
            int lines;
        } line_ev;

    } data;
};

/* [ PUBLIC API FN DCL ] */

/* Sets up the internal Tetris grid and initializes game state variables.
 * Call this function whenever a new game session starts. It must be executed
 * at least once before calling any other ctetris_* APIs to prevent acting on
 * uninitialized garbage data.
 */
void ctetris_init(void);

/* Pushes the current frame's user inputs to the engine.
 * This should be populated and executed every frame before running
 * ctetris_update.
 */
void ctetris_input_push(struct InputState input_state);

/* Steps the engine simulation state forward and populates internal event queue.
 * 'delta' represents the elapsed execution frame-time in seconds. Internal
 * timers are incremented with this value. Passing 0.0 or not calling this
 * fn at all freezes the simulation, usefull for pausing the engine/game.
 */
void ctetris_update(double delta);

/* Dequeues and returns one `struct CTetrisEvent` per call from the internal
 * event queue. Events are returned sequentially in FIFO order. If the queue is
 * empty then an event of type CTETRIS_EVENT_NONE is returned.
 *
 * NOTE: The Engine can push multiple events in one tick.
 */
struct CTetrisEvent ctetris_event_pop(void);

#endif
