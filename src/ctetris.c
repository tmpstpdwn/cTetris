#include "ctetris.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MOVES_BEFORE_LOCK 15
#define SHAPE_LOCK_DELAY 0.5f

#define FLOATING_DELAY 0.7f

#define DROP_DELAY 0.5f
#define SOFT_DROP_DELAY 0.05f
#define STRAFE_DELAY 0.2f

#define COMBO_BONUS 50
#define B2B_BONUS 1.5f

#define TIMER_INACTIVE (-1.0)
#define TIMER_START (0.0)

enum PieceState { STATE_DROPPING, STATE_LOCKING, STATE_FLOATING };

enum MovementDir { LEFT, RIGHT, DOWN };
enum RotationDir { CLOCKWISE, ANTI_CLOCKWISE };

struct CollisionStatus {
    bool hit;
    struct Coord offset;
};

struct Shape {
    enum ShapeType type;
    struct Coord offsets[OFFSETS_COUNT];
    struct Coord pos;
};

static struct Shape shape_o = {
    .type = O, .offsets = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};
static struct Shape shape_l = {
    .type = L, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {1, 1}}, .pos = {0, 0}};
static struct Shape shape_i = {
    .type = I, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {2, 0}}, .pos = {0, 0}};
static struct Shape shape_t = {
    .type = T, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {0, 1}}, .pos = {0, 0}};
static struct Shape shape_s = {
    .type = S, .offsets = {{-1, 1}, {0, 1}, {0, 0}, {1, 0}}, .pos = {0, 0}};
static struct Shape shape_j = {
    .type = J, .offsets = {{-1, 0}, {0, 0}, {1, 0}, {-1, 1}}, .pos = {0, 0}};
static struct Shape shape_z = {
    .type = Z, .offsets = {{-1, 0}, {0, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};

static struct Shape *shapes[SHAPE_COUNT] = {
    [N] = NULL,     [X] = NULL,     [O] = &shape_o,
    [L] = &shape_l, [J] = &shape_j, [I] = &shape_i,
    [T] = &shape_t, [S] = &shape_s, [Z] = &shape_z};

static int next_shape_index = 2; // Exclude N and X.

static enum ShapeType board[ROWS][COLS] = {0};

static struct Shape curr_shape, shadow_shape;

static enum PieceState curr_shape_state;
static double curr_shape_state_timer;
static int lock_move_count, y_max;

static int score, combo, level, lines;
static bool b2b;

static double curr_shape_strafe_timer;
static bool new_move = false;

static bool hard_drop = false;
static bool soft_drop = false;

static void clear_board(void) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            board[i][j] = N;
        }
    }
}

static int get_shape_y_max(const struct Shape *shape) {
    int max_y = 0;
    for (int i = 1; i < OFFSETS_COUNT; i++) {
        int current_y = (int)(shape->pos.y + shape->offsets[i].y);
        if (current_y > max_y) {
            max_y = current_y;
        }
    }
    return max_y;
}

static void shuffle_shapes(void) {
    for (int i = 6; i > 0; i--) {
        int j = GetRandomValue(0, i);
        struct Shape *tmp = shapes[2 + i];
        shapes[2 + i] = shapes[2 + j];
        shapes[2 + j] = tmp;
    }
}

static struct Shape get_random_shape(void) {
    static bool init = true;
    struct Shape shape;

    if (init) {
        init = false;
        shuffle_shapes();
        shape = *shapes[next_shape_index++];
    } else if (next_shape_index == SHAPE_COUNT - 1) {
        shape = *shapes[next_shape_index];
        shuffle_shapes();
        next_shape_index = 2;
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

        } else if (!out_of_bounds && board[y][x] != N) {
            status.hit = true;
            status.offset = offset;
        }
    }

    return status;
}

static struct Shape get_shadow_shape(struct Shape shape) {
    do {
        shape.pos.y++;
    } while (!shape_collides(&shape).hit);

    shape.pos.y--;
    shape.type = X;

    return shape;
}

void ctetris_init(void) {
    clear_board();

    curr_shape = get_random_shape();
    shadow_shape = get_shadow_shape(curr_shape);
    y_max = get_shape_y_max(&curr_shape);

    curr_shape_state = STATE_DROPPING;
    curr_shape_state_timer = TIMER_START;
    lock_move_count = 0;

    score = 0;
    combo = 0;
    lines = 0;
    level = 1;

    curr_shape_strafe_timer = TIMER_INACTIVE;
    b2b = false;
}

