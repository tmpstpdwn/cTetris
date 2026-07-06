/* [ INCLUDES ] */

#include "renderer.h"

#include <stdio.h>

#include "ctetris.h"

#include "raylib.h"
#include "raymath.h"

#include "assets.h"

/* [ DEFINES ] */

#define SCALE_INT(val, scale) ((int)roundf((val) * (scale)))

#define TITLE_ACCENTED "c"
#define TITLE_DIM "Tetris"

#define SPLIT_AT_PCT 0.5f

#define TEXT_BUFF_LEN 20

#define TITLE_FONT_SIZE FONT_MD_S
#define BADGE_FONT_SIZE FONT_SM_S
#define ACTIVE_STAT_FONT_SIZE FONT_LG_S
#define RECORD_STAT_FONT_SIZE FONT_MD_S
#define LABEL_FONT_SIZE FONT_MD_S

#define FADE_IN_SPEED 5.0f
#define FADE_OUT_SPEED 4.0f
#define LINE_MOVE_SPEED 10.0f
#define FLASH_SPEED 10.0f
#define LOCK_FADE_SPEED 2.0f

#define TRAIL_ANIM_SPEED 12.0f
#define BADGE_ANIM_SPEED 10.0f

#define THEME_T_SPEED 10.0f
#define GAMEOVER_T_SPEED 5.0f

/* [ ENUMS AND STRUCTS ] */

enum ColorScheme {
    SCHEME_DARK,
    SCHEME_LIGHT,
    SCHEME_COUNT,
};

// All distinct colors used per colorscheme.
enum ColorIndex {
    COL_BG,
    COL_BG_ALT,
    COL_FG,
    COL_FG_ALT,
    COL_OUTLINE,
    COL_O,
    COL_L,
    COL_I,
    COL_T,
    COL_S,
    COL_J,
    COL_Z,
    COL_SHADOW,
    COL_COUNT,
};

// All keybindings.
enum KeyBind {
    KB_MOVE_LEFT,
    KB_MOVE_RIGHT,
    KB_ROTATE_CW,
    KB_ROTATE_ACW, // Anit-clockwise.
    KB_SOFT_DROP,
    KB_HARD_DROP,
    KB_PAUSE,
    KB_RESTART,
    KB_MUTE,
    KB_THEME,
    KEY_BINDS,
};

// UI animation types.
enum UIAnimType {
    ANIM_NONE,      // No animations.
    ANIM_LERP,      // Linear interpolation bw colors: lerp_from -> lerp_to.
    ANIM_FLASH,     // Pretty much "lerp to WHITE".
    ANIM_TRANSLATE, // Animate translation (movement).
};

// Represents a ui animation.
struct UIAnim {
    enum UIAnimType type;
    float progress; // (0.0f - 1.0f)
    float speed;    // duration = 1/speed.
    // For ANIM_TRANSLATE.
    Vector2 move_from;
    Vector2 move_to;
    // For ANIM_LERP.
    enum ColorIndex lerp_from;
    enum ColorIndex lerp_to;
};

// Represents Any sort of text used on UI.
struct UIText {
    char text[TEXT_BUFF_LEN]; // buffer.
    struct UIAnim anim;
    Rectangle bounds;
    Font *font;
    int font_size;
    int font_spacing;
    enum ColorIndex color;
};

// Represents a badge, which is just a rectangle with text inside it and a
// border.
struct UIBadge {
    struct UIAnim border_anim;
    struct UIAnim bg_anim;
    enum ColorIndex border_col;
    enum ColorIndex bg_col;
    struct UIText text;
    Rectangle bounds;
    int pad_h;
    int pad_v;
    bool active;
};

// Represents a single keyinfo, which just a badge containing key name and its
// description displayed side by side.
struct UIKeyInfo {
    struct UIBadge badge;
    struct UIText desc;
};

// Line/Separator.
struct UIDivider {
    Rectangle bounds;
    enum ColorIndex color;
    int thickness;
};

// Represents a grid row.
struct UIGridCell {
    struct UIAnim anim;
    enum ShapeType type;
};

// A UIShape.
struct UIShape {
    struct Shape shape; // Engine's shape definition.
    struct UIAnim anim;
};

/* [ GLOBAL - VARIABLES ] */

// Layout contrainsts derived at runtime wrt the dimensions of the monitor it
// was spawned on. The "_S" in the below variable names stands for "SCALED".
static int CELL_S;      // Size of main grid cell.
static int PREV_CELL_S; // Size of preview grid cell.
static int CELL_PAD_S;  // padding per main grid cell (also
                        // the same for preview grid).

// Header, grid and sidebar container origins.
static int HDR_CON_X_S, HDR_CON_Y_S, GRID_CON_X_S, GRID_CON_Y_S, SB_CON_X_S,
    SB_CON_Y_S;

// Actual grid draw origin - the grid's real pixel footprint (CELL_S * COLS
// by CELL_S * ROWS) centered within the grid container.
static int GRID_CX_S, GRID_CY_S;

// Header container width and height.
static int HDR_CON_H_S, HDR_CON_W_S;
// Same as above but for the grid.
static int GRID_CON_W_S, GRID_CON_H_S;
// Same as above but for the sidebar.
static int SB_CON_PAD_S, SB_CON_H_S, SB_CON_W_S;

// Padding, Margin used for items in the sidebar.
static int SB_SEC_PAD_S, SB_LABEL_MG_S, SB_KEYINFO_MG_S;

// Logical width and height of the window.
static int LW_S, LH_S;

// Font sizes for small, medium and large fonts.
static int FONT_SM_S, FONT_MD_S, FONT_LG_S;

// The size to load small, medium and large fonts adjusting for high dpi
// scaling.
static int FONT_SM_LOAD_SZ, FONT_MD_LOAD_SZ, FONT_LG_LOAD_SZ;

// Assets.
static Font font_sm, font_md, font_lg;
static Sound sfx_thud, sfx_click, sfx_clack;

// Visual ui stuff.
static const Color COLORS[SCHEME_COUNT][COL_COUNT] =
    {
        [SCHEME_DARK] =
            {
                [COL_BG] = {18, 18, 18, 255},
                [COL_BG_ALT] = {28, 28, 30, 255},
                [COL_FG] = {229, 229, 234, 255},
                [COL_FG_ALT] = {142, 142, 147, 255},
                [COL_OUTLINE] = {44, 44, 46, 255},
                [COL_SHADOW] = {50, 50, 52, 255},
                [COL_O] = {242, 201, 76, 255},
                [COL_L] = {242, 153, 74, 255},
                [COL_I] = {86, 204, 242, 255},
                [COL_T] = {187, 107, 217, 255},
                [COL_S] = {39, 174, 96, 255},
                [COL_J] = {47, 128, 237, 255},
                [COL_Z] = {235, 87, 87, 255},
            },
        [SCHEME_LIGHT] =
            {
                [COL_BG] = {244, 244, 246, 255},
                [COL_BG_ALT] = {230, 230, 235, 255},
                [COL_FG] = {44, 44, 46, 255},
                [COL_FG_ALT] = {142, 142, 147, 255},
                [COL_OUTLINE] = {209, 209, 214, 255},
                [COL_SHADOW] = {218, 218, 222, 255},
                [COL_O] = {212, 163, 11, 255},
                [COL_L] = {214, 116, 28, 255},
                [COL_I] = {13, 142, 179, 255},
                [COL_T] = {140, 63, 189, 255},
                [COL_S] = {27, 138, 72, 255},
                [COL_J] = {29, 97, 194, 255},
                [COL_Z] = {196, 43, 43, 255},
            },
};

