#include "include/ui/play_screen.h"
#include "include/domain/avocado_rules.h"
#include "include/platform/feedback_helper.h"
#include "include/platform/storage_helper.h"
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
struct PlayScreen {
    View *view;
    AvocadoData *data;
    AvocadoFeedback *feedback;
};

typedef struct {
    PlayScreen *screen;
} PlayViewModel;

/**
 * Tumbler: rim only slightly wider than the pit (r=9 → Ø18; mouth Ø22 → 2 px gap/side).
 * Base a bit narrower so it still reads as a glass.
 */
enum {
    CupCx = 64,
    CupY = 18,
    /* Shorter glass (crop bottom) so pit can sit higher and still fit HUD/roots. */
    CupH = 32,
    CupTopY = CupY + 2,
    CupBotY = CupY + CupH - 2,
    CupTopHalfW = 11,
    CupBotHalfW = 9,
};

static void play_on_action(PlayScreen *screen);

/** Left/right inner x at row y for the trapezoid glass (inclusive edges are the wall). */
static void glass_horizontal_at(int cx, int top_y, int bot_y, int top_half_w, int bot_half_w, int y,
                                int *out_l, int *out_r) {
    const int tl = cx - top_half_w;
    const int tr = cx + top_half_w;
    const int bl = cx - bot_half_w;
    const int br = cx + bot_half_w;
    if (y <= top_y) {
        *out_l = tl;
        *out_r = tr;
        return;
    }
    if (y >= bot_y) {
        *out_l = bl;
        *out_r = br;
        return;
    }
    const int dy = bot_y - top_y;
    const int n = y - top_y;
    *out_l = tl + (bl - tl) * n / dy;
    *out_r = tr + (br - tr) * n / dy;
}

static void cup_horizontal_at(int y, int *out_l, int *out_r) {
    glass_horizontal_at(CupCx, CupTopY, CupBotY, CupTopHalfW, CupBotHalfW, y, out_l, out_r);
}

/**
 * Normal play: water fills the cup to the rim. Clear water ≈ 50% checker; as
 * dirty_level rises, more “off” cells fill in → visibly murky toward game over.
 */