static bool rotate_shape(struct Shape *shape, enum RotationDir dir) {
    if (shape->type == O)
        return true;

    struct Shape rotated_shape = *shape;

    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = rotated_shape.offsets[i].x;
        int y = rotated_shape.offsets[i].y;
        switch (dir) {
        case CLOCKWISE:
            rotated_shape.offsets[i].x = -y;
            rotated_shape.offsets[i].y = x;
            break;
        case ANTI_CLOCKWISE:
            rotated_shape.offsets[i].x = y;
            rotated_shape.offsets[i].y = -x;
            break;
        }
    }

    struct CollisionStatus status = shape_collides(&rotated_shape);

    if (!status.hit) {
        *shape = rotated_shape;
        return true;
    }

    rotated_shape.pos.x -= status.offset.x;
    rotated_shape.pos.y -= status.offset.y;

    if (!shape_collides(&rotated_shape).hit) {
        *shape = rotated_shape;
        return true;
    }

    return false;
}

void ctetris_rotate_cw(void) {
    new_move = new_move || rotate_shape(&curr_shape, CLOCKWISE);
}

void ctetris_rotate_acw(void) {
    new_move = new_move || rotate_shape(&curr_shape, ANTI_CLOCKWISE);
}

static bool move_shape(struct Shape *shape, enum MovementDir dir,
                       double last_move_timer, double delay) {
    struct Shape next_shape = *shape;

    switch (dir) {
    case DOWN:
        next_shape.pos.y++;
        break;
    case LEFT:
        next_shape.pos.x--;
        break;
    case RIGHT:
        next_shape.pos.x++;
        break;
    default:
        break;
    };

    struct CollisionStatus status = shape_collides(&next_shape);

    if (!status.hit) {
        if (last_move_timer == TIMER_INACTIVE || last_move_timer >= delay) {
            *shape = next_shape;
            return true;
        }
    }

    return false;
}

static bool strafe(enum MovementDir dir, bool delay) {
    static enum MovementDir last_strafe = -1;

    if (dir != LEFT && dir != RIGHT)
        return false;

    if (last_strafe != dir || !delay)
        curr_shape_strafe_timer = TIMER_INACTIVE;

    if (move_shape(&curr_shape, dir, curr_shape_strafe_timer, STRAFE_DELAY)) {
        new_move = true;
        curr_shape_strafe_timer = TIMER_START;
        last_strafe = dir;
        return true;
    }
    return false;
}

void ctetris_strafe_left(bool delay) { strafe(LEFT, delay); }
void ctetris_strafe_right(bool delay) { strafe(RIGHT, delay); }

static void write_to_board(struct Shape *shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        struct Coord offset = shape->offsets[i];

        int x = shape->pos.x + offset.x;
        int y = shape->pos.y + offset.y;

        board[y][x] = shape->type;
    }
}

static int clear_lines(void) {
    int write = ROWS - 1;
    int rows_cleared = 0;
    for (int read = ROWS - 1; read >= 0; read--) {
        int occupied = 0;
        for (int col = 0; col < COLS; col++)
            if (board[read][col] != N)
                occupied++;
        if (occupied < COLS) {
            if (write != read)
                memcpy(board[write], board[read],
                       COLS * sizeof(enum ShapeType));
            write--;
        } else {
            rows_cleared++;
        }
    }
    for (int row = write; row >= 0; row--)
        memset(board[row], N, COLS * sizeof(enum ShapeType));
    return rows_cleared;
}

