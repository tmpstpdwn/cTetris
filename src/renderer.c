/* [ RENDERER_C ] */

/* [ INCLUDES ] */

#include <stdint.h>
#include <stdio.h>

#include "raylib.h"
#include "raymath.h"

#include "assets.h"
#include "ctetris.h"

#include "renderer.h"

/* [ DEFINES ] */

#define FPS 60

// Text literals.
#define TITLE_ACCENTED "c"
#define TITLE_DIM "Tetris"
#define CREDIT_TEXT "cTetris v1.0.0 | Tmpstpdwn"

// Window height % wrt monitor height.
#define WH_TO_MH_PCT 0.8f
#define SPLIT_AT_PCT 0.5f // Where the sidebar should start %.

#define TEXT_BUFF_LEN 32

#define NEXT_GRID_SIZE 4

// Font size for text elements.
#define TITLE_FONT_SIZE FONT_LG
#define CREDIT_FONT_SIZE FONT_SM
#define LABEL_FONT_SIZE FONT_MD
#define NUMERIC_FONT_SIZE FONT_LG
#define POPUP_FONT_SIZE FONT_MD
#define BADGE_FONT_SIZE FONT_SM

// Speed for animations.
#define SHAPE_FADE_IN_SPEED 5.0f
#define SHAPE_LOCK_FADE_OUT_SPEED (1.0f / SHAPE_LOCK_DELAY)
#define SHAPE_FLASH_SPEED 10.0f

#define LINE_FADE_OUT_SPEED 4.0f
#define LINE_MOVE_SPEED 10.0f

#define TRAIL_FADE_OUT_SPEED 10.0f

#define POPUP_FADE_IN_SPEED 15.0f
#define POPUP_FADE_OUT_SPEED 1.0f

#define BADGE_STATE_LERP_SPEED 10.0f
#define THEME_LERP_SPEED 5.0f
#define GAMEOVER_LERP_SPEED 5.0f

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
    COL_NEUTRAL,
    COL_SUBTLE,
    COL_YELLOW,
    COL_ORANGE,
    COL_CYAN,
    COL_PURPLE,
    COL_GREEN,
    COL_BLUE,
    COL_RED,
    COL_COUNT,
};

// All keybindings.
enum KeyBind {
    KB_MOVE_LEFT,
    KB_MOVE_RIGHT,
    KB_ROTATE_RIGHT,
    KB_ROTATE_LEFT,
    KB_SOFT_DROP,
    KB_HARD_DROP,
    KB_PAUSE,
    KB_RESTART,
    KB_MUTE,
    KB_THEME,
    KB_COUNT,
};

// Animation types.
enum AnimType {
    ANIM_NONE,      // No animations.
    ANIM_LERP,      // Linear interpolation bw colors.
    ANIM_FLASH,     // Pretty much "lerp to WHITE".
    ANIM_TRANSLATE, // Animate translation (movement).
};

// Represents a animation.
struct Anim {
    enum AnimType type;
    float progress; // (0.0f - 1.0f)
    float speed;    // duration = 1/speed.
    // For ANIM_TRANSLATE.
    Vector2 move_from;
    Vector2 move_to;
    // For ANIM_LERP.
    enum ColorIndex lerp_from;
    enum ColorIndex lerp_to;
};

// Line/Separator.
struct UIDivider {
    Rectangle bounds; // x, y, width and height.
    enum ColorIndex color;
    uint64_t thickness;
};

// Represents any sort of text used on UI.
struct UIText {
    char text[TEXT_BUFF_LEN]; // buffer.
    struct Anim anim;
    Rectangle bounds; // x, y, width and height.
    Font *font;
    uint64_t font_size;
    uint64_t font_spacing; // computed from `font_size`.
    enum ColorIndex color; // Default color.
};

// Used for score popup.
// A popup when set active, fades in `text` and then fades it out.
// Once faded out, `active` will be automatically set to false.
struct UIPopup {
    bool active;
    bool fading_out; // Is the popup currently being faded out?
    struct UIText ui_text;
};

// Represents a badge, which is just a rectangle with text inside it and a
// border.
// When `active` set to true while it is false, the badge fades in to an active
// state, using colors that are brighter.
// When `active` set to false while it is true, the badge fades in to an
// inactive state, using colors that are dimmed.
struct UIBadge {
    bool active;
    struct Anim border_anim;
    struct Anim bg_anim;
    enum ColorIndex border_col;
    enum ColorIndex bg_col;
    struct UIText ui_text;
    Rectangle bounds; // x, y, width and  height.
    // vertical and horizontal padding.
    uint64_t pad_h;
    uint64_t pad_v;
};

// Represents a single keyinfo, which is just a badge containing key name
// displayed side by side with its description.
struct UIKeyInfo {
    struct UIBadge ui_badge;
    struct UIText ui_text;
};

// Represents a grid cell.
struct UIGridCell {
    struct Anim anim;
    enum ShapeType type;
};

// A UIShape.
struct UIShape {
    struct Shape shape; // Engine's shape definition.
    struct Anim anim;
};

/* [ VARIABLES ] */

// Layout constraints derived at runtime wrt the dimensions of the monitor it
// was spawned on.

// Size of main, next grid cell.
static uint64_t MAIN_CELL, NEXT_CELL;
// Padding for main, next grid cell.
static uint64_t MAIN_CELL_PAD, NEXT_CELL_PAD;

// Header, main grid and sidebar container origins.
static uint64_t HDR_CON_X, HDR_CON_Y, MAIN_GRID_CON_X, MAIN_GRID_CON_Y,
    SB_CON_X, SB_CON_Y;

// Actual grid content origins for main and next grid.
static uint64_t MAIN_GRID_CX, MAIN_GRID_CY;
static uint64_t NEXT_GRID_CX, NEXT_GRID_CY;

// Header container width and height.
static uint64_t HDR_CON_W, HDR_CON_H;
// Same as above but for the main grid.
static uint64_t MAIN_GRID_CON_W, MAIN_GRID_CON_H;
// Same as above but for the sidebar.
static uint64_t SB_CON_W, SB_CON_H;

// Padding for sidebar.
static uint64_t SB_CON_PAD;

// Padding, Margin used for items in the sidebar.
static uint64_t SB_SEC_PAD, SB_LABEL_MG, SB_KEYINFO_MG;

// width and height of the window.
static uint64_t W, H;

// Font sizes for small, medium and large fonts.
static uint64_t FONT_SM, FONT_MD, FONT_LG;

// Assets.
static Font font_sm, font_md, font_lg;
static Sound sfx_thud, sfx_click, sfx_clack;

// Default dark and light colorscheme definitions.
static const Color COLORS[SCHEME_COUNT][COL_COUNT] =
    {
        [SCHEME_DARK] =
            {
                [COL_BG] = {18, 18, 18, 255},
                [COL_BG_ALT] = {28, 28, 30, 255},
                [COL_FG] = {229, 229, 234, 255},
                [COL_FG_ALT] = {142, 142, 147, 255},
                [COL_SUBTLE] = {44, 44, 46, 255},
                [COL_NEUTRAL] = {50, 50, 52, 255},
                [COL_YELLOW] = {242, 201, 76, 255},
                [COL_ORANGE] = {242, 153, 74, 255},
                [COL_CYAN] = {86, 204, 242, 255},
                [COL_PURPLE] = {187, 107, 217, 255},
                [COL_GREEN] = {39, 174, 96, 255},
                [COL_BLUE] = {47, 128, 237, 255},
                [COL_RED] = {235, 87, 87, 255},
            },
        [SCHEME_LIGHT] =
            {
                [COL_BG] = {244, 244, 246, 255},
                [COL_BG_ALT] = {230, 230, 235, 255},
                [COL_FG] = {44, 44, 46, 255},
                [COL_FG_ALT] = {142, 142, 147, 255},
                [COL_SUBTLE] = {209, 209, 214, 255},
                [COL_NEUTRAL] = {218, 218, 222, 255},
                [COL_YELLOW] = {212, 163, 11, 255},
                [COL_ORANGE] = {214, 116, 28, 255},
                [COL_CYAN] = {13, 142, 179, 255},
                [COL_PURPLE] = {140, 63, 189, 255},
                [COL_GREEN] = {27, 138, 72, 255},
                [COL_BLUE] = {29, 97, 194, 255},
                [COL_RED] = {196, 43, 43, 255},
            },
};