static void draw_water_dither_full_cup(Canvas *canvas, uint8_t dirty_level) {
    canvas_set_color(canvas, ColorBlack);
    int l = 0;
    int r = 0;
    const unsigned murk = (unsigned)dirty_level * 128u / (unsigned)AVOCADO_GRIME_GAME_OVER;
    for (int y = CupTopY + 1; y < CupBotY; y++) {
        cup_horizontal_at(y, &l, &r);
        for (int x = l + 1; x < r; x++) {
            const int checker = (x + y) & 1;
            const unsigned h = (unsigned)((x * 31 + y * 17) & 127);
            if (checker == 0 || murk > h) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

/** Game over: low puddle + surface line (drought). */
static void draw_water_dither(Canvas *canvas, int y_surface) {
    if (y_surface >= CupBotY) {
        return;
    }
    canvas_set_color(canvas, ColorBlack);
    int l = 0;
    int r = 0;
    cup_horizontal_at(y_surface, &l, &r);
    canvas_draw_line(canvas, l, y_surface, r, y_surface);
    for (int y = y_surface + 1; y < CupBotY; y++) {
        cup_horizontal_at(y, &l, &r);
        for (int x = l + 1; x < r; x++) {
            if (((x + y) & 1) == 0) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

/** Avocado pit: no face; optional crack when dried (game over). */
static void draw_pit(Canvas *canvas, int cx, int cy, size_t radius, bool cracked) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, cx, cy, radius);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, cx - 3, cy - 3, 2);
    canvas_set_color(canvas, ColorBlack);
    if (cracked) {
        canvas_draw_line(canvas, cx - (int)radius + 2, cy - 2, cx + (int)radius - 2, cy + 2);
    }
}

static void draw_roots(Canvas *canvas, int cx, int y_start, uint8_t roots) {
    if (roots == 0) {
        return;
    }
    const int jar_bottom = CupBotY;
    canvas_set_color(canvas, ColorBlack);
    /* Taproot length scales with level so growth is visible on screen. */
    const int tap_len = 5 + (int)roots * 2;
    int y_tip = y_start + tap_len;
    if (y_tip > jar_bottom) {
        y_tip = jar_bottom;
    }
    canvas_draw_line(canvas, cx, y_start, cx, y_tip);

    if (roots == 1u) {
        /* Tiny forks so level 1 is still visible. */
        canvas_draw_line(canvas, cx - 3, y_start + 2, cx, y_start + 5);
        canvas_draw_line(canvas, cx + 3, y_start + 2, cx, y_start + 5);
        return;
    }

    /* Lateral roots: more strands and wider spread as roots grow. */
    for (uint8_t n = 0; n < roots; n++) {
        const int dx = -10 + (20 * (int)n) / ((int)roots - 1);
        const int y_mid = y_start + 3 + (int)n * 2;
        if (y_mid > jar_bottom - 2) {
            break;
        }
        canvas_draw_line(canvas, cx, y_start + 1, cx + dx / 2, y_mid);
        canvas_draw_line(canvas, cx + dx / 2, y_mid, cx + dx, y_mid + 4);
    }
}

static void draw_grime(Canvas *canvas, uint8_t grime) {
    if (grime == 0u) {
        return;
    }
    canvas_set_color(canvas, ColorBlack);
    const int y_span = CupBotY - CupTopY - 10;
    if (y_span < 4) {
        return;
    }
    /* More specks as grime rises (extra visible when water is already dark). */
    unsigned n = (unsigned)grime * 2u + 4u;
    if (n > 28u) {
        n = 28u;
    }
    for (unsigned i = 0; i < n; i++) {
        const int y = CupTopY + 6 + (int)((i * 13u + (unsigned)grime * 3u) % (unsigned)y_span);
        int l = 0;
        int r = 0;
        cup_horizontal_at(y, &l, &r);
        const int inner = r - l - 8;
        if (inner <= 1) {
            continue;
        }
        const int x = l + 4 + (int)((i * 17u + (unsigned)grime) % (unsigned)inner);
        canvas_draw_dot(canvas, x, y);
        if (i > 5u && grime > 5u) {
            canvas_draw_dot(canvas, x + 1, y);
        }
    }
}

static void draw_cup_outline(Canvas *canvas) {
    const int top_l = CupCx - CupTopHalfW;
    const int top_r = CupCx + CupTopHalfW;
    const int bot_l = CupCx - CupBotHalfW;
    const int bot_r = CupCx + CupBotHalfW;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, top_l, CupTopY, top_r, CupTopY);
    canvas_draw_line(canvas, top_l, CupTopY, bot_l, CupBotY);
    canvas_draw_line(canvas, top_r, CupTopY, bot_r, CupBotY);
    canvas_draw_line(canvas, bot_l, CupBotY, bot_r, CupBotY);
}

static void draw_star_sparkle(Canvas *canvas, int cx, int cy) {
    const int r = 10;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, cx, cy - r, cx, cy + r);
    canvas_draw_line(canvas, cx - r, cy, cx + r, cy);
    canvas_draw_line(canvas, cx - 7, cy - 7, cx + 7, cy + 7);
    canvas_draw_line(canvas, cx - 7, cy + 7, cx + 7, cy - 7);
}

/** Victory-screen: same tapered glass, dither water, pit + sprout. */
static void draw_mini_avocado(Canvas *canvas, int cx, int cy) {
    const int top_y = cy - 12;
    const int bot_y = cy + 10;
    /* Mini pit peel_r=7 (Ø14); rim Ø16 → ~1 px/side clearance. */
    const int top_hw = 8;
    const int bot_hw = 7;
    const int pit_cx = cx;
    const int pit_cy = cy - 1;
    const size_t peel_r = 7;

    canvas_set_color(canvas, ColorBlack);
    int l = 0;
    int r = 0;
    glass_horizontal_at(cx, top_y, bot_y, top_hw, bot_hw, top_y, &l, &r);
    canvas_draw_line(canvas, l, top_y, r, top_y);
    const int bl = cx - bot_hw;
    const int br = cx + bot_hw;
    canvas_draw_line(canvas, cx - top_hw, top_y, bl, bot_y);
    canvas_draw_line(canvas, cx + top_hw, top_y, br, bot_y);
    canvas_draw_line(canvas, bl, bot_y, br, bot_y);

    for (int y = top_y + 1; y < bot_y; y++) {
        glass_horizontal_at(cx, top_y, bot_y, top_hw, bot_hw, y, &l, &r);
        for (int x = l + 1; x < r; x++) {
            if (((x + y) & 1) == 0) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }

    canvas_draw_circle(canvas, pit_cx, pit_cy, peel_r);
    canvas_draw_disc(canvas, pit_cx, pit_cy + 1, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, pit_cx - 2, pit_cy - 2);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, pit_cx, pit_cy - (int)peel_r, pit_cx, pit_cy - (int)peel_r - 2);
    canvas_draw_line(canvas, pit_cx, pit_cy - (int)peel_r - 1, pit_cx - 3,
                     pit_cy - (int)peel_r - 3);
    canvas_draw_line(canvas, pit_cx, pit_cy - (int)peel_r - 1, pit_cx + 3,
                     pit_cy - (int)peel_r - 3);
}

static void draw_victory_screen(Canvas *canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "Victory!");
    draw_star_sparkle(canvas, 64, 22);
    draw_mini_avocado(canvas, 64, 44);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, "Congratulations!");
    elements_button_right(canvas, "OK");
}