static int get_line_bonus(uint8_t lines) {
    switch (lines) {
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

const struct Shape *ctetris_get_curr_shape(void) { return &curr_shape; }
const struct Shape *ctetris_get_curr_shadow_shape(void) {
    return &shadow_shape;
}

const struct Shape *ctetris_get_next_shape(void) {
    return shapes[next_shape_index];
}

struct Coord ctetris_get_offset(const struct Shape *shape, uint8_t offsets_n) {
    if (offsets_n < 0 && offsets_n >= OFFSETS_COUNT)
        return (struct Coord){0, 0};
    return shape->offsets[offsets_n];
}

enum ShapeType ctetris_get_board_cell(uint8_t row, uint8_t col) {
    if (row >= 0 && row < ROWS && col >= 0 && col < COLS)
        return board[row][col];
    return N;
}

struct Coord ctetris_get_pos(const struct Shape *shape) { return shape->pos; }

enum ShapeType ctetris_get_type(const struct Shape *shape) {
    return shape->type;
}

static bool is_shape_grounded(void) {
    struct Shape down_check = curr_shape;
    down_check.pos.y++;
    return shape_collides(&down_check).hit;
}

static void handle_move_limit(void) {
    shadow_shape = get_shadow_shape(curr_shape);
    if (curr_shape_state != STATE_DROPPING) {
        if (lock_move_count < MOVES_BEFORE_LOCK - 1) {
            if (curr_shape_state == STATE_LOCKING) {
                curr_shape_state_timer = TIMER_START;
            }
            lock_move_count++;
        } else if (is_shape_grounded()) {
            hard_drop = true;
        } else {
            curr_shape_state = STATE_DROPPING;
            curr_shape_state_timer = TIMER_INACTIVE;
        }
    }
}

void ctetris_hard_drop(void) { hard_drop = true; }

void ctetris_soft_drop(void) { soft_drop = true; }

static void clear_lines_and_score(void) {
    int lines_cleared = clear_lines();

    if (lines_cleared) {
        combo++;

        int line_bonus = get_line_bonus(lines_cleared);

        if (lines_cleared == 4) {
            if (b2b)
                line_bonus = (line_bonus * B2B_BONUS);
            b2b = true;
        } else {
            b2b = false;
        }

        score += line_bonus * level;
        score += combo * COMBO_BONUS * level;

        lines += lines_cleared;
        level = (lines / 10) + 1;
    } else {
        combo = 0;
    }
}

int ctetris_get_score(void) { return score; }
int ctetris_get_level(void) { return level; }
int ctetris_get_lines(void) { return lines; }
int ctetris_get_combo(void) { return combo; }

static bool endgame_or_reset(void) {
    curr_shape = get_random_shape();
    shadow_shape = get_shadow_shape(curr_shape);

    if (shape_collides(&curr_shape).hit)
        return true;

    curr_shape_state = STATE_DROPPING;
    curr_shape_state_timer = TIMER_START;
    lock_move_count = 0;
    y_max = get_shape_y_max(&curr_shape);
    curr_shape_strafe_timer = TIMER_INACTIVE;

    return false;
}

static bool handle_grounded_shape(void) {
    if (hard_drop || (curr_shape_state == STATE_LOCKING &&
                      curr_shape_state_timer >= SHAPE_LOCK_DELAY)) {

        if (hard_drop) {
            score += (shadow_shape.pos.y - curr_shape.pos.y) * 2;

            enum ShapeType curr_t = curr_shape.type;
            curr_shape = shadow_shape;
            curr_shape.type = curr_t;
            hard_drop = false;
        }

        write_to_board(&curr_shape);
        clear_lines_and_score();

        return endgame_or_reset();
    }

    else if (curr_shape_state != STATE_LOCKING) {
        curr_shape_state = STATE_LOCKING;
        curr_shape_state_timer = TIMER_START;
    }

    return false;
}

static void handle_airborne_shape(void) {
    if (curr_shape_state == STATE_LOCKING) {
        curr_shape_state = STATE_FLOATING;
        curr_shape_state_timer = TIMER_START;
    }

    struct Shape dummy_shape = curr_shape;
    double drop_delay = (soft_drop) ? SOFT_DROP_DELAY : DROP_DELAY;
    if (move_shape(&dummy_shape, DOWN, curr_shape_state_timer, drop_delay)) {
        if (soft_drop) {
            score++;
            soft_drop = false;
        }

        if (curr_shape_state == STATE_FLOATING) {
            if (curr_shape_state_timer >= FLOATING_DELAY) {
                curr_shape = dummy_shape;
                curr_shape_state = STATE_DROPPING;
                curr_shape_state_timer = TIMER_START;
            }
        } else {
            curr_shape = dummy_shape;
            curr_shape_state_timer = TIMER_START;
        }
    }

    int current_max_y = get_shape_y_max(&curr_shape);
    if (current_max_y > y_max) {
        y_max = current_max_y;
        lock_move_count = 0;
    }
}

bool ctetris_update(double delta) {
    if (curr_shape_state_timer != TIMER_INACTIVE)
        curr_shape_state_timer += delta;
    if (curr_shape_strafe_timer != TIMER_INACTIVE)
        curr_shape_strafe_timer += delta;

    if (new_move) {
        handle_move_limit();
        new_move = false;
    }

    if (is_shape_grounded() || hard_drop)
        return handle_grounded_shape();

    handle_airborne_shape();
    return false;
}