static enum ColorScheme current_scheme = SCHEME_DARK;
static enum ColorScheme prev_scheme = SCHEME_DARK;

// Progress meter (0.0f - 1.0f) for theme switch and game over lerp.
static float theme_switch_t = 1.0f;
static float game_over_t = 0.0f;

static struct UIKeyInfo ui_key_info_list[KB_COUNT];

// Key names for keyboard.
static const char *keyboard_key_names[KB_COUNT] = {
    [KB_MOVE_LEFT] = "LEFT",  [KB_MOVE_RIGHT] = "RIGHT",
    [KB_ROTATE_RIGHT] = "UP", [KB_ROTATE_LEFT] = "Z",
    [KB_SOFT_DROP] = "DOWN",  [KB_HARD_DROP] = "SPC",
    [KB_PAUSE] = "P",         [KB_RESTART] = "R",
    [KB_MUTE] = "M",          [KB_THEME] = "T",
};

// Key bind descriptions.
static const char *kb_desc[KB_COUNT] = {
    [KB_MOVE_LEFT] = "Move left",
    [KB_MOVE_RIGHT] = "Move right",
    [KB_ROTATE_RIGHT] = "Rotate right",
    [KB_ROTATE_LEFT] = "Rotate left",
    [KB_SOFT_DROP] = "Soft drop",
    [KB_HARD_DROP] = "Hard drop",
    [KB_PAUSE] = "Pause",
    [KB_RESTART] = "Restart",
    [KB_MUTE] = "Mute",
    [KB_THEME] = "Toggle theme",
};

// All dividers.
static struct UIDivider div_hdr_bottom;
static struct UIDivider div_sb_left;
static struct UIDivider div_sb_1;
static struct UIDivider div_sb_2;
static struct UIDivider div_sb_3;
static struct UIDivider div_sb_level_lines;

// All text elements on the UI.
static struct UIText txt_title_accent, txt_title_dim;
static struct UIText txt_lbl_score, txt_lbl_best, txt_lbl_level, txt_lbl_lines,
    txt_lbl_next, txt_lbl_controls;
static struct UIText txt_score, txt_hs, txt_level, txt_lines;
static struct UIText txt_credit;

// Popup for score and combo.
static struct UIPopup pp_score, pp_combo;

// The visual ui main grid which will be used for drawing.
static struct UIGridCell ui_main_grid[ROWS][COLS];
static bool ui_main_grid_animating = false; // Is the main grid being animated?
// Set to true when the active shape locks. Used to write the active shape on to
// the visual ui main grid once the lock animation is done.
static bool ui_main_grid_write_pending = false;

// Player's current shape.
static struct UIShape player_active_shape;
// Ghost projection / Shadow of the current active shape.
static struct UIShape player_shadow_shape;
// Player's next shape.
static struct UIShape player_next_shape;

// Hard drop trail animation.
static struct Anim trail_anim;
// Hard drop trail animation works by drawing vertical rectangular stripes
// along the horizontal axis bw the shape and its projection / shadow.
// For example: A Horizontal I shape would need 4 of these rectangles, a
// vertical one would only need one.
static Rectangle trail_rects[OFFSETS_COUNT];
// Number of rectangles needed for the current trail
// animation. always <= OFFSETS_COUNT as shapes can
// atmost be only 4 cells horizontally long.
static uint8_t trail_rect_count;

// Used to animate line move after line clear animation.
// `(struct Coord){line_index_to_move, move_distance (in cells)}`
static struct Coord line_moves[ROWS];
static uint8_t line_move_count; // How many lines to move.
// Is there a line move pending to be done and animated?
static bool line_move_pending = false;

// Game stats.
static uint32_t score, high_score;
static uint8_t lines, level;

// Game state stuff.
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
    if (game_over && index >= COL_YELLOW && index <= COL_RED)
        c = ColorLerp(c, COLORS[current_scheme][COL_BG_ALT],
                      game_over_t * 0.75f);
    return c;
}

static enum ColorIndex piece_color(enum ShapeType t) {
    switch (t) {
    case O:
        return COL_YELLOW;
    case L:
        return COL_ORANGE;
    case I:
        return COL_CYAN;
    case T:
        return COL_PURPLE;
    case S:
        return COL_GREEN;
    case J:
        return COL_BLUE;
    case Z:
        return COL_RED;
    default:
        return COL_BG_ALT;
    }
}

static Font *get_font(uint64_t font_size) {
    if (font_size == FONT_LG)
        return &font_lg;
    else if (font_size == FONT_MD)
        return &font_md;
    else
        return &font_sm;
}

/* Animation utilities */

static void anim_set_none(struct Anim *anim) {
    anim->type = ANIM_NONE;
    anim->progress = 0.0f;
    anim->speed = 0.0f;
}

static void anim_set_lerp(struct Anim *anim, float speed, enum ColorIndex from,
                          enum ColorIndex to) {
    anim->type = ANIM_LERP;
    anim->progress = 0.0f;
    anim->speed = speed;
    anim->lerp_from = from;
    anim->lerp_to = to;
}

static void anim_set_translate(struct Anim *anim, float speed, Vector2 from,
                               Vector2 to) {
    anim->type = ANIM_TRANSLATE;
    anim->progress = 0.0f;
    anim->speed = speed;
    anim->move_from = from;
    anim->move_to = to;
}

static void anim_set_flash(struct Anim *anim, float speed) {
    anim->type = ANIM_FLASH;
    anim->progress = 0.0f;
    anim->speed = speed;
}

static void anim_update(struct Anim *anim, float dt) {
    if (anim->type == ANIM_NONE)
        return;

    anim->progress += dt * anim->speed;
    if (anim->progress >= 1.0f) {
        anim->progress = 1.0f;
        anim->type = ANIM_NONE;
    }
}

/* Main Grid utilities */

static void ui_main_grid_init(void) {
    for (uint8_t r = 0; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS; c++) {
            ui_main_grid[r][c].type = N;
            anim_set_none(&ui_main_grid[r][c].anim);
        }
    }
    ui_main_grid_animating = false;
}

static void ui_main_grid_shape_write(struct Shape shape) {
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        // Here the assumption is that the shape is valid and entirely within
        // the main grid.
        uint8_t gc = shape.pos.x + shape.offsets[i].x;
        uint8_t gr = shape.pos.y + shape.offsets[i].y;
        ui_main_grid[gr][gc].type = shape.type;
    }
}

/* Fns to initialize primitive ui elements */

static void ui_divider_init(struct UIDivider *ui_divider, enum ColorIndex color,
                            uint64_t thickness) {
    if (!ui_divider)
        return;

    ui_divider->color = color;
    ui_divider->thickness = (thickness < 1) ? 1 : thickness;
}