static enum ColorScheme current_scheme = SCHEME_DARK;
static enum ColorScheme prev_scheme = SCHEME_DARK;

// Progress meter (0.0f - 1.0f) for theme switch and game over lerp.
static float theme_switch_t = 0.0f;
static float game_over_t = 0.0f;

static struct UIText txt_title_accent, txt_title_dim;
static struct UIText txt_lbl_score, txt_lbl_best, txt_lbl_level, txt_lbl_lines,
    txt_lbl_next, txt_lbl_controls;
static struct UIText txt_score, txt_hs, txt_level, txt_lines;

static struct UIBadge badge_combo;

static struct UIDivider div_hdr_bottom;
static struct UIDivider div_sb_left;
static struct UIDivider div_sb_1;
static struct UIDivider div_sb_2;
static struct UIDivider div_sb_3;
static struct UIDivider div_sb_stats;

// The visual ui grid which will be used for drawing.
static struct UIGridCell ui_grid[ROWS][COLS];
static bool grid_anim = false; // Is the grid being animated?

// Players current shape.
static struct UIShape player_active_shape;
// Ghost projection / Shadow of the current active shape.
static struct UIShape player_shadow_shape;
// Preview of the next shape.
static struct UIShape player_prev_shape;

// Both are set to false when the shape locks.
// Used to avoid drawing stale shapes during the duration from when a shape
// locks to a new shape is spawned as animations might be drawn those frames.
static bool curr_shape_exists = false;
static bool curr_shadow_exists = false;

// Hard drop trail animation.
static struct UIAnim trail_anim;
// Hard drop trail animations works by drawing vertical rectangular stripes
// along the horizontal axis bw the shape and its shadow.
// For example: A Horizontal I shape would need 4 of these rectangles, a
// vertical one would only need one.
static Rectangle trail_rects[OFFSETS_COUNT];
// Number of rectangles needed for the current trail
// animation. always <= OFFSETS_COUNT as shapes can
// atmost be only 4 units horizontally long.
static int trail_rect_count;

static struct UIKeyInfo keybindings[KEY_BINDS];

// Key names for keyboard.
static const char *keyboard_key_names[KEY_BINDS] = {
    [KB_MOVE_LEFT] = "LEFT", [KB_MOVE_RIGHT] = "RIGHT", [KB_ROTATE_CW] = "UP",
    [KB_ROTATE_ACW] = "Z",   [KB_SOFT_DROP] = "DOWN",   [KB_HARD_DROP] = "SPC",
    [KB_PAUSE] = "P",        [KB_RESTART] = "R",        [KB_MUTE] = "M",
    [KB_THEME] = "T",
};

// Key bind descriptions.
static const char *kb_desc[KEY_BINDS] = {
    [KB_MOVE_LEFT] = "Move left", [KB_MOVE_RIGHT] = "Move right",
    [KB_ROTATE_CW] = "Rotate cw", [KB_ROTATE_ACW] = "Rotate ccw",
    [KB_SOFT_DROP] = "Soft drop", [KB_HARD_DROP] = "Hard drop",
    [KB_PAUSE] = "Pause",         [KB_RESTART] = "Restart",
    [KB_MUTE] = "Mute",           [KB_THEME] = "Toggle theme",
};

// Game state stuff
static uint32_t score = 0;
static uint32_t high_score = 0;

static bool paused = false;
static bool audio_muted = false;
static bool block_engine = false;
static bool game_over = false;

/* [ FN DEF ] */

/* Helpers */

// returns raylib `Color` for the given `enum ColorIndex`.
// In case of theme switch / game over it returns modified colors presenting a
// smooth lerp animation.
static Color index_to_color(enum ColorIndex index) {
    Color c = COLORS[current_scheme][index];
    if (theme_switch_t < 1.0f)
        c = ColorLerp(COLORS[prev_scheme][index], c, theme_switch_t);
    if (game_over && index >= COL_O && index <= COL_Z)
        c = ColorLerp(c, COLORS[current_scheme][COL_BG_ALT],
                      game_over_t * 0.75f);
    return c;
}

static enum ColorIndex piece_color(enum ShapeType t) {
    switch (t) {
    case O:
        return COL_O;
    case L:
        return COL_L;
    case I:
        return COL_I;
    case T:
        return COL_T;
    case S:
        return COL_S;
    case J:
        return COL_J;
    case Z:
        return COL_Z;
    default:
        return COL_BG_ALT;
    }
}

static Font *get_font_from_sz(int font_size) {
    if (font_size == FONT_LG_S)
        return &font_lg;
    else if (font_size == FONT_MD_S)
        return &font_md;
    else
        return &font_sm;
}

/* Animation utilities */

static void anim_set_none(struct UIAnim *anim) {
    anim->type = ANIM_NONE;
    anim->progress = 0.0f;
    anim->speed = 0.0f;
}

static void anim_set_lerp(struct UIAnim *anim, float speed,
                          enum ColorIndex from, enum ColorIndex to) {
    anim->type = ANIM_LERP;
    anim->progress = 0.0f;
    anim->speed = speed;
    anim->lerp_from = from;
    anim->lerp_to = to;
}

static void anim_set_translate(struct UIAnim *anim, float speed, Vector2 from,
                               Vector2 to) {
    anim->type = ANIM_TRANSLATE;
    anim->progress = 0.0f;
    anim->speed = speed;
    anim->move_from = from;
    anim->move_to = to;
}

static void anim_set_flash(struct UIAnim *anim, float speed) {
    anim->type = ANIM_FLASH;
    anim->progress = 0.0f;
    anim->speed = speed;
}

static void anim_update(struct UIAnim *anim, float dt) {
    if (anim->type == ANIM_NONE)
        return;

    anim->progress += dt * anim->speed;
    if (anim->progress >= 1.0f) {
        anim->progress = 1.0f;
        anim->type = ANIM_NONE;
    }
}

/* Grid utilities */

static void init_ui_grid(void) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            ui_grid[r][c].type = N;
            anim_set_none(&ui_grid[r][c].anim);
        }
    }
}

static void write_to_ui_grid(struct Shape shape) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int gc = shape.pos.x + shape.offsets[i].x;
        int gr = shape.pos.y + shape.offsets[i].y;
        if (gr >= 0 && gr < ROWS && gc >= 0 && gc < COLS) {
            ui_grid[gr][gc].type = shape.type;
        }
    }
}

/* Fns to initialize primitive ui components */

static void init_ui_divider(struct UIDivider *divider, enum ColorIndex color,
                            int thickness) {
    divider->color = color;
    divider->thickness = (thickness < 1) ? 1 : thickness;
}

