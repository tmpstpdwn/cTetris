#include "raylib.h"
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ROWS 20
#define COLS 10
#define CELL_SIZE 30

#define OFFSETS_COUNT 4
#define SHAPE_COUNT 9
#define DIR_COUNT 3 // count of shape movement directions;

#define MOVES_BEFORE_LOCK 15
#define SHAPE_LOCK_DELAY 0.5f

#define FLOATING_DELAY 0.7f

#define DROP_DELAY 0.5f
#define SOFT_DROP_DELAY 0.05f
#define SHIFT_DELAY 0.2f

#define BOARD_BG (Color){17, 17, 17, 255}

#define COLOR_N (Color){30, 30, 30, 255}
#define COLOR_O (Color){93, 202, 165, 255}
#define COLOR_L (Color){127, 119, 221, 255}
#define COLOR_I (Color){55, 138, 221, 255}
#define COLOR_T (Color){216, 90, 48, 255}
#define COLOR_S (Color){186, 117, 23, 255}
#define COLOR_J (Color){212, 83, 126, 255}
#define COLOR_Z (Color){99, 153, 34, 255}
#define COLOR_X (Color){42, 42, 42, 255} // SHADOW.

enum ShapeType {
  N, // NULL
  X, // Shadow.
  O,
  L,
  J,
  I,
  T,
  S,
  Z
};

enum Dir { LEFT, RIGHT, DOWN };
enum RotationDir { CLOCKWISE, ANTI_CLOCKWISE };
enum PieceState { STATE_DROPPING, STATE_LOCKING, STATE_FLOATING };

struct Shape {
  enum ShapeType type;
  Vector2 offsets[OFFSETS_COUNT];
  Vector2 pos;
};

struct CollisionStatus {
  bool hit;
  Vector2 offset; // worst colliding shape offset;
};

static struct Shape shape_o = {
    .type = O, .offsets = {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, .pos = {0, 0}};

static struct Shape shape_l = {
    .type = L, .offsets = {{0, -1}, {0, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};

static struct Shape shape_i = {
    .type = I, .offsets = {{0, -1}, {0, 0}, {0, 1}, {0, 2}}, .pos = {0, 0}};

static struct Shape shape_t = {
    .type = T, .offsets = {{-1, 0}, {0, -1}, {0, 0}, {1, 0}}, .pos = {0, 0}};

static struct Shape shape_s = {
    .type = S, .offsets = {{0, 0}, {1, 0}, {0, 1}, {-1, 1}}, .pos = {0, 0}};

static struct Shape shape_j = {
    .type = J, .offsets = {{0, -1}, {0, 0}, {0, 1}, {-1, 1}}, .pos = {0, 0}};

static struct Shape shape_z = {
    .type = Z, .offsets = {{0, 0}, {-1, 0}, {0, 1}, {1, 1}}, .pos = {0, 0}};

static enum ShapeType board[ROWS][COLS] = {0};

static struct Shape *shapes[SHAPE_COUNT] = {
    [N] = NULL,     [X] = NULL,     [O] = &shape_o,
    [L] = &shape_l, [J] = &shape_j, [I] = &shape_i,
    [T] = &shape_t, [S] = &shape_s, [Z] = &shape_z};

static Color colors[SHAPE_COUNT] = {
    [N] = COLOR_N, [X] = COLOR_X, [O] = COLOR_O, [L] = COLOR_L, [J] = COLOR_J,
    [I] = COLOR_I, [T] = COLOR_T, [S] = COLOR_S, [Z] = COLOR_Z};

static struct Shape curr_shape, shadow_shape;
static enum PieceState curr_shape_state;
static double curr_shape_state_timer;
static int lock_move_count, y_max;

static double last_move_time[DIR_COUNT] = {0};

static bool new_game = true;
static bool hard_drop = false;

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
  int index = 2;

  for (int i = 0; i < 7; i++) {
    int random_selection =
        GetRandomValue(index, SHAPE_COUNT - 1); // Exclude N & X .
    struct Shape *shape = shapes[random_selection];
    shapes[random_selection] = shapes[index];
    shapes[index] = shape;
    index++;
  }
}

static struct Shape get_random_shape(void) {
  static bool init = true;
  static int curr_shape_index = 2;
  struct Shape shape;