static void ui_text_init(struct UIText *ui_text, Font *font, uint64_t font_size,
                         enum ColorIndex col) {
    if (!ui_text || !font)
        return;

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

static void ui_popup_init(struct UIPopup *ui_popup, Font *font,
                          uint64_t font_size, enum ColorIndex col) {
    if (!ui_popup || !font)
        return;

    ui_popup->active = false;
    ui_popup->fading_out = false;
    ui_text_init(&ui_popup->ui_text, font, font_size, col);
}

static void ui_badge_init(struct UIBadge *ui_badge, Font *font,
                          uint64_t font_size, enum ColorIndex border_col,
                          enum ColorIndex bg_col, enum ColorIndex fg_col) {
    if (!ui_badge || !font)
        return;

    ui_text_init(&ui_badge->ui_text, font, font_size, fg_col);

    ui_badge->border_col = border_col;
    ui_badge->bg_col = bg_col;

    anim_set_none(&ui_badge->border_anim);
    anim_set_none(&ui_badge->bg_anim);

    ui_badge->active = false;
    ui_badge->pad_h = font_size / 2;
    ui_badge->pad_v = font_size / 3;
}

static void ui_keyinfo_init(struct UIKeyInfo *ui_key_info, Font *font,
                            uint64_t font_size) {
    if (!ui_key_info || !font)
        return;

    ui_badge_init(&ui_key_info->ui_badge, font, font_size, COL_SUBTLE,
                  COL_BG_ALT, COL_FG_ALT);
    ui_text_init(&ui_key_info->ui_text, font, font_size, COL_FG_ALT);
}

/* Fns to update primitive ui elements */

static void ui_divider_update(struct UIDivider *ui_divider, uint64_t from_x,
                              uint64_t from_y, uint64_t to_x, uint64_t to_y) {
    if (!ui_divider)
        return;

    uint64_t width = to_x > from_x ? to_x - from_x : from_x - to_x;
    uint64_t height = to_y > from_y ? to_y - from_y : from_y - to_y;

    if (width == 0)
        width = ui_divider->thickness;
    if (height == 0)
        height = ui_divider->thickness;

    ui_divider->bounds = (Rectangle){.x = from_x < to_x ? from_x : to_x,
                                     .y = from_y < to_y ? from_y : to_y,
                                     .width = width,
                                     .height = height};
}

static void ui_text_update(struct UIText *ui_text, const char *text,
                           uint64_t *x, uint64_t *y) {
    if (!ui_text)
        return;

    if (text) {
        snprintf(ui_text->text, sizeof(ui_text->text), "%s", text);
        Vector2 dim = MeasureTextEx(*ui_text->font, ui_text->text,
                                    ui_text->font_size, ui_text->font_spacing);

        ui_text->bounds.width = dim.x;
        ui_text->bounds.height = dim.y;
    }

    if (x)
        ui_text->bounds.x = *x;

    if (y)
        ui_text->bounds.y = *y;
}

static void ui_popup_update(struct UIPopup *ui_popup, const char *text,
                            uint64_t *x, uint64_t *y, const bool *active) {
    if (!ui_popup)
        return;

    if (active) {
        // If the popup is set to active state while its already active and the
        // popup is rn animating fade out, then pull back the animation to the
        // previous checkpoint which is right when fade in finishes (text
        // is completely opaque).
        if (*active && ui_popup->active && ui_popup->fading_out) {
            anim_set_lerp(&ui_popup->ui_text.anim, POPUP_FADE_IN_SPEED, COL_BG,
                          ui_popup->ui_text.color);
            ui_popup->ui_text.anim.progress = 1;
            ui_popup->fading_out = false;
        } else if (!*active) {
            ui_popup->fading_out = false;
            anim_set_none(&ui_popup->ui_text.anim);
        }
        ui_popup->active = *active;
    }
    ui_text_update(&ui_popup->ui_text, text, x, y);
}

static void ui_badge_update(struct UIBadge *ui_badge, const char *text,
                            uint64_t *x, uint64_t *y, const bool *active) {
    if (!ui_badge)
        return;

    ui_text_update(&ui_badge->ui_text, text, NULL, NULL);

    uint64_t pos_x = x ? *x : ui_badge->bounds.x;
    uint64_t pos_y = y ? *y : ui_badge->bounds.y;

    ui_badge->bounds = (Rectangle){
        .x = pos_x,
        .y = pos_y,
        .width = ui_badge->ui_text.bounds.width + ui_badge->pad_h * 2,
        .height = ui_badge->ui_text.bounds.height + ui_badge->pad_v * 2};

    uint64_t text_x =
        pos_x +
        (ui_badge->bounds.width - ui_badge->ui_text.bounds.width) / 2.0f;
    uint64_t text_y =
        pos_y +
        (ui_badge->bounds.height - ui_badge->ui_text.bounds.height) / 2.0f;

    ui_text_update(&ui_badge->ui_text, NULL, &text_x, &text_y);

    if (!active || ui_badge->active == *active)
        return;

    ui_badge->active = *active;
    enum ColorIndex border_from, bg_from, text_from;
    enum ColorIndex border_to, bg_to, text_to;

    if (*active) {
        border_from = COL_SUBTLE;
        bg_from = COL_BG_ALT;
        text_from = COL_FG_ALT;
        border_to = COL_FG;
        bg_to = COL_SUBTLE;
        text_to = COL_FG;
    } else {
        border_from = COL_FG;
        bg_from = COL_SUBTLE;
        text_from = COL_FG;
        border_to = COL_SUBTLE;
        bg_to = COL_BG_ALT;
        text_to = COL_FG_ALT;
    }

    ui_badge->border_col = border_to;
    anim_set_lerp(&ui_badge->border_anim, BADGE_STATE_LERP_SPEED, border_from,
                  border_to);
    ui_badge->bg_col = bg_to;
    anim_set_lerp(&ui_badge->bg_anim, BADGE_STATE_LERP_SPEED, bg_from, bg_to);
    ui_badge->ui_text.color = text_to;
    anim_set_lerp(&ui_badge->ui_text.anim, BADGE_STATE_LERP_SPEED, text_from,
                  text_to);
}

static void ui_keyinfo_update(struct UIKeyInfo *ui_key_info,
                              const char *key_text, const char *desc_text,
                              uint64_t *x, uint64_t *y) {

    ui_badge_update(&ui_key_info->ui_badge, key_text, x, y, NULL);
    ui_text_update(&ui_key_info->ui_text, desc_text, NULL, NULL);

    uint64_t desc_x = ui_key_info->ui_badge.bounds.x +
                      ui_key_info->ui_badge.bounds.width +
                      ui_key_info->ui_text.font_size;
    uint64_t desc_y =
        ui_key_info->ui_badge.bounds.y + (ui_key_info->ui_badge.bounds.height -
                                          ui_key_info->ui_text.bounds.height) /
                                             2.0f;

    ui_text_update(&ui_key_info->ui_text, NULL, &desc_x, &desc_y);
}

/* Fns to draw primitive ui elements */

// Draw a rect applying the given animation properties.
static void ui_rect_draw(Rectangle rect, enum ColorIndex base_color,
                         bool outline, const struct Anim *anim) {
    Color col = index_to_color(base_color);
    Rectangle drawn_rect = rect;

    if (anim && anim->type != ANIM_NONE) {
        if (anim->type == ANIM_TRANSLATE) {
            Vector2 anim_pos =
                Vector2Lerp(anim->move_from, anim->move_to, anim->progress);
            drawn_rect.x = anim_pos.x;
            drawn_rect.y = anim_pos.y;
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

static void ui_divider_draw(const struct UIDivider *ui_divider) {
    if (!ui_divider)
        return;

    ui_rect_draw(ui_divider->bounds, ui_divider->color, false, NULL);
}

static void ui_text_draw(struct UIText *ui_text, float dt) {
    if (!ui_text)
        return;

    Color col = index_to_color(ui_text->color);
    if (ui_text->anim.type == ANIM_LERP) {
        col = ColorLerp(index_to_color(ui_text->anim.lerp_from),
                        index_to_color(ui_text->anim.lerp_to),
                        ui_text->anim.progress);
    }
    DrawTextEx(*ui_text->font, ui_text->text,
               (Vector2){ui_text->bounds.x, ui_text->bounds.y},
               ui_text->font_size, ui_text->font_spacing, col);
    anim_update(&ui_text->anim, dt);
}

static void ui_popup_draw(struct UIPopup *ui_popup, float dt) {
    if (!ui_popup || !ui_popup->active)
        return;

    if (ui_popup->ui_text.anim.type == ANIM_NONE) {
        anim_set_lerp(&ui_popup->ui_text.anim, POPUP_FADE_IN_SPEED, COL_BG,
                      ui_popup->ui_text.color);
    }

    ui_text_draw(&ui_popup->ui_text, dt);

    if (ui_popup->ui_text.anim.type == ANIM_NONE) {
        if (ui_popup->fading_out) {
            ui_popup->active = false;
            ui_popup->fading_out = false;
            return;
        } else {
            anim_set_lerp(&ui_popup->ui_text.anim, POPUP_FADE_OUT_SPEED,
                          ui_popup->ui_text.color, COL_BG);
            ui_popup->fading_out = true;
        }
    }
}

static void ui_badge_draw(struct UIBadge *ui_badge, float dt) {
    if (!ui_badge)
        return;

    ui_rect_draw(ui_badge->bounds, ui_badge->bg_col, false, &ui_badge->bg_anim);
    ui_rect_draw(ui_badge->bounds, ui_badge->border_col, true,
                 &ui_badge->border_anim);
    ui_text_draw(&ui_badge->ui_text, dt);

    anim_update(&ui_badge->border_anim, dt);
    anim_update(&ui_badge->bg_anim, dt);
}

/* Caching utilities */

/* UI elements store pixel coordinates (position) in them.
 * Following fns are responsible for deciding where ui elements should go
 * by setting their position.
 * As the window size is fixed, there is no point in recomputing the same
 * pixel coordinates every frame. So they are computed once in the
 * `renderer_init` fn. Drawing utilities just draws using already cached
 * position.
 * */

static void header_cache(void) {
    uint64_t hdr_title_w = txt_title_accent.bounds.width +
                           txt_title_dim.bounds.width +
                           txt_title_accent.font_spacing;
    uint64_t hdr_title_h = txt_title_accent.bounds.height;

    uint64_t hdr_title_x = HDR_CON_X + (HDR_CON_W - hdr_title_w) / 2;
    uint64_t hdr_title_y = HDR_CON_Y + (HDR_CON_H - hdr_title_h) / 2;

    ui_text_update(&txt_title_accent, NULL, &hdr_title_x, &hdr_title_y);

    uint64_t dim_x = hdr_title_x + txt_title_accent.bounds.width +
                     txt_title_accent.font_spacing;

    ui_text_update(&txt_title_dim, NULL, &dim_x, &hdr_title_y);
}

static uint64_t sb_score_cache(uint64_t x, uint64_t y) {
    ui_text_update(&txt_lbl_score, NULL, &x, &y);

    uint64_t best_lbl_x =
        SB_CON_X + SB_CON_W - SB_CON_PAD - txt_lbl_best.bounds.width;
    ui_text_update(&txt_lbl_best, NULL, &best_lbl_x, &y);

    y += txt_lbl_score.bounds.height + SB_LABEL_MG;

    ui_text_update(&txt_score, NULL, &x, &y);

    uint64_t best_val_x =
        SB_CON_X + SB_CON_W - SB_CON_PAD - txt_hs.bounds.width;

    ui_text_update(&txt_hs, NULL, &best_val_x, &y);

    y += txt_hs.bounds.height + SB_SEC_PAD;

    ui_divider_update(&div_sb_1, x, y, W - SB_CON_PAD, y);

    return y + 1;
}

static uint64_t sb_level_lines_cache(uint64_t x, uint64_t y) {
    y += SB_SEC_PAD;

    uint64_t sb_cw = SB_CON_W - SB_CON_PAD * 2;
    uint64_t split = x + sb_cw / 2;
    uint64_t lines_x = split + SB_CON_PAD;

    ui_text_update(&txt_lbl_level, NULL, &x, &y);
    ui_text_update(&txt_lbl_lines, NULL, &lines_x, &y);

    y += txt_lbl_level.bounds.height + SB_LABEL_MG;
    ui_text_update(&txt_level, NULL, &x, &y);
    ui_text_update(&txt_lines, NULL, &lines_x, &y);

    ui_divider_update(&div_sb_level_lines, split,
                      y - txt_lbl_level.bounds.height - SB_LABEL_MG, split,
                      y + txt_level.bounds.height);

    y += txt_level.bounds.height + SB_SEC_PAD;

    ui_divider_update(&div_sb_2, x, y, W - SB_CON_PAD, y);

    return y + 1;
}

static uint64_t sb_next_cache(uint64_t x, uint64_t y) {
    y += SB_SEC_PAD;

    ui_text_update(&txt_lbl_next, NULL, &x, &y);

    y += txt_lbl_next.bounds.height + SB_LABEL_MG;

    NEXT_GRID_CX = x;
    NEXT_GRID_CY = y;

    y += NEXT_GRID_SIZE * NEXT_CELL + SB_SEC_PAD;
    ui_divider_update(&div_sb_3, x, y, W - SB_CON_PAD, y);

    return y + 1;
}

static uint64_t sb_controls_cache(uint64_t x, uint64_t y) {
    y += SB_SEC_PAD;

    ui_text_update(&txt_lbl_controls, NULL, &x, &y);
    y += txt_lbl_controls.bounds.height + SB_LABEL_MG;

    for (uint64_t i = 0; i < KB_COUNT; i++) {
        ui_keyinfo_update(&ui_key_info_list[i], keyboard_key_names[i],
                          kb_desc[i], &x, &y);
        y += ui_key_info_list[i].ui_badge.bounds.height + SB_KEYINFO_MG;
    }

    return y - SB_KEYINFO_MG + SB_SEC_PAD;
}

static void credit_cache(void) {
    uint64_t credit_x =
        SB_CON_X + SB_CON_W / 2 - (uint64_t)txt_credit.bounds.width / 2;
    uint64_t credit_y =
        SB_CON_Y + SB_CON_H - txt_credit.bounds.height - SB_CON_PAD;
    ui_text_update(&txt_credit, NULL, &credit_x, &credit_y);
}

static void ui_elements_cache(void) {
    ui_divider_update(&div_hdr_bottom, HDR_CON_X, HDR_CON_Y + HDR_CON_H,
                      HDR_CON_X + HDR_CON_W, HDR_CON_Y + HDR_CON_H);
    ui_divider_update(&div_sb_left, SB_CON_X, SB_CON_Y, SB_CON_X,
                      SB_CON_Y + SB_CON_H);

    header_cache();

    // Sidebar content origin.
    uint64_t sb_cx = SB_CON_X + SB_CON_PAD;
    uint64_t sb_cy = SB_CON_Y + SB_CON_PAD;

    sb_cy = sb_score_cache(sb_cx, sb_cy);
    sb_cy = sb_level_lines_cache(sb_cx, sb_cy);
    sb_cy = sb_next_cache(sb_cx, sb_cy);
    sb_cy = sb_controls_cache(sb_cx, sb_cy);

    credit_cache();
}

/* Draw UI components */

static void ui_main_grid_draw(float dt) {
    Rectangle border_rect = {MAIN_GRID_CX, MAIN_GRID_CY, MAIN_CELL * COLS,
                             MAIN_CELL * ROWS};
    ui_rect_draw(border_rect, COL_SUBTLE, true, NULL);

    bool still_animating = false;
    for (uint8_t r = 0; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS; c++) {
            Rectangle cell_rect = {
                MAIN_GRID_CX + c * MAIN_CELL + MAIN_CELL_PAD,
                MAIN_GRID_CY + r * MAIN_CELL + MAIN_CELL_PAD,
                MAIN_CELL - MAIN_CELL_PAD * 2,
                MAIN_CELL - MAIN_CELL_PAD * 2,
            };
            ui_rect_draw(cell_rect, COL_BG_ALT, false, NULL);

            // Draw the cell if it isnt empty or has an active animation.
            if (ui_main_grid[r][c].type != N ||
                ui_main_grid[r][c].anim.type != ANIM_NONE) {
                ui_rect_draw(cell_rect, piece_color(ui_main_grid[r][c].type),
                             false, &ui_main_grid[r][c].anim);
                anim_update(&ui_main_grid[r][c].anim, dt);
            }

            still_animating =
                still_animating || (ui_main_grid[r][c].anim.type != ANIM_NONE);
        }
    }
    ui_main_grid_animating = still_animating;
}

static void ui_shape_draw(struct UIShape *ui_shape, enum ColorIndex col_ind,
                          uint64_t origin_x, uint64_t origin_y,
                          uint64_t cell_size, uint64_t cell_pad, float dt) {
    if (!ui_shape || ui_shape->shape.type == N)
        return;

    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        uint8_t gc = ui_shape->shape.pos.x + ui_shape->shape.offsets[i].x;
        uint8_t gr = ui_shape->shape.pos.y + ui_shape->shape.offsets[i].y;
        Rectangle r = {
            origin_x + gc * cell_size + cell_pad,
            origin_y + gr * cell_size + cell_pad,
            cell_size - cell_pad * 2,
            cell_size - cell_pad * 2,
        };
        ui_rect_draw(r, col_ind, false, &ui_shape->anim);
    }
    anim_update(&ui_shape->anim, dt);
}

static void header_draw(float dt) {
    ui_text_draw(&txt_title_accent, dt);
    ui_text_draw(&txt_title_dim, dt);
}

static void sb_score_draw(float dt) {
    ui_text_draw(&txt_lbl_score, dt);
    ui_text_draw(&txt_lbl_best, dt);
    ui_text_draw(&txt_score, dt);
    ui_text_draw(&txt_hs, dt);

    ui_divider_draw(&div_sb_1);
}

static void sb_level_lines_draw(float dt) {
    ui_text_draw(&txt_lbl_level, dt);
    ui_text_draw(&txt_lbl_lines, dt);
    ui_text_draw(&txt_level, dt);
    ui_text_draw(&txt_lines, dt);

    ui_divider_draw(&div_sb_level_lines);
    ui_divider_draw(&div_sb_2);
}

static void sb_next_draw(float dt) {
    ui_text_draw(&txt_lbl_next, dt);

    uint64_t next_grid_dim = NEXT_GRID_SIZE * NEXT_CELL;
    Rectangle next_grid_rect = {NEXT_GRID_CX, NEXT_GRID_CY, next_grid_dim,
                                next_grid_dim};
    ui_rect_draw(next_grid_rect, COL_SUBTLE, true, NULL);

    // Draw next grid.
    for (uint8_t r = 0; r < NEXT_GRID_SIZE; r++) {
        for (uint8_t c = 0; c < NEXT_GRID_SIZE; c++) {
            Rectangle cell_r = {
                NEXT_GRID_CX + c * NEXT_CELL + NEXT_CELL_PAD,
                NEXT_GRID_CY + r * NEXT_CELL + NEXT_CELL_PAD,
                NEXT_CELL - NEXT_CELL_PAD * 2,
                NEXT_CELL - NEXT_CELL_PAD * 2,
            };
            ui_rect_draw(cell_r, COL_BG_ALT, false, NULL);
        }
    }

    // Draw next shape.
    ui_shape_draw(&player_next_shape, piece_color(player_next_shape.shape.type),
                  NEXT_GRID_CX, NEXT_GRID_CY, NEXT_CELL, NEXT_CELL_PAD, dt);

    ui_divider_draw(&div_sb_3);
}

static void sb_controls_draw(float dt) {
    ui_text_draw(&txt_lbl_controls, dt);

    for (uint8_t i = 0; i < KB_COUNT; i++) {
        ui_badge_draw(&ui_key_info_list[i].ui_badge, dt);
        ui_text_draw(&ui_key_info_list[i].ui_text, dt);
    }
}

static void credit_draw(void) { ui_text_draw(&txt_credit, 0.0f); }

/* Hard drop trail */

// Sets up hard drop trail rectangles for the given shape and path.
static void hard_drop_trail_set(struct Shape shape, struct Coord from,
                                struct Coord to) {
    /* The objective here is to use 2 arrays of size OFFSETS_COUNT.
     * One stores main grid coordinates of the given shape's top most cells
     * when it is at `from`. The other stores main grid coordinates of the given
     * shape's top most cells when it is at `to`. Then rectangles are drawn
     * bw them. The number of rectangles to be drawn are the number of
     * top most cells that shape has in that particular orientation.
     * For example: The O shape only requires just 2 rectangles in all
     * orientation.
     * */

    trail_rect_count = 0;

    // Computes minimum x, to be used as index for the first cell in the 2
    // arrays mentioned above.
    uint8_t min_x = shape.offsets[0].x + shape.pos.x;
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        uint8_t new_x = shape.offsets[i].x + from.x;
        if (new_x < min_x)
            min_x = new_x;
    }

    // As the shapes are continuous, the second cell is just
    // min_x + 1, the fourth cell would be min_x + 3.
    struct Coord starts[OFFSETS_COUNT];
    struct Coord ends[OFFSETS_COUNT];
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        starts[i] = (struct Coord){-1, -1};
        ends[i] = (struct Coord){-1, -1};
    }

    // Filling the 2 arrays above.
    // Find min y for a given x for all x the shape occupies, this gives the top
    // most y. Do it for both `from` and `to` and store them in `starts` and
    // `ends`.
    for (uint8_t i = 0; i < OFFSETS_COUNT; i++) {
        uint8_t x = shape.offsets[i].x + from.x;
        uint8_t yf = shape.offsets[i].y + from.y;
        uint8_t yt = shape.offsets[i].y + to.y;
        uint8_t ind = x - min_x; // Computing correct index with min_x.

        if (starts[ind].x == -1) {
            starts[ind] = (struct Coord){x, yf};
            ends[ind] = (struct Coord){x, yt};
            trail_rect_count++;
            continue;
        }

        if (yf < starts[ind].y)
            starts[ind].y = yf;
        if (yt < ends[ind].y)
            ends[ind].y = yt;
    }

    // Setting up the rectangles bw `starts` and `ends` (The trail).
    for (uint8_t i = 0; i < trail_rect_count; i++) {
        float rx = MAIN_GRID_CX + starts[i].x * MAIN_CELL;
        float ry = MAIN_GRID_CY + starts[i].y * MAIN_CELL;
        float ry2 = MAIN_GRID_CY + ends[i].y * MAIN_CELL;
        trail_rects[i] = (Rectangle){rx, ry, MAIN_CELL, ry2 - ry};
    }

    // Setting animation.
    anim_set_lerp(&trail_anim, TRAIL_FADE_OUT_SPEED, COL_NEUTRAL, COL_NEUTRAL);
}