static void update_ui_divider(struct UIDivider *divider, int from_x, int from_y,
                              int to_x, int to_y) {
    int width = to_x > from_x ? to_x - from_x : from_x - to_x;
    int height = to_y > from_y ? to_y - from_y : from_y - to_y;

    if (width == 0)
        width = divider->thickness;
    if (height == 0)
        height = divider->thickness;

    divider->bounds = (Rectangle){.x = (float)(from_x < to_x ? from_x : to_x),
                                  .y = (float)(from_y < to_y ? from_y : to_y),
                                  .width = (float)width,
                                  .height = (float)height};
}

static void init_ui_text(struct UIText *ui_text, Font *font, int font_size,
                         enum ColorIndex col) {
    ui_text->color = col;
    ui_text->font = font;

    ui_text->font_size = font_size;
    if (ui_text->font_size < 1)
        ui_text->font_size = 1;

    ui_text->font_spacing = font_size * 0.06f;
    if (ui_text->font_spacing < 1)
        ui_text->font_spacing = 1;

    anim_set_none(&ui_text->anim);
}

static void init_ui_badge(struct UIBadge *ui_badge, Font *font, int font_size,
                          enum ColorIndex border_col, enum ColorIndex bg_col,
                          enum ColorIndex fg_col) {
    init_ui_text(&ui_badge->text, font, font_size, fg_col);

    ui_badge->border_col = border_col;
    ui_badge->bg_col = bg_col;

    anim_set_none(&ui_badge->border_anim);
    anim_set_none(&ui_badge->bg_anim);

    ui_badge->active = false;
    ui_badge->pad_h = font_size / 2;
    ui_badge->pad_v = font_size / 3;
}

static void init_ui_keyinfo(struct UIKeyInfo *key_info, Font *font,
                            int font_size) {
    init_ui_badge(&key_info->badge, font, font_size, COL_OUTLINE, COL_BG_ALT,
                  COL_FG_ALT);
    init_ui_text(&key_info->desc, font, font_size, COL_FG_ALT);
}

/* Fns to update primitive ui components */

static void update_ui_text(struct UIText *ui_text, const char *text, int *x,
                           int *y) {
    if (text) {
        snprintf(ui_text->text, sizeof(ui_text->text), "%s", text);
        Vector2 dim =
            MeasureTextEx(*ui_text->font, ui_text->text,
                          (float)ui_text->font_size, ui_text->font_spacing);

        ui_text->bounds.width = dim.x;
        ui_text->bounds.height = dim.y;
    }

    if (x)
        ui_text->bounds.x = *x;

    if (y)
        ui_text->bounds.y = *y;
}

static void update_ui_badge(struct UIBadge *ui_badge, const char *text, int *x,
                            int *y, bool *active) {
    update_ui_text(&ui_badge->text, text, NULL, NULL);

    int pos_x = x ? *x : (int)ui_badge->bounds.x;
    int pos_y = y ? *y : (int)ui_badge->bounds.y;

    ui_badge->bounds = (Rectangle){
        .x = (float)pos_x,
        .y = (float)pos_y,
        .width = ui_badge->text.bounds.width + (float)(ui_badge->pad_h * 2),
        .height = ui_badge->text.bounds.height + (float)(ui_badge->pad_v * 2)};

    int text_x =
        pos_x +
        (int)((ui_badge->bounds.width - ui_badge->text.bounds.width) / 2.0f);
    int text_y =
        pos_y +
        (int)((ui_badge->bounds.height - ui_badge->text.bounds.height) / 2.0f);

    update_ui_text(&ui_badge->text, NULL, &text_x, &text_y);

    if (!active || ui_badge->active == *active)
        return;

    ui_badge->active = *active;
    enum ColorIndex border_from, bg_from, text_from;
    enum ColorIndex border_to, bg_to, text_to;

    if (*active) {
        border_from = COL_OUTLINE;
        bg_from = COL_BG_ALT;
        text_from = COL_FG_ALT;
        border_to = COL_FG;
        bg_to = COL_OUTLINE;
        text_to = COL_FG;
    } else {
        border_from = COL_FG;
        bg_from = COL_OUTLINE;
        text_from = COL_FG;
        border_to = COL_OUTLINE;
        bg_to = COL_BG_ALT;
        text_to = COL_FG_ALT;
    }

    ui_badge->border_col = border_to;
    anim_set_lerp(&ui_badge->border_anim, BADGE_ANIM_SPEED, border_from,
                  border_to);
    ui_badge->bg_col = bg_to;
    anim_set_lerp(&ui_badge->bg_anim, BADGE_ANIM_SPEED, bg_from, bg_to);
    ui_badge->text.color = text_to;
    anim_set_lerp(&ui_badge->text.anim, BADGE_ANIM_SPEED, text_from, text_to);
}

static void update_ui_keyinfo(struct UIKeyInfo *key_info, const char *key_text,
                              const char *desc_text, int *x, int *y) {

    update_ui_badge(&key_info->badge, key_text, x, y, NULL);
    update_ui_text(&key_info->desc, desc_text, NULL, NULL);

    int desc_x =
        (int)(key_info->badge.bounds.x + key_info->badge.bounds.width) +
        key_info->desc.font_size;
    int desc_y =
        (int)(key_info->badge.bounds.y +
              (key_info->badge.bounds.height - key_info->desc.bounds.height) /
                  2.0f);

    update_ui_text(&key_info->desc, NULL, &desc_x, &desc_y);
}

/* Drawing utilities for primitive ui stuff */

// Draw a rect applying the given animation properties.
static void draw_rect(Rectangle rect, enum ColorIndex base_color, bool outline,
                      const struct UIAnim *anim) {
    Color col = index_to_color(base_color);
    Rectangle drawn_rect = rect;

    if (anim && anim->type != ANIM_NONE) {
        if (anim->type == ANIM_TRANSLATE) {
            Vector2 blended_offset =
                Vector2Lerp(anim->move_from, anim->move_to, anim->progress);
            drawn_rect.x += blended_offset.x;
            drawn_rect.y += blended_offset.y;
        } else if (anim->type == ANIM_LERP) {
            col = ColorLerp(index_to_color(anim->lerp_from),
                            index_to_color(anim->lerp_to), anim->progress);
        } else if (anim->type == ANIM_FLASH) {
            col = ColorLerp(col, WHITE, 1.0f - anim->progress);
        }
    }

    if (outline)
        DrawRectangleLinesEx(drawn_rect, 1.0f, col);
    else
        DrawRectangleRec(drawn_rect, col);
}

static void draw_ui_text(struct UIText *ui_text, float dt) {
    Color col = index_to_color(ui_text->color);
    if (ui_text->anim.type == ANIM_LERP) {
        col = ColorLerp(index_to_color(ui_text->anim.lerp_from),
                        index_to_color(ui_text->anim.lerp_to),
                        ui_text->anim.progress);
    }
    DrawTextEx(*ui_text->font, ui_text->text,
               (Vector2){ui_text->bounds.x, ui_text->bounds.y},
               (float)ui_text->font_size, ui_text->font_spacing, col);
    anim_update(&ui_text->anim, dt);
}