  if (init) {
    init = false;
    shuffle_shapes();
    shape = *shapes[curr_shape_index++];
  } else if (curr_shape_index == SHAPE_COUNT - 1) {
    shape = *shapes[curr_shape_index];
    shuffle_shapes();
    curr_shape_index = 2;
  } else {
    shape = *shapes[curr_shape_index++];
  }

  int y = 0;
  for (int i = 0; i < OFFSETS_COUNT; i++)
    y = (shape.offsets[i].y < y) ? shape.offsets[i].y : y;

  shape.pos = (Vector2){.y = -y, .x = (int)(COLS / 2)};

  return shape;
}

static struct CollisionStatus shape_collides(const struct Shape *shape) {
  struct CollisionStatus status = {0};
  bool out_of_bounds = false;
  int worst = 0;

  for (int i = 0; i < OFFSETS_COUNT; i++) {
    Vector2 offset = shape->offsets[i];
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

static bool move_shape(struct Shape *shape, enum Dir dir, double delay) {
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
    if (GetTime() - last_move_time[dir] >= delay) {
      *shape = next_shape;
      last_move_time[dir] = GetTime();
      return true;
    }
  }

  return false;
}

static void draw_shape(const struct Shape *shape) {
  for (int i = 0; i < OFFSETS_COUNT; i++) {
    Vector2 offset = shape->offsets[i];

    int x = (shape->pos.x + offset.x) * CELL_SIZE;
    int y = (shape->pos.y + offset.y) * CELL_SIZE;

    DrawRectangle(x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2,
                  colors[shape->type]);
  }
}

static void clear_board(void) {
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      board[i][j] = N;
    }
  }
}

static void write_to_board(struct Shape *shape) {
  for (int i = 0; i < OFFSETS_COUNT; i++) {
    Vector2 offset = shape->offsets[i];

    int x = shape->pos.x + offset.x;
    int y = shape->pos.y + offset.y;

    board[y][x] = shape->type;
  }
}

static void draw_board(void) {
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      DrawRectangle(j * CELL_SIZE + 1, i * CELL_SIZE + 1, CELL_SIZE - 2,
                    CELL_SIZE - 2, colors[board[i][j]]);
    }
  }
}

static int clear_and_compress(void) {
  int write = ROWS - 1;
  int rows_cleared = 0;
  for (int read = ROWS - 1; read >= 0; read--) {
    int occupied = 0;
    for (int col = 0; col < COLS; col++)
      if (board[read][col] != N)
        occupied++;
    if (occupied < COLS) {
      if (write != read)
        memcpy(board[write], board[read], COLS * sizeof(enum ShapeType));
      write--;
    } else {
      rows_cleared++;
    }
  }
  for (int row = write; row >= 0; row--)
    memset(board[row], N, COLS * sizeof(enum ShapeType));
  return rows_cleared;
}

static void ui_init(void) {
  const int screenWidth = COLS * CELL_SIZE;
  const int screenHeight = ROWS * CELL_SIZE;
  SetRandomSeed(time(NULL));
  InitWindow(screenWidth, screenHeight, "cTetris");
}

static void ui_shutdown(void) { CloseWindow(); }

static void game_init(void) {
  if (!new_game)
    return;
  clear_board();

  curr_shape = get_random_shape();
  shadow_shape = get_shadow_shape(curr_shape);
  y_max = get_shape_y_max(&curr_shape);

  curr_shape_state = STATE_DROPPING;
  curr_shape_state_timer = 0;
  lock_move_count = 0;

  last_move_time[LEFT] = last_move_time[RIGHT] = last_move_time[DOWN] = 0;

  new_game = false;
}