// Draw the hard drop trail if active.
static void hard_drop_trail_draw(float dt) {
    if (trail_anim.type == ANIM_NONE)
        return;

    for (uint8_t i = 0; i < trail_rect_count; i++) {
        ui_rect_draw(trail_rects[i], COL_NEUTRAL, false, &trail_anim);
    }

    anim_update(&trail_anim, dt);
}

/* Event handler */

static void score_popup_show(unsigned int delta_score) {
    if (delta_score == 0)
        return;
    char buf[TEXT_BUFF_LEN];
    snprintf(buf, sizeof(buf), "+%u", delta_score);
    uint64_t x = txt_score.bounds.x + txt_score.bounds.width +
                 pp_score.ui_text.bounds.height * 0.5f;
    uint64_t y =
        txt_score.bounds.y +
        ((txt_score.bounds.height - pp_score.ui_text.bounds.height) / 2);
    ui_popup_update(&pp_score, buf, &x, &y, &(bool){true});
}

static void combo_popup_show(unsigned int combo_count,
                             unsigned int combo_points) {
    if (combo_count == 0 || combo_points == 0)
        return;
    char buf[TEXT_BUFF_LEN];
    snprintf(buf, sizeof(buf), "x%u +%u", combo_count, combo_points);
    uint64_t x = txt_score.bounds.x + txt_score.bounds.width +
                 pp_combo.ui_text.bounds.height * 0.5f;
    uint64_t y =
        txt_score.bounds.y +
        ((txt_score.bounds.height - pp_score.ui_text.bounds.height) / 2);
    if (pp_score.active)
        x += pp_score.ui_text.bounds.width +
             pp_combo.ui_text.bounds.height * 0.5f;
    ui_popup_update(&pp_combo, buf, &x, &y, &(bool){true});
}