static void draw_ui_badge(struct UIBadge *ui_badge, float dt) {
    draw_rect(ui_badge->bounds, ui_badge->bg_col, false, &ui_badge->bg_anim);
    draw_rect(ui_badge->bounds, ui_badge->border_col, true,
              &ui_badge->border_anim);
    draw_ui_text(&ui_badge->text, dt);

    anim_update(&ui_badge->border_anim, dt);
    anim_update(&ui_badge->bg_anim, dt);
}

static void draw_ui_divider(const struct UIDivider *ui_divider) {
    draw_rect(ui_divider->bounds, ui_divider->color, false, NULL);
}

/* Caching utilities */

/* UI components store pixel coordinates (position) in them.
 * Following fns are responsible for deciding where ui compnents should go by
 * setting their position.
 * */

static void recache_dividers(void) {
    update_ui_divider(&div_hdr_bottom, HDR_CON_X_S, HDR_CON_Y_S + HDR_CON_H_S,
                      HDR_CON_X_S + HDR_CON_W_S, HDR_CON_Y_S + HDR_CON_H_S);
    update_ui_divider(&div_sb_left, SB_CON_X_S, SB_CON_Y_S, SB_CON_X_S,
                      SB_CON_Y_S + SB_CON_H_S);
}

static void recache_header(void) {
    int hdr_title_w = txt_title_accent.bounds.width +
                      txt_title_dim.bounds.width +
                      txt_title_accent.font_spacing;
    int hdr_title_h = txt_title_accent.bounds.height;

    int hdr_title_x = HDR_CON_X_S + (HDR_CON_W_S - hdr_title_w) / 2;
    int hdr_title_y = HDR_CON_Y_S + (HDR_CON_H_S - hdr_title_h) / 2;

    update_ui_text(&txt_title_accent, NULL, &hdr_title_x, &hdr_title_y);

    int dim_x = hdr_title_x + (int)txt_title_accent.bounds.width +
                txt_title_accent.font_spacing;

    update_ui_text(&txt_title_dim, NULL, &dim_x, &hdr_title_y);
}

static int recache_sidebar_score(int sb_x, int y) {
    update_ui_text(&txt_lbl_score, NULL, &sb_x, &y);

    int best_lbl_x = LW_S - SB_CON_PAD_S - (int)txt_lbl_best.bounds.width;
    update_ui_text(&txt_lbl_best, NULL, &best_lbl_x, &y);

    y += FONT_MD_S + SB_LABEL_MG_S;

    update_ui_text(&txt_score, NULL, &sb_x, &y);

    int best_val_x =
        (SB_CON_X_S + SB_CON_W_S) - SB_CON_PAD_S - (int)txt_hs.bounds.width;

    update_ui_text(&txt_hs, NULL, &best_val_x, &y);

    y += FONT_LG_S + SB_LABEL_MG_S;

    update_ui_badge(&badge_combo, NULL, &sb_x, &y, NULL);

    y += (int)badge_combo.bounds.height + SB_SEC_PAD_S;

    update_ui_divider(&div_sb_1, sb_x, y, LW_S - SB_CON_PAD_S, y);

    y += 1;

    return y;
}

static int recache_sidebar_stats(int sb_x, int y) {
    y += SB_SEC_PAD_S;
    int sb_w = SB_CON_W_S - SB_CON_PAD_S * 2;
    int split = sb_x + sb_w / 2;

    update_ui_text(&txt_lbl_level, NULL, &sb_x, &y);
    update_ui_text(&txt_lbl_lines, NULL, &split, &y);

    y += FONT_MD_S + SB_LABEL_MG_S;
    update_ui_text(&txt_level, NULL, &sb_x, &y);
    update_ui_text(&txt_lines, NULL, &split, &y);

    update_ui_divider(&div_sb_stats, split - SB_CON_PAD_S,
                      y - FONT_MD_S - SB_LABEL_MG_S, split - SB_CON_PAD_S,
                      y - FONT_MD_S - SB_LABEL_MG_S + FONT_MD_S +
                          SB_LABEL_MG_S + FONT_LG_S);

    y += FONT_LG_S + SB_SEC_PAD_S;

    update_ui_divider(&div_sb_2, sb_x, y, LW_S - SB_CON_PAD_S, y);
    y += 1;

    return y;
}

static int recache_sidebar_preview(int sb_x, int y) {
    y += SB_SEC_PAD_S;
    update_ui_text(&txt_lbl_next, NULL, &sb_x, &y);

    y += FONT_MD_S + SB_LABEL_MG_S;

    y += 4 * PREV_CELL_S + SB_SEC_PAD_S;
    update_ui_divider(&div_sb_3, sb_x, y, LW_S - SB_CON_PAD_S, y);
    y += 1;

    return y;
}

static void recache_sidebar_controls(int sb_x, int y) {
    y += SB_SEC_PAD_S;
    update_ui_text(&txt_lbl_controls, NULL, &sb_x, &y);
    y += FONT_MD_S + SB_LABEL_MG_S;

    for (int i = 0; i < KEY_BINDS; i++) {
        update_ui_keyinfo(&keybindings[i], keyboard_key_names[i], kb_desc[i],
                          &sb_x, &y);
        y += (int)keybindings[i].badge.bounds.height + SB_KEYINFO_MG_S;
    }
}

static void recache_ui_layout(void) {
    recache_dividers();
    recache_header();

    int sb_x = SB_CON_X_S + SB_CON_PAD_S;
    int y = SB_CON_Y_S + SB_CON_PAD_S;

    y = recache_sidebar_score(sb_x, y);
    y = recache_sidebar_stats(sb_x, y);
    y = recache_sidebar_preview(sb_x, y);
    recache_sidebar_controls(sb_x, y);
}

/* Draw UI components */

// Draw the hard drop trail if active.

static void draw_ui_grid(float dt) {
    Rectangle border_rect = {(float)GRID_CX_S, (float)GRID_CY_S,
                             (float)(CELL_S * COLS), (float)(CELL_S * ROWS)};
    draw_rect(border_rect, COL_OUTLINE, true, NULL);

    bool still_animating = false;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            Rectangle cell_rect = {
                (float)(GRID_CX_S + c * CELL_S + CELL_PAD_S),
                (float)(GRID_CY_S + r * CELL_S + CELL_PAD_S),
                (float)(CELL_S - CELL_PAD_S * 2),
                (float)(CELL_S - CELL_PAD_S * 2),
            };
            draw_rect(cell_rect, COL_BG_ALT, false, NULL);

            if (ui_grid[r][c].type != N ||
                ui_grid[r][c].anim.type != ANIM_NONE) {
                draw_rect(cell_rect, piece_color(ui_grid[r][c].type), false,
                          &ui_grid[r][c].anim);
                anim_update(&ui_grid[r][c].anim, dt);
            }

            still_animating =
                still_animating || (ui_grid[r][c].anim.type != ANIM_NONE);
        }
    }
    grid_anim = still_animating;
}

static void draw_ui_shape(struct UIShape *ui_shape, enum ColorIndex col_ind,
                          float dt) {
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int gc = ui_shape->shape.pos.x + ui_shape->shape.offsets[i].x;
        int gr = ui_shape->shape.pos.y + ui_shape->shape.offsets[i].y;
        if (gr >= 0 && gr < ROWS && gc >= 0 && gc < COLS) {
            Rectangle r = {
                (float)(GRID_CX_S + gc * CELL_S + CELL_PAD_S),
                (float)(GRID_CY_S + gr * CELL_S + CELL_PAD_S),
                (float)(CELL_S - CELL_PAD_S * 2),
                (float)(CELL_S - CELL_PAD_S * 2),
            };
            draw_rect(r, col_ind, false, &ui_shape->anim);
        }
    }
    anim_update(&ui_shape->anim, dt);
}

