#ifndef CTETRIS_H
#define CTETRIS_H

#include <stdbool.h>
#include <stdint.h>

#define ROWS 20
#define COLS 10

#define OFFSETS_COUNT 4
#define SHAPE_COUNT 9

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

struct Shape;

struct Coord {
    int x;
    int y;
};

void ctetris_init(void);
void ctetris_rotate_cw(void);
void ctetris_rotate_acw(void);
void ctetris_strafe_left(bool delay);
void ctetris_strafe_right(bool delay);
void ctetris_hard_drop(void);
void ctetris_soft_drop(void);

int ctetris_get_score(void);
int ctetris_get_level(void);
int ctetris_get_lines(void);
int ctetris_get_combo(void);

enum ShapeType ctetris_get_board_cell(uint8_t row, uint8_t col);
enum ShapeType ctetris_get_type(const struct Shape *shape);
const struct Shape *ctetris_get_curr_shape(void);
const struct Shape *ctetris_get_curr_shadow_shape(void);
const struct Shape *ctetris_get_next_shape(void);
struct Coord ctetris_get_offset(const struct Shape *shape, uint8_t offset_n);
struct Coord ctetris_get_pos(const struct Shape *shape);

bool ctetris_update(double delta);

#endif