static bool event_new_shape_handle(struct CTetrisEvent ev) {
    player_active_shape.shape = ev.shape;
    player_shadow_shape.shape = ctetris_shape_proj_get();
    player_next_shape.shape = ctetris_shape_next_get();

    // If the engine went inactive with the new shape, meaning game ended:
    if (ev.engine_inactive) {
        game_over = true;
        game_over_t = 0.0f;
        return false;
    }

    // Center next shape inside next grid.
    struct Shape *s = &player_next_shape.shape;
    int8_t min_x = s->offsets[0].x, max_x = s->offsets[0].x;
    int8_t min_y = s->offsets[0].y, max_y = s->offsets[0].y;
    for (uint8_t i = 1; i < OFFSETS_COUNT; i++) {
        if (s->offsets[i].x < min_x)
            min_x = s->offsets[i].x;
        if (s->offsets[i].x > max_x)
            max_x = s->offsets[i].x;
        if (s->offsets[i].y < min_y)
            min_y = s->offsets[i].y;
        if (s->offsets[i].y > max_y)
            max_y = s->offsets[i].y;
    }
    s->pos.x = (NEXT_GRID_SIZE - (max_x - min_x + 1)) / 2 - min_x;
    s->pos.y = (NEXT_GRID_SIZE - (max_y - min_y + 1)) / 2 - min_y;

    // Set shape fade in animations.
    anim_set_lerp(&player_active_shape.anim, SHAPE_FADE_IN_SPEED, COL_BG_ALT,
                  piece_color(player_active_shape.shape.type));
    anim_set_lerp(&player_shadow_shape.anim, SHAPE_FADE_IN_SPEED, COL_BG_ALT,
                  COL_NEUTRAL);
    anim_set_lerp(&player_next_shape.anim, SHAPE_FADE_IN_SPEED, COL_BG_ALT,
                  piece_color(player_next_shape.shape.type));

    return true;
}