bool input(void) {
  bool new_move = false;

  if (IsKeyPressed(KEY_UP)) {
    new_move = rotate_shape(&curr_shape, CLOCKWISE);
  } else if (IsKeyPressed(KEY_Z)) {
    new_move = rotate_shape(&curr_shape, ANTI_CLOCKWISE);
  }

  if (IsKeyPressed(KEY_LEFT)) {
    new_move = new_move || move_shape(&curr_shape, LEFT, 0);
  } else if (IsKeyDown(KEY_LEFT)) {
    new_move = new_move || move_shape(&curr_shape, LEFT, SHIFT_DELAY);
  } else if (IsKeyPressed(KEY_RIGHT)) {
    new_move = new_move || move_shape(&curr_shape, RIGHT, 0);
  } else if (IsKeyDown(KEY_RIGHT)) {
    new_move = new_move || move_shape(&curr_shape, RIGHT, SHIFT_DELAY);
  }

  if (IsKeyDown(KEY_DOWN)) { // Soft drop
    move_shape(&curr_shape, DOWN, SOFT_DROP_DELAY);
  }

  if (IsKeyPressed(KEY_SPACE)) { // Hard drop
    hard_drop = true;
  }

  return new_move;
}

void update(bool new_move) {
  struct Shape down_check = curr_shape;
  down_check.pos.y++;
  bool is_grounded = shape_collides(&down_check).hit;

  if (new_move) {
    shadow_shape = get_shadow_shape(curr_shape);
    if (curr_shape_state != STATE_DROPPING) {
      if (lock_move_count < MOVES_BEFORE_LOCK - 1) {
        if (curr_shape_state == STATE_LOCKING) {
          curr_shape_state_timer = GetTime();
        }
        lock_move_count++;
      } else if (is_grounded) {
        hard_drop = true;
      } else {
        curr_shape_state = STATE_DROPPING;
        curr_shape_state_timer = 0;
      }
    }
  }

  if (is_grounded || hard_drop) {

    if (curr_shape_state != STATE_LOCKING && !hard_drop) {
      curr_shape_state = STATE_LOCKING;
      curr_shape_state_timer = GetTime();
    }

    // Guarantees either hard drop || STATE_LOCKING
    else if (hard_drop ||
             GetTime() - curr_shape_state_timer >= SHAPE_LOCK_DELAY) {

      if (hard_drop) {
        enum ShapeType curr_t = curr_shape.type;
        curr_shape = shadow_shape;
        curr_shape.type = curr_t;
        hard_drop = false;
      }

      write_to_board(&curr_shape);
      clear_and_compress();

      curr_shape = get_random_shape();
      shadow_shape = get_shadow_shape(curr_shape);

      if (shape_collides(&curr_shape).hit) {
        new_game = true;
      } else {
        curr_shape_state = STATE_DROPPING;
        curr_shape_state_timer = 0;
        lock_move_count = 0;
        y_max = get_shape_y_max(&curr_shape);
      }
    }

  }

  else {
    if (curr_shape_state == STATE_LOCKING) {
      curr_shape_state = STATE_FLOATING;
      curr_shape_state_timer = GetTime();
    }

    struct Shape dummy_shape = curr_shape;
    if (move_shape(&dummy_shape, DOWN, DROP_DELAY)) {

      if (curr_shape_state == STATE_FLOATING) {
        if (GetTime() - curr_shape_state_timer >= FLOATING_DELAY) {
          curr_shape = dummy_shape;
          curr_shape_state = STATE_DROPPING;
          curr_shape_state_timer = GetTime();
        }
      } else {
        curr_shape = dummy_shape;
      }
    }
  }

  int current_lowest_y = get_shape_y_max(&curr_shape);
  if (current_lowest_y > y_max) {
    y_max = current_lowest_y;
    lock_move_count = 0;
  }
}

void render(void) {
  BeginDrawing();
  ClearBackground(BOARD_BG);
  draw_board();
  draw_shape(&shadow_shape);
  draw_shape(&curr_shape);
  EndDrawing();
}

int main(void) {
  ui_init();

  while (!WindowShouldClose()) {
    game_init();
    update(input());
    render();
  }

  ui_shutdown();
  return 0;
}