static void draw_header(float dt) {
    draw_ui_text(&txt_title_accent, dt);
    draw_ui_text(&txt_title_dim, dt);
}

static void draw_score(float dt) {
    draw_ui_text(&txt_lbl_score, dt);
    draw_ui_text(&txt_lbl_best, dt);
    draw_ui_text(&txt_score, dt);
    draw_ui_text(&txt_hs, dt);

    draw_ui_badge(&badge_combo, dt);
    draw_ui_divider(&div_sb_1);
}

static void draw_stats(float dt) {
    draw_ui_text(&txt_lbl_level, dt);
    draw_ui_text(&txt_lbl_lines, dt);
    draw_ui_text(&txt_level, dt);
    draw_ui_text(&txt_lines, dt);

    draw_ui_divider(&div_sb_stats);
    draw_ui_divider(&div_sb_2);
}

static void draw_preview(float dt) {
    draw_ui_text(&txt_lbl_next, dt);

    int sb_x = (int)txt_lbl_next.bounds.x;
    int box = 4 * PREV_CELL_S;
    int box_y = (int)txt_lbl_next.bounds.y + FONT_MD_S + SB_LABEL_MG_S;
    Rectangle box_r = {(float)sb_x, (float)box_y, (float)box, (float)box};
    draw_rect(box_r, COL_OUTLINE, true, NULL);

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4;
             c++) { // Fix: increment c instead of r to break infinite loop
            Rectangle cell_r = {
                (float)(sb_x + c * PREV_CELL_S + CELL_PAD_S),
                (float)(box_y + r * PREV_CELL_S + CELL_PAD_S),
                (float)(PREV_CELL_S - CELL_PAD_S * 2),
                (float)(PREV_CELL_S - CELL_PAD_S * 2),
            };
            draw_rect(cell_r, COL_BG_ALT, false, NULL);
        }
    }

    struct Shape *s = &player_prev_shape.shape;
    int min_x = s->offsets[0].x, max_x = s->offsets[0].x;
    int min_y = s->offsets[0].y, max_y = s->offsets[0].y;
    for (int i = 1; i < OFFSETS_COUNT; i++) {
        if (s->offsets[i].x < min_x)
            min_x = s->offsets[i].x;
        if (s->offsets[i].x > max_x)
            max_x = s->offsets[i].x;
        if (s->offsets[i].y < min_y)
            min_y = s->offsets[i].y;
        if (s->offsets[i].y > max_y)
            max_y = s->offsets[i].y;
    }
    int off_x = (4 - (max_x - min_x + 1)) / 2 - min_x;
    int off_y = (4 - (max_y - min_y + 1)) / 2 - min_y;

    enum ColorIndex col_ind = piece_color(s->type);
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int gc = s->offsets[i].x + off_x;
        int gr = s->offsets[i].y + off_y;
        Rectangle r = {
            (float)(sb_x + gc * PREV_CELL_S + CELL_PAD_S),
            (float)(box_y + gr * PREV_CELL_S + CELL_PAD_S),
            (float)(PREV_CELL_S - CELL_PAD_S * 2),
            (float)(PREV_CELL_S - CELL_PAD_S * 2),
        };
        draw_rect(r, col_ind, false, &player_prev_shape.anim);
    }
    anim_update(&player_prev_shape.anim, dt);

    draw_ui_divider(&div_sb_3);
}

static void draw_controls(float dt) {
    draw_ui_text(&txt_lbl_controls, dt);

    for (int i = 0; i < KEY_BINDS; i++) {
        draw_ui_badge(&keybindings[i].badge, dt);
        draw_ui_text(&keybindings[i].desc, dt);
    }
}

/* Hard drop trail */

// Sets up hard drop trail rectangles for the given shape and path.
static void trail_set(struct Shape shape, struct Coord from, struct Coord to) {
    /* The objective here is to use 2 arrays of size OFFSETS_COUNT.
     * One stores grid coordinates of the given shape's bottom most cells when
     * it is at `from`.
     * The other stores grid coordinates of the given shape's top
     * most cells when it is at `to`.
     * Then rectangles are drawn bw them.
     * The number of rectangles to be drawn are the number of bottom most / top
     * most cells that shape has in that particular orientation.
     * For example:
     * The O shape only requires just 2 rectangles in all orientation.
     * */

    trail_rect_count = 0;

    // Computes minimum x, to be used as index for the first cell in the 2
    // arrays mensioned above.
    int min_x = shape.offsets[0].x + shape.pos.x;
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int new_x = shape.offsets[i].x + from.x;
        if (new_x < min_x)
            min_x = new_x;
    }

    // As the shapes are continuous, the second cell is just
    // min_x + 1, the fourth cell would be min_x + 3.
    struct Coord starts[OFFSETS_COUNT]; // max y for a given x. bottom most.
    struct Coord ends[OFFSETS_COUNT];   // min y for a given x. top most.
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        starts[i] = (struct Coord){-1, -1};
        ends[i] = (struct Coord){-1, -1};
    }

    // Filling the 2 arrays above.
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        int x = shape.offsets[i].x + from.x;
        int yb = shape.offsets[i].y + from.y;
        int yt = shape.offsets[i].y + to.y;
        int ind = x - min_x; // Computing correct index with min_x.

        if (starts[ind].x == -1) {
            starts[ind] = (struct Coord){x, yb};
            ends[ind] = (struct Coord){x, yt};
            trail_rect_count++;
            continue;
        }

        if (yb > starts[ind].y)
            starts[ind].y = yb;
        if (yt < ends[ind].y)
            ends[ind].y = yt;
    }

    // Setting up the rectangles.
    int active_idx = 0;
    for (int i = 0; i < OFFSETS_COUNT; i++) {
        if (starts[i].x != -1) {
            int rx = GRID_CX_S + starts[i].x * CELL_S;
            int ry = GRID_CY_S + (starts[i].y + 1) * CELL_S;
            int ry2 = GRID_CY_S + (ends[i].y) * CELL_S;

            trail_rects[active_idx++] = (Rectangle){
                (float)rx, (float)ry, (float)CELL_S, (float)(ry2 - ry)};
        }
    }

    // Setting animation.
    anim_set_lerp(&trail_anim, TRAIL_ANIM_SPEED, COL_SHADOW, COL_SHADOW);
}

static void draw_trail(float dt) {
    if (trail_anim.type == ANIM_NONE)
        return;

    for (int i = 0; i < trail_rect_count; i++) {
        draw_rect(trail_rects[i], COL_SHADOW, false, &trail_anim);
    }

    anim_update(&trail_anim, dt);
}

/* Event handler */