static void event_soft_drop_handle(struct CTetrisEvent ev) {
    player_active_shape.shape = ev.shape;
    uint32_t delta = ev.score - score;
    score = ev.score;

    if (score > high_score)
        renderer_high_score_set(score);

    char buf[TEXT_BUFF_LEN];
    snprintf(buf, sizeof(buf), "%u", score);
    ui_text_update(&txt_score, buf, NULL, NULL);

    score_popup_show(delta);

    if (!audio_muted)
        PlaySound(sfx_clack);
}

static void event_shift_rotate_handle(struct CTetrisEvent ev) {
    player_active_shape.shape = ev.shape;
    player_shadow_shape.shape = ctetris_shape_proj_get();

    if (!audio_muted)
        PlaySound(sfx_clack);
}

static bool event_hard_drop_handle(struct CTetrisEvent ev) {
    uint32_t delta = ev.score - score;
    score = ev.score;

    if (score > high_score)
        renderer_high_score_set(score);

    char buf[TEXT_BUFF_LEN];
    snprintf(buf, sizeof(buf), "%u", score);
    ui_text_update(&txt_score, buf, NULL, NULL);

    score_popup_show(delta);
    hard_drop_trail_set(player_active_shape.shape,
                        player_active_shape.shape.pos,
                        player_shadow_shape.shape.pos);

    player_active_shape.shape = ev.shape;

    if (!audio_muted)
        PlaySound(sfx_thud);

    return false;
}

static bool event_line_clear_handle(struct CTetrisEvent ev) {
    uint8_t combo_points = CALC_COMBO_POINTS(level, ev.combo);
    uint32_t score_delta = ev.score - score;
    score = ev.score;
    level = ev.level;
    lines = ev.lines;

    if (score > high_score)
        renderer_high_score_set(score);

    char buf[TEXT_BUFF_LEN];
    snprintf(buf, sizeof(buf), "%u", score);
    ui_text_update(&txt_score, buf, NULL, NULL);
    snprintf(buf, sizeof(buf), "%u", level);
    ui_text_update(&txt_level, buf, NULL, NULL);
    snprintf(buf, sizeof(buf), "%u", lines);
    ui_text_update(&txt_lines, buf, NULL, NULL);

    // Score / Combo popups
    score_popup_show(score_delta - combo_points);
    combo_popup_show(ev.combo, combo_points);

    // Fade out cleared lines
    for (uint8_t i = 0; i < ev.lines_cleared_count; i++) {
        uint8_t row = ev.lines_cleared_indices[i];
        for (uint8_t c = 0; c < COLS; c++) {
            anim_set_lerp(&ui_main_grid[row][c].anim, LINE_FADE_OUT_SPEED,
                          piece_color(ui_main_grid[row][c].type), COL_BG_ALT);
            ui_main_grid[row][c].type = N;
        }
    }

    // Store indices of rows that has to move after line clear and to where.
    // How many cleared lines a row has below it is exactly the number of rows
    // it should move down.
    line_move_count = 0;
    for (int8_t src = ROWS - 1; src >= 0; src--) {
        uint8_t drop_distance = 0;
        for (uint8_t i = 0; i < ev.lines_cleared_count; i++) {
            if (ev.lines_cleared_indices[i] > src)
                drop_distance++;
        }
        if (drop_distance > 0) {
            bool empty = true;
            for (uint8_t c = 0; c < COLS; c++) {
                if (ui_main_grid[src][c].type != N) {
                    empty = false;
                    break;
                }
            }
            if (!empty) {
                uint8_t dst = src + drop_distance;
                line_moves[line_move_count++] = (struct Coord){src, dst};
            }
        }
    }
    line_move_pending = true;

    return false;
}

// Handle the given engine event. returns false if the engine should be
// blocked.
// Main event handler.
static bool event_handle(struct CTetrisEvent ev) {
    switch (ev.type) {
    case CTETRIS_EVENT_NEW_SHAPE:
        return event_new_shape_handle(ev);

    case CTETRIS_EVENT_DROP:
        player_active_shape.shape = ev.shape;
        break;

    case CTETRIS_EVENT_SOFT_DROP:
        event_soft_drop_handle(ev);
        break;

    case CTETRIS_EVENT_SHIFT:
    case CTETRIS_EVENT_ROTATE:
        event_shift_rotate_handle(ev);
        break;

    case CTETRIS_EVENT_HARD_DROP:
        return event_hard_drop_handle(ev);

    case CTETRIS_EVENT_LOCK_START:
    case CTETRIS_EVENT_LOCK_RESET:
        anim_set_lerp(&player_active_shape.anim, SHAPE_LOCK_FADE_OUT_SPEED,
                      piece_color(player_active_shape.shape.type), COL_NEUTRAL);
        break;

    case CTETRIS_EVENT_LOCK_CANCEL:
        anim_set_none(&player_active_shape.anim);
        break;

    case CTETRIS_EVENT_LOCK_DONE:
        if (!audio_muted)
            PlaySound(sfx_click);
        anim_set_flash(&player_active_shape.anim, SHAPE_FLASH_SPEED);
        ui_main_grid_write_pending = true;
        return false;

    case CTETRIS_EVENT_LINE_CLEAR:
        return event_line_clear_handle(ev);

    default:
        break;
    }

    return true;
}

/* Asset management */