static void play_draw_callback(Canvas *canvas, void *model) {
    furi_assert(model);
    const PlayViewModel *vm = model;
    PlayScreen *screen = vm->screen;
    const AvocadoData *d = screen->data;

    canvas_clear(canvas);

    if (avocado_rules_should_show_victory(d)) {
        draw_victory_screen(canvas);
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    const bool game_over = avocado_rules_is_game_over(d);
    const int cx = 64;
    const size_t pit_r = game_over ? 8u : 9u;
    /*
     * Water fill starts at CupTopY + 1. Pit center high so most of the disc is above
     * the water line (y < CupTopY + 1): reads as floating with top sticking out of the glass.
     */
    const int pit_cy = game_over ? (CupY + 22) : (CupY + 5);
    const int y_drought_surface = CupY + CupH - 9;

    if (game_over) {
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "GAME OVER");
    } else {
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Avocado pit");
    }

    if (game_over) {
        draw_water_dither(canvas, y_drought_surface);
    } else {
        draw_water_dither_full_cup(canvas, d->dirty_level);
    }

    draw_pit(canvas, cx, pit_cy, pit_r, game_over);

    if (!game_over) {
        draw_roots(canvas, cx, pit_cy + (int)pit_r, d->roots_length);
        draw_grime(canvas, d->dirty_level);
    }

    draw_cup_outline(canvas);

    canvas_set_font(canvas, FontSecondary);
    if (game_over) {
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, "Too dry!");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, "Press Start: new game");
        elements_button_right(canvas, "Start");
    } else {
        elements_button_right(canvas, "Clean");
    }
}

/* Flipper ViewInputCallback uses non-const InputEvent*; cannot be const here. */
// cppcheck-suppress constParameterCallback
static bool play_input_callback(InputEvent *event, void *context) {
    PlayScreen *screen = context;
    if (event->type != InputTypeShort) {
        return false;
    }
    if (event->key == InputKeyOk || event->key == InputKeyRight) {
        play_on_action(screen);
        return true;
    }
    return false;
}

static void play_on_action(PlayScreen *screen) {
    if (avocado_rules_should_show_victory(screen->data)) {
        avocado_rules_acknowledge_victory(screen->data);
        avocado_data_save(screen->data);
        if (screen->feedback) {
            avocado_feedback_play(screen->feedback, false);
        }
        view_commit_model(screen->view, true);
        return;
    }

    const bool was_game_over = avocado_rules_is_game_over(screen->data);
    avocado_rules_on_primary_action(screen->data);
    avocado_data_save(screen->data);

    if (screen->feedback) {
        avocado_feedback_play(screen->feedback, was_game_over);
    }
    view_commit_model(screen->view, true);
}

PlayScreen *play_screen_alloc(AvocadoData *data, AvocadoFeedback *feedback) {
    furi_check(data);

    PlayScreen *screen = malloc(sizeof(PlayScreen));
    if (!screen) {
        return NULL;
    }
    screen->view = view_alloc();
    if (!screen->view) {
        free(screen);
        return NULL;
    }
    screen->data = data;
    screen->feedback = feedback;

    view_allocate_model(screen->view, ViewModelTypeLockFree, sizeof(PlayViewModel));
    PlayViewModel *vm = view_get_model(screen->view);
    vm->screen = screen;
    view_commit_model(screen->view, false);

    view_set_context(screen->view, screen);
    view_set_draw_callback(screen->view, play_draw_callback);
    view_set_input_callback(screen->view, play_input_callback);

    return screen;
}

void play_screen_free(PlayScreen *screen) {
    if (!screen) {
        return;
    }
    view_free(screen->view);
    free(screen);
}

View *play_screen_get_view(PlayScreen *screen) {
    furi_assert(screen);
    return screen->view;
}