// Handle the given engine event. returns false if the engine should be blocked.
static bool handle_event(struct CTetrisEvent ev) {
    switch (ev.type) {
    case CTETRIS_EVENT_GAME_OVER:
        game_over = true;
        game_over_t = 0.0f;
        return false; // Engine blocked untile manual reset.

    case CTETRIS_EVENT_LINE_MOVE:
        for (int i = 0; i < ev.data.line_ev.lines; i++) {
            int src = ev.data.line_ev.info[i].x;
            int dst = ev.data.line_ev.info[i].y;

            for (int c = 0; c < COLS; c++) {
                if (ui_grid[src][c].type == N)
                    continue;
                ui_grid[dst][c] = ui_grid[src][c];
                anim_set_translate(
                    &ui_grid[dst][c].anim, LINE_MOVE_SPEED,
                    (Vector2){0.0f, (float)((src - dst) * CELL_S)},
                    (Vector2){0.0f, 0.0f});
                ui_grid[src][c].type = N;
            }
        }
        return false; // Engine blocked for animation.

    case CTETRIS_EVENT_LINE_CLEAR:
        for (int i = 0; i < ev.data.line_ev.lines; i++) {
            int row = ev.data.line_ev.info[i].x;
            for (int c = 0; c < COLS; c++) {
                anim_set_lerp(&ui_grid[row][c].anim, FADE_OUT_SPEED,
                              piece_color(ui_grid[row][c].type), COL_BG_ALT);
                ui_grid[row][c].type = N;
            }
        }
        return false; // Engine blocked for animation.

    case CTETRIS_EVENT_SCORE_UPDATE:
        score = ev.data.score;
        snprintf(txt_score.text, sizeof(txt_score.text), "%u", score);
        if (score > high_score)
            renderer_set_high_score(score);
        break;

    case CTETRIS_EVENT_LEVEL_UPDATE: {
        char buf[TEXT_BUFF_LEN];
        snprintf(buf, sizeof(buf), "%u", ev.data.level);
        update_ui_text(&txt_level, buf, NULL, NULL);
        recache_ui_layout();
        break;
    }

    case CTETRIS_EVENT_LINES_UPDATE: {
        char buf[TEXT_BUFF_LEN];
        snprintf(buf, sizeof(buf), "%u", ev.data.lines);
        update_ui_text(&txt_lines, buf, NULL, NULL);
        recache_ui_layout();
    } break;

    case CTETRIS_EVENT_COMBO_UPDATE: {
        char buf[TEXT_BUFF_LEN];
        snprintf(buf, sizeof(buf), "%ux COMBO", ev.data.combo);
        bool combo_state = ev.data.combo != 0;
        update_ui_badge(&badge_combo, buf, NULL, NULL, &combo_state);
        recache_ui_layout();
        break;
    }

    case CTETRIS_EVENT_LOCK_CANCEL:
        anim_set_none(&player_active_shape.anim);
        break;

    case CTETRIS_EVENT_LOCK_START:
    case CTETRIS_EVENT_LOCK_RESET:
        anim_set_lerp(&player_active_shape.anim, LOCK_FADE_SPEED,
                      piece_color(player_active_shape.shape.type), COL_SHADOW);
        break;

    case CTETRIS_EVENT_LOCK_DONE:
        if (!audio_muted)
            PlaySound(sfx_click);
        anim_set_flash(&player_active_shape.anim, FLASH_SPEED);
        write_to_ui_grid(player_active_shape.shape);
        return false; // Engine blocked for animation.

    case CTETRIS_EVENT_HARD_DROP:
        if (!audio_muted)
            PlaySound(sfx_thud);
        trail_set(player_active_shape.shape, player_active_shape.shape.pos,
                  player_shadow_shape.shape.pos);
        return false; // Engine blocked for animation.

    case CTETRIS_EVENT_SOFT_DROP:
    case CTETRIS_EVENT_SHIFT:
    case CTETRIS_EVENT_ROTATE:
        if (!audio_muted)
            PlaySound(sfx_clack);
        break;

    case CTETRIS_EVENT_ACTIVE_SHAPE_UPDATE:
        player_active_shape.shape = ev.data.shape;
        if (!curr_shape_exists) {
            // Fade in animation for new active shape.
            anim_set_lerp(&player_active_shape.anim, FADE_IN_SPEED, COL_BG_ALT,
                          piece_color(ev.data.shape.type));
            curr_shape_exists = true;
        }
        break;

    case CTETRIS_EVENT_SHADOW_SHAPE_UPDATE:
        player_shadow_shape.shape = ev.data.shape;
        if (!curr_shadow_exists) {
            // Fade in animation for new shadow shape.
            anim_set_lerp(&player_shadow_shape.anim, FADE_IN_SPEED, COL_BG_ALT,
                          COL_SHADOW);
            curr_shadow_exists = true;
        }
        break;

    case CTETRIS_EVENT_NEXT_SHAPE_UPDATE:
        player_prev_shape.shape = ev.data.shape;
        // Fade in animation for new next shape.
        anim_set_lerp(&player_prev_shape.anim, FADE_IN_SPEED, COL_BG_ALT,
                      piece_color(ev.data.shape.type));
        break;

    default:
        break;
    }
    return true;
}

/* Asset management */