static void assets_load(void) {
    font_sm =
        LoadFontFromMemory(".otf", FONT_OTF, FONT_OTF_LEN, FONT_SM, NULL, 0);
    font_md =
        LoadFontFromMemory(".otf", FONT_OTF, FONT_OTF_LEN, FONT_MD, NULL, 0);
    font_lg =
        LoadFontFromMemory(".otf", FONT_OTF, FONT_OTF_LEN, FONT_LG, NULL, 0);

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

static void assets_unload(void) {
    UnloadFont(font_sm);
    UnloadFont(font_md);
    UnloadFont(font_lg);
    UnloadSound(sfx_thud);
    UnloadSound(sfx_click);
    UnloadSound(sfx_clack);
}

/* UI Core */

static void ui_layout_compute(uint64_t window_w, uint64_t window_h) {
    W = window_w;
    H = window_h;

    FONT_LG = 0.04f * H;
    FONT_MD = 0.7f * FONT_LG;
    FONT_SM = 0.5f * FONT_LG;

    uint64_t split_x = window_w * SPLIT_AT_PCT;

    HDR_CON_X = 0;
    HDR_CON_Y = 0;
    HDR_CON_W = split_x;
    HDR_CON_H = TITLE_FONT_SIZE * 1.5;

    MAIN_GRID_CON_X = 0;
    MAIN_GRID_CON_Y = HDR_CON_H;
    MAIN_GRID_CON_W = split_x;
    MAIN_GRID_CON_H = window_h - HDR_CON_H;

    uint64_t cell_h = MAIN_GRID_CON_H / (ROWS + 2);
    uint64_t cell_w = MAIN_GRID_CON_W / COLS;
    MAIN_CELL = (cell_h < cell_w) ? cell_h : cell_w;
    MAIN_CELL_PAD = MAIN_CELL * 0.05f;
    if (MAIN_CELL_PAD < 1)
        MAIN_CELL_PAD = 1;

    NEXT_CELL = MAIN_CELL * 0.9f;
    NEXT_CELL_PAD = MAIN_CELL_PAD * 0.9f;
    if (NEXT_CELL_PAD < 1)
        NEXT_CELL_PAD = 1;

    MAIN_GRID_CX = MAIN_GRID_CON_X + (MAIN_GRID_CON_W - MAIN_CELL * COLS) / 2;
    MAIN_GRID_CY = MAIN_GRID_CON_Y + (MAIN_GRID_CON_H - MAIN_CELL * ROWS) / 2;

    SB_CON_X = split_x;
    SB_CON_Y = 0;
    SB_CON_W = window_w - split_x;
    SB_CON_H = window_h;

    SB_CON_PAD = 0.04f * SB_CON_W;

    SB_SEC_PAD = LABEL_FONT_SIZE;
    SB_LABEL_MG = LABEL_FONT_SIZE * 0.5;
    SB_KEYINFO_MG = SB_LABEL_MG * 0.5;
}

// Initialize all UI elements to a default state.
static void ui_elements_init(void) {
    // Dividers
    ui_divider_init(&div_hdr_bottom, COL_SUBTLE, 1);
    ui_divider_init(&div_sb_left, COL_SUBTLE, 1);
    ui_divider_init(&div_sb_1, COL_SUBTLE, 1);
    ui_divider_init(&div_sb_2, COL_SUBTLE, 1);
    ui_divider_init(&div_sb_3, COL_SUBTLE, 1);
    ui_divider_init(&div_sb_level_lines, COL_SUBTLE, 1);

    // Header text
    ui_text_init(&txt_title_accent, get_font(TITLE_FONT_SIZE), TITLE_FONT_SIZE,
                 COL_FG);
    ui_text_init(&txt_title_dim, get_font(TITLE_FONT_SIZE), TITLE_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_update(&txt_title_accent, TITLE_ACCENTED, NULL, NULL);
    ui_text_update(&txt_title_dim, TITLE_DIM, NULL, NULL);

    // Sidebar labels
    ui_text_init(&txt_lbl_score, get_font(LABEL_FONT_SIZE), LABEL_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_init(&txt_lbl_best, get_font(LABEL_FONT_SIZE), LABEL_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_init(&txt_lbl_level, get_font(LABEL_FONT_SIZE), LABEL_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_init(&txt_lbl_lines, get_font(LABEL_FONT_SIZE), LABEL_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_init(&txt_lbl_next, get_font(LABEL_FONT_SIZE), LABEL_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_init(&txt_lbl_controls, get_font(LABEL_FONT_SIZE), LABEL_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_update(&txt_lbl_score, "SCORE", NULL, NULL);
    ui_text_update(&txt_lbl_best, "BEST", NULL, NULL);
    ui_text_update(&txt_lbl_level, "LEVEL", NULL, NULL);
    ui_text_update(&txt_lbl_lines, "LINES", NULL, NULL);
    ui_text_update(&txt_lbl_next, "NEXT", NULL, NULL);
    ui_text_update(&txt_lbl_controls, "CONTROLS", NULL, NULL);

    // Sidebar numerics
    ui_text_init(&txt_score, get_font(NUMERIC_FONT_SIZE), NUMERIC_FONT_SIZE,
                 COL_FG);
    ui_text_init(&txt_hs, get_font(NUMERIC_FONT_SIZE), NUMERIC_FONT_SIZE,
                 COL_FG_ALT);
    ui_text_init(&txt_level, get_font(NUMERIC_FONT_SIZE), NUMERIC_FONT_SIZE,
                 COL_FG);
    ui_text_init(&txt_lines, get_font(NUMERIC_FONT_SIZE), NUMERIC_FONT_SIZE,
                 COL_FG);
    ui_text_update(&txt_score, "0", NULL, NULL);
    ui_text_update(&txt_hs, "0", NULL, NULL);
    ui_text_update(&txt_level, "1", NULL, NULL);
    ui_text_update(&txt_lines, "0", NULL, NULL);

    // Credit
    ui_text_init(&txt_credit, get_font(CREDIT_FONT_SIZE), CREDIT_FONT_SIZE,
                 COL_NEUTRAL);
    ui_text_update(&txt_credit, CREDIT_TEXT, NULL, NULL);

    // Popups
    ui_popup_init(&pp_score, get_font(POPUP_FONT_SIZE), POPUP_FONT_SIZE,
                  COL_GREEN);
    ui_popup_init(&pp_combo, get_font(POPUP_FONT_SIZE), POPUP_FONT_SIZE,
                  COL_YELLOW);

    ui_popup_update(&pp_score, " ", NULL, NULL, NULL);
    ui_popup_update(&pp_combo, " ", NULL, NULL, NULL);

    // Key bindings
    for (uint8_t i = 0; i < KB_COUNT; i++) {
        ui_keyinfo_init(&ui_key_info_list[i], get_font(BADGE_FONT_SIZE),
                        BADGE_FONT_SIZE);
        ui_keyinfo_update(&ui_key_info_list[i], keyboard_key_names[i],
                          kb_desc[i], NULL, NULL);
    }
}

// Initialize state variables, used on when a new game starts.
static void state_init(void) {
    ctetris_init();
    ui_main_grid_init();

    score = 0;
    level = 1;
    lines = 0;
    ui_text_update(&txt_score, "0", NULL, NULL);
    ui_text_update(&txt_level, "1", NULL, NULL);
    ui_text_update(&txt_lines, "0", NULL, NULL);

    ui_popup_update(&pp_score, NULL, NULL, NULL, &(bool){false});
    ui_popup_update(&pp_combo, NULL, NULL, NULL, &(bool){false});

    anim_set_none(&trail_anim);

    player_active_shape.shape.type = N;
    player_shadow_shape.shape.type = N;
    player_next_shape.shape.type = N;

    paused = false;
    ui_main_grid_write_pending = false;
    block_engine = false;
    line_move_pending = false;
    game_over = false;
    game_over_t = 0.0f;
}

// Initialize the renderer.
void renderer_init(void) {
    InitWindow(10, 10, "cTetris");

    // Dummy window to figure out dpi scale factor manually.
    uint64_t dummy_size = 500;
    SetWindowSize(dummy_size, dummy_size);
    float dpi_scale_factor = (float)GetRenderWidth() / dummy_size;

    uint64_t mon = GetCurrentMonitor();
    // Make the window size set to a certain % of that of the monitor height.
    // Window is a rectangle.
    uint64_t window_size = GetMonitorHeight(mon) * WH_TO_MH_PCT;

    uint64_t logical_window_size = window_size / dpi_scale_factor;

    // To get a window of `window_size` i need to ask raylib for a window of
    // `logical_window_size`.
    SetWindowSize(logical_window_size, logical_window_size);

    // Center the window.
    SetWindowPosition((GetMonitorWidth(mon) - window_size) / 2,
                      (GetMonitorHeight(mon) - window_size) / 2);

    InitAudioDevice();

    // Window decoration icon setup.
    Image icon = LoadImageFromMemory(".png", ICON_PNG, ICON_PNG_LEN);
    SetWindowIcon(icon);
    UnloadImage(icon);

    // Derive layout constraints from the now computed window dimensions.
    ui_layout_compute(window_size, window_size);

    SetTargetFPS(FPS);
    SetExitKey(KEY_NULL);

    // Load Font, Sound assets.
    assets_load();

    ui_elements_init();  // Initialize ui elements.
    ui_elements_cache(); // Compute where ui elements should go (position).

    state_init(); // Initialize the engine and game state variables.
}

void renderer_shutdown(void) {
    assets_unload();
    CloseWindow();
}

void renderer_high_score_set(uint32_t hs) {
    high_score = hs;
    char buf[TEXT_BUFF_LEN];
    snprintf(buf, sizeof(buf), "%u", high_score);
    ui_text_update(&txt_hs, buf, NULL, NULL);
    txt_hs.bounds.x = SB_CON_X + SB_CON_W - SB_CON_PAD - txt_hs.bounds.width;
}

uint32_t renderer_high_score_get(void) { return high_score; }

// Renderer's input handler.
bool renderer_input(void) {
    if (WindowShouldClose())
        return false;

    if (IsKeyPressed(KEY_P)) {
        paused = !paused;
        ui_badge_update(&ui_key_info_list[KB_PAUSE].ui_badge, NULL, NULL, NULL,
                        &paused);
    }

    if (IsKeyPressed(KEY_R)) {
        ui_badge_update(&ui_key_info_list[KB_RESTART].ui_badge, NULL, NULL,
                        NULL, &(bool){true});
        state_init();

        ui_badge_update(&ui_key_info_list[KB_PAUSE].ui_badge, NULL, NULL, NULL,
                        &paused);
        return true;
    } else {
        ui_badge_update(&ui_key_info_list[KB_RESTART].ui_badge, NULL, NULL,
                        NULL, &(bool){false});
    }

    if (IsKeyPressed(KEY_T)) {
        prev_scheme = current_scheme;
        current_scheme = (current_scheme + 1) % SCHEME_COUNT;
        theme_switch_t = 0.0f;
        ui_badge_update(&ui_key_info_list[KB_THEME].ui_badge, NULL, NULL, NULL,
                        &(bool){true});
    } else {
        ui_badge_update(&ui_key_info_list[KB_THEME].ui_badge, NULL, NULL, NULL,
                        &(bool){false});
    }

    if (IsKeyPressed(KEY_M)) {
        audio_muted = !audio_muted;
        ui_badge_update(&ui_key_info_list[KB_MUTE].ui_badge, NULL, NULL, NULL,
                        &audio_muted);
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
        .rotate_right_pressed = IsKeyPressed(KEY_UP),
        .rotate_left_pressed = IsKeyPressed(KEY_Z),
        .hard_drop_pressed = IsKeyPressed(KEY_SPACE),
    };
    ctetris_input_push(input_state);

    // Set key info badge state based on input.
    bool left = input_state.shift_left_pressed || input_state.shift_left_held;
    bool right =
        input_state.shift_right_pressed || input_state.shift_right_held;

    if (ui_key_info_list[KB_HARD_DROP].ui_badge.active !=
        input_state.hard_drop_pressed)
        ui_badge_update(&ui_key_info_list[KB_HARD_DROP].ui_badge, NULL, NULL,
                        NULL, &input_state.hard_drop_pressed);

    if (ui_key_info_list[KB_MOVE_LEFT].ui_badge.active != left)
        ui_badge_update(&ui_key_info_list[KB_MOVE_LEFT].ui_badge, NULL, NULL,
                        NULL, &left);

    if (ui_key_info_list[KB_MOVE_RIGHT].ui_badge.active != right)
        ui_badge_update(&ui_key_info_list[KB_MOVE_RIGHT].ui_badge, NULL, NULL,
                        NULL, &right);

    if (ui_key_info_list[KB_SOFT_DROP].ui_badge.active !=
        input_state.soft_drop_held)
        ui_badge_update(&ui_key_info_list[KB_SOFT_DROP].ui_badge, NULL, NULL,
                        NULL, &input_state.soft_drop_held);

    if (ui_key_info_list[KB_ROTATE_RIGHT].ui_badge.active !=
        input_state.rotate_right_pressed)
        ui_badge_update(&ui_key_info_list[KB_ROTATE_RIGHT].ui_badge, NULL, NULL,
                        NULL, &input_state.rotate_right_pressed);

    if (ui_key_info_list[KB_ROTATE_LEFT].ui_badge.active !=
        input_state.rotate_left_pressed)
        ui_badge_update(&ui_key_info_list[KB_ROTATE_LEFT].ui_badge, NULL, NULL,
                        NULL, &input_state.rotate_left_pressed);

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

    // In case the engine is blocked for playing animations / mutating main ui
    // grid then unblock once they are finished.
    if (block_engine) {
        if (ui_main_grid_animating ||
            (player_active_shape.anim.type != ANIM_NONE) ||
            (trail_anim.type != ANIM_NONE))
            return;

        if (line_move_pending) {
            for (uint8_t i = 0; i < line_move_count; i++) {
                uint8_t src = line_moves[i].x;
                uint8_t dst = line_moves[i].y;
                for (uint8_t c = 0; c < COLS; c++) {
                    if (ui_main_grid[src][c].type == N)
                        continue;
                    ui_main_grid[dst][c] = ui_main_grid[src][c];
                    uint64_t x = MAIN_GRID_CX + MAIN_CELL * c + MAIN_CELL_PAD;
                    uint64_t from_y = MAIN_GRID_CY + src * MAIN_CELL;
                    uint64_t to_y = MAIN_GRID_CY + dst * MAIN_CELL;
                    anim_set_translate(&ui_main_grid[dst][c].anim,
                                       LINE_MOVE_SPEED, (Vector2){x, from_y},
                                       (Vector2){x, to_y});
                    ui_main_grid[src][c].type = N;
                }
            }
            line_move_pending = false;
            return;
        }

        if (ui_main_grid_write_pending) {
            ui_main_grid_shape_write(player_active_shape.shape);
            ui_main_grid_write_pending = false;
            // Once the shape is locked then dont draw it and its shadow until a
            // new shape is spawned.
            player_active_shape.shape.type = N;
            player_shadow_shape.shape.type = N;
        }

        block_engine = false;
    }

    struct CTetrisEvent ev;
    while ((ev = ctetris_event_pop()).type != CTETRIS_EVENT_NONE) {
        if (!event_handle(ev)) {
            block_engine = true;
            return;
        }
    }

    // The engine is only stepped forward when it isnt blocked and
    // all buffered events are processed.
    ctetris_update(delta);
}

// Render everything.
void renderer_render(void) {
    float dt = GetFrameTime();

    BeginDrawing();

    ClearBackground(index_to_color(COL_BG));

    header_draw(dt);

    ui_divider_draw(&div_hdr_bottom);
    ui_divider_draw(&div_sb_left);

    ui_main_grid_draw(dt);

    ui_shape_draw(&player_shadow_shape, COL_NEUTRAL, MAIN_GRID_CX, MAIN_GRID_CY,
                  MAIN_CELL, MAIN_CELL_PAD, dt);
    ui_shape_draw(&player_active_shape,
                  piece_color(player_active_shape.shape.type), MAIN_GRID_CX,
                  MAIN_GRID_CY, MAIN_CELL, MAIN_CELL_PAD, dt);

    hard_drop_trail_draw(dt);

    sb_score_draw(dt);
    sb_level_lines_draw(dt);
    sb_next_draw(dt);
    sb_controls_draw(dt);

    ui_popup_draw(&pp_score, dt);
    ui_popup_draw(&pp_combo, dt);

    credit_draw();

    EndDrawing();

    // Theme switch and game over lerp animation update.
    if (theme_switch_t < 1.0f) {
        theme_switch_t += dt * THEME_LERP_SPEED;
        if (theme_switch_t > 1.0f)
            theme_switch_t = 1.0f;
    }

    if (game_over && game_over_t < 1.0f) {
        game_over_t += dt * GAMEOVER_LERP_SPEED;
        if (game_over_t > 1.0f)
            game_over_t = 1.0f;
    }
}

/* [ END ] */