static void load_assets(void) {
    int codepoints[99];
    for (int i = 0; i < 95; i++)
        codepoints[i] = 32 + i;
    codepoints[95] = 0xF057E;
    codepoints[96] = 0xF0581;
    codepoints[97] = 0xF040A;
    codepoints[98] = 0xF03e4;

    font_sm = LoadFontFromMemory(".ttf", FONT_TTF, FONT_TTF_LEN,
                                 FONT_SM_LOAD_SZ, codepoints, 99);
    font_md = LoadFontFromMemory(".ttf", FONT_TTF, FONT_TTF_LEN,
                                 FONT_MD_LOAD_SZ, codepoints, 99);
    font_lg = LoadFontFromMemory(".ttf", FONT_TTF, FONT_TTF_LEN,
                                 FONT_LG_LOAD_SZ, codepoints, 99);

    SetTextureFilter(font_sm.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(font_md.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(font_lg.texture, TEXTURE_FILTER_POINT);

    Wave sfx_thud_wave =
        LoadWaveFromMemory(".wav", SFX_THUD_WAV, SFX_THUD_WAV_LEN);
    Wave sfx_click_wave =
        LoadWaveFromMemory(".wav", SFX_CLICK_WAV, SFX_CLICK_WAV_LEN);
    Wave sfx_clack_wave =
        LoadWaveFromMemory(".wav", SFX_CLACK_WAV, SFX_CLACK_WAV_LEN);

    sfx_thud = LoadSoundFromWave(sfx_thud_wave);
    sfx_click = LoadSoundFromWave(sfx_click_wave);
    sfx_clack = LoadSoundFromWave(sfx_clack_wave);

    UnloadWave(sfx_thud_wave);
    UnloadWave(sfx_click_wave);
    UnloadWave(sfx_clack_wave);
}

static void unload_assets(void) {
    UnloadFont(font_sm);
    UnloadFont(font_md);
    UnloadFont(font_lg);
    UnloadSound(sfx_thud);
    UnloadSound(sfx_click);
    UnloadSound(sfx_clack);
}

/* UI Core */

static void compute_layout(int logical_w, int logical_h, double dpi_scale) {
    LW_S = logical_w;
    LH_S = logical_h;

    FONT_LG_S = (int)(0.04f * LW_S);
    FONT_MD_S = (int)(0.6f * FONT_LG_S);
    FONT_SM_S = (int)(0.5f * FONT_LG_S);

    FONT_SM_LOAD_SZ = SCALE_INT((float)FONT_SM_S, (float)dpi_scale);
    FONT_MD_LOAD_SZ = SCALE_INT((float)FONT_MD_S, (float)dpi_scale);
    FONT_LG_LOAD_SZ = SCALE_INT((float)FONT_LG_S, (float)dpi_scale);

    int split_x = (int)(logical_w * SPLIT_AT_PCT);

    HDR_CON_X_S = 0;
    HDR_CON_Y_S = 0;
    HDR_CON_W_S = split_x;
    HDR_CON_H_S = TITLE_FONT_SIZE * 2;

    GRID_CON_X_S = 0;
    GRID_CON_Y_S = HDR_CON_H_S;
    GRID_CON_W_S = split_x;
    GRID_CON_H_S = logical_h - HDR_CON_H_S;

    CELL_S = GRID_CON_H_S / (ROWS + 2);
    CELL_PAD_S = (int)(CELL_S * 0.05f);
    PREV_CELL_S = (int)(CELL_S * 0.9f);

    GRID_CX_S = GRID_CON_X_S + (GRID_CON_W_S - CELL_S * COLS) / 2;
    GRID_CY_S = GRID_CON_Y_S + (GRID_CON_H_S - CELL_S * ROWS) / 2;

    SB_CON_X_S = split_x;
    SB_CON_Y_S = 0;
    SB_CON_W_S = logical_w - split_x;
    SB_CON_H_S = logical_h;

    SB_CON_PAD_S = (int)(0.04f * SB_CON_W_S);

    SB_SEC_PAD_S = LABEL_FONT_SIZE;
    SB_LABEL_MG_S = LABEL_FONT_SIZE * 0.5;
    SB_KEYINFO_MG_S = SB_LABEL_MG_S * 0.5;
}

// Initialize all UI elements to a default state before any rendering.
static void init_ui_elements(void) {
    // Header text
    init_ui_text(&txt_title_accent, get_font_from_sz(TITLE_FONT_SIZE),
                 TITLE_FONT_SIZE, COL_FG);
    init_ui_text(&txt_title_dim, get_font_from_sz(TITLE_FONT_SIZE),
                 TITLE_FONT_SIZE, COL_FG_ALT);
    update_ui_text(&txt_title_accent, TITLE_ACCENTED, NULL, NULL);
    update_ui_text(&txt_title_dim, TITLE_DIM, NULL, NULL);

    // Sidebar labels
    init_ui_text(&txt_lbl_score, get_font_from_sz(LABEL_FONT_SIZE),
                 LABEL_FONT_SIZE, COL_FG_ALT);
    init_ui_text(&txt_lbl_best, get_font_from_sz(LABEL_FONT_SIZE),
                 LABEL_FONT_SIZE, COL_FG_ALT);
    init_ui_text(&txt_lbl_level, get_font_from_sz(LABEL_FONT_SIZE),
                 LABEL_FONT_SIZE, COL_FG_ALT);
    init_ui_text(&txt_lbl_lines, get_font_from_sz(LABEL_FONT_SIZE),
                 LABEL_FONT_SIZE, COL_FG_ALT);
    init_ui_text(&txt_lbl_next, get_font_from_sz(LABEL_FONT_SIZE),
                 LABEL_FONT_SIZE, COL_FG_ALT);
    init_ui_text(&txt_lbl_controls, get_font_from_sz(LABEL_FONT_SIZE),
                 LABEL_FONT_SIZE, COL_FG_ALT);
    update_ui_text(&txt_lbl_score, "SCORE", NULL, NULL);
    update_ui_text(&txt_lbl_best, "BEST", NULL, NULL);
    update_ui_text(&txt_lbl_level, "LEVEL", NULL, NULL);
    update_ui_text(&txt_lbl_lines, "LINES", NULL, NULL);
    update_ui_text(&txt_lbl_next, "NEXT", NULL, NULL);
    update_ui_text(&txt_lbl_controls, "CONTROLS", NULL, NULL);

    // Sidebar numerics
    init_ui_text(&txt_score, get_font_from_sz(ACTIVE_STAT_FONT_SIZE),
                 ACTIVE_STAT_FONT_SIZE, COL_FG);
    init_ui_text(&txt_hs, get_font_from_sz(RECORD_STAT_FONT_SIZE),
                 RECORD_STAT_FONT_SIZE, COL_FG_ALT);
    init_ui_text(&txt_level, get_font_from_sz(ACTIVE_STAT_FONT_SIZE),
                 ACTIVE_STAT_FONT_SIZE, COL_FG);
    init_ui_text(&txt_lines, get_font_from_sz(ACTIVE_STAT_FONT_SIZE),
                 ACTIVE_STAT_FONT_SIZE, COL_FG);
    update_ui_text(&txt_score, "0", NULL, NULL);
    update_ui_text(&txt_hs, "0", NULL, NULL);
    update_ui_text(&txt_level, "1", NULL, NULL);
    update_ui_text(&txt_lines, "0", NULL, NULL);

    // Badges
    init_ui_badge(&badge_combo, get_font_from_sz(BADGE_FONT_SIZE),
                  BADGE_FONT_SIZE, COL_OUTLINE, COL_BG_ALT, COL_FG_ALT);
    update_ui_badge(&badge_combo, "0x COMBO", NULL, NULL, NULL);

    // Dividers
    init_ui_divider(&div_hdr_bottom, COL_OUTLINE, 1);
    init_ui_divider(&div_sb_left, COL_OUTLINE, 1);
    init_ui_divider(&div_sb_1, COL_OUTLINE, 1);
    init_ui_divider(&div_sb_2, COL_OUTLINE, 1);
    init_ui_divider(&div_sb_3, COL_OUTLINE, 1);
    init_ui_divider(&div_sb_stats, COL_OUTLINE, 1);

    // Key bindings
    for (int i = 0; i < KEY_BINDS; i++) {
        init_ui_keyinfo(&keybindings[i], get_font_from_sz(BADGE_FONT_SIZE),
                        BADGE_FONT_SIZE);
        update_ui_keyinfo(&keybindings[i], keyboard_key_names[i], kb_desc[i],
                          NULL, NULL);
    }
}

// Initialize state variables, used on when a new game starts.
static void init_state(void) {
    ctetris_init();
    init_ui_grid();

    score = 0;

    update_ui_text(&txt_score, "0", NULL, NULL);
    update_ui_text(&txt_level, "1", NULL, NULL);
    update_ui_text(&txt_lines, "0", NULL, NULL);

    bool combo_state = false;
    update_ui_badge(&badge_combo, "0x COMBO", NULL, NULL, &combo_state);

    paused = false;
    curr_shape_exists = false;
    curr_shadow_exists = false;
    block_engine = false;
    game_over = false;
    game_over_t = 0.0f;

    recache_ui_layout();
}

// Initialize the renderer.
void renderer_init(void) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    // Init dummy window to get monitor dimensions and figure out the window's
    // logical width and height.
    InitWindow(100, 100, "cTetris");

    // Window decoration icon setup.
    Image icon = LoadImageFromMemory(".png", ICON_PNG, ICON_PNG_LEN);
    if (icon.data != NULL) {
        SetWindowIcon(icon);
        UnloadImage(icon);
    }

    int mon = GetCurrentMonitor();
    InitAudioDevice();

    // Window height and width is set to be 80% of monitor height.
    int final_h = (int)(GetMonitorHeight(mon) * 0.8f);

    // derive layout variables from the now computed window dimensions.
    compute_layout(final_h, final_h, GetWindowScaleDPI().x);

    SetWindowSize(LW_S, LH_S); // Resize.
    // Center the window on the monitor.
    SetWindowPosition((GetMonitorWidth(mon) - LW_S) / 2,
                      (GetMonitorHeight(mon) - LH_S) / 2);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // Load Font, Sound assets.
    load_assets();

    init_ui_elements();  // Setup ui elements.
    recache_ui_layout(); // Compute where ui elements would go (position).

    init_state(); // Initialize game state variables.
}

void renderer_shutdown(void) {
    unload_assets();
    CloseWindow();
}

void renderer_set_high_score(uint32_t hs) {
    high_score = hs;
    char buf[TEXT_BUFF_LEN];
    snprintf(buf, sizeof(buf), "%u", high_score);
    update_ui_text(&txt_hs, buf, NULL, NULL);
    recache_ui_layout();
}

uint32_t renderer_get_high_score(void) { return high_score; }

// Renderer's input handler.
bool renderer_input(void) {
    if (WindowShouldClose())
        return false;

    if (IsKeyPressed(KEY_P)) {
        paused = !paused;
        update_ui_badge(&keybindings[KB_PAUSE].badge, NULL, NULL, NULL,
                        &paused);
        recache_ui_layout();
    }

    if (IsKeyPressed(KEY_R)) {
        bool restart = true;
        update_ui_badge(&keybindings[KB_RESTART].badge, NULL, NULL, NULL,
                        &restart);
        init_state();

        update_ui_badge(&keybindings[KB_PAUSE].badge, NULL, NULL, NULL,
                        &paused);
        return true;
    } else {
        bool restart = false;
        update_ui_badge(&keybindings[KB_RESTART].badge, NULL, NULL, NULL,
                        &restart);
    }

    if (IsKeyPressed(KEY_T)) {
        prev_scheme = current_scheme;
        current_scheme = (current_scheme + 1) % SCHEME_COUNT;
        theme_switch_t = 0.0f;
        bool theme = true;
        update_ui_badge(&keybindings[KB_THEME].badge, NULL, NULL, NULL, &theme);
    } else {
        bool theme = false;
        update_ui_badge(&keybindings[KB_THEME].badge, NULL, NULL, NULL, &theme);
    }

    if (IsKeyPressed(KEY_M)) {
        audio_muted = !audio_muted;
        update_ui_badge(&keybindings[KB_MUTE].badge, NULL, NULL, NULL,
                        &audio_muted);
        recache_ui_layout();
    }

    if (paused || block_engine)
        return true;

    // Push input to engine.
    struct InputState input_state = {
        .shift_left_pressed = IsKeyPressed(KEY_LEFT),
        .shift_right_pressed = IsKeyPressed(KEY_RIGHT),
        .shift_left_held = IsKeyDown(KEY_LEFT),
        .shift_right_held = IsKeyDown(KEY_RIGHT),
        .soft_drop_held = IsKeyDown(KEY_DOWN),
        .rotate_cw_pressed = IsKeyPressed(KEY_UP),
        .rotate_ccw_pressed = IsKeyPressed(KEY_Z),
        .hard_drop_pressed = IsKeyPressed(KEY_SPACE),
    };
    ctetris_input_push(input_state);

    bool left = input_state.shift_left_pressed || input_state.shift_left_held;
    bool right =
        input_state.shift_right_pressed || input_state.shift_right_held;

    update_ui_badge(&keybindings[KB_HARD_DROP].badge, NULL, NULL, NULL,
                    &input_state.hard_drop_pressed);

    update_ui_badge(&keybindings[KB_MOVE_LEFT].badge, NULL, NULL, NULL, &left);

    update_ui_badge(&keybindings[KB_MOVE_RIGHT].badge, NULL, NULL, NULL,
                    &right);

    update_ui_badge(&keybindings[KB_SOFT_DROP].badge, NULL, NULL, NULL,
                    &input_state.soft_drop_held);

    update_ui_badge(&keybindings[KB_ROTATE_CW].badge, NULL, NULL, NULL,
                    &input_state.rotate_cw_pressed);

    update_ui_badge(&keybindings[KB_ROTATE_ACW].badge, NULL, NULL, NULL,
                    &input_state.rotate_ccw_pressed);

    return true;
}

// Update the game, step the engine forward and deal with events.
void renderer_update(void) {
    static double last_t = -1.0;
    double now = GetTime();
    if (last_t == -1)
        last_t = now;
    double delta = now - last_t;
    last_t = now;

    if (paused || game_over)
        return;

    // In case the engine is blocked for playing animations then block till all
    // animations are finished.
    if (block_engine) {
        if (grid_anim || (player_active_shape.anim.type != ANIM_NONE) ||
            (trail_anim.type != ANIM_NONE))
            return;

        block_engine = false;

        // This is set to true when the shape locks and set to false when all
        // animations since shape lock is over.
        curr_shape_exists = false;
        curr_shadow_exists = false;
    }

    struct CTetrisEvent ev;
    while ((ev = ctetris_event_pop()).type != CTETRIS_EVENT_NONE) {
        if (!handle_event(ev)) {
            block_engine = true;
            return;
        }
    }

    // The engine is only stepped forward when it isnt blocked and also when
    // all buffered events are processed.
    ctetris_update(delta);
}

// Render everything.
void renderer_render(void) {
    float dt = GetFrameTime();

    BeginDrawing();

    ClearBackground(index_to_color(COL_BG));

    draw_header(dt);

    draw_ui_divider(&div_hdr_bottom);
    draw_ui_divider(&div_sb_left);

    draw_ui_grid(dt);

    // stop drawing the active shape and its shadow if the engine reports the
    // shape has been locked.
    if (curr_shadow_exists)
        draw_ui_shape(&player_shadow_shape, COL_SHADOW, dt);
    if (curr_shape_exists)
        draw_ui_shape(&player_active_shape,
                      piece_color(player_active_shape.shape.type), dt);

    draw_trail(dt);

    draw_score(dt);
    draw_stats(dt);
    draw_preview(dt);
    draw_controls(dt);

    EndDrawing();

    // Theme switch and game over lerp animation update.
    if (theme_switch_t < 1.0f) {
        theme_switch_t += dt * THEME_T_SPEED;
        if (theme_switch_t > 1.0f)
            theme_switch_t = 1.0f;
    }

    if (game_over && game_over_t < 1.0f) {
        game_over_t += dt * GAMEOVER_T_SPEED;
        if (game_over_t > 1.0f)
            game_over_t = 1.0f;
    }
}

/* [ END ] */
