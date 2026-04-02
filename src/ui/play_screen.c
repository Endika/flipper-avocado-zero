#include "include/ui/play_screen.h"
#include "include/domain/avocado_rules.h"
#include "include/platform/feedback_helper.h"
#include "include/platform/storage_helper.h"
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdio.h>

struct PlayScreen {
    View *view;
    AvocadoData *data;
    AvocadoFeedback *feedback;
};

typedef struct {
    PlayScreen *screen;
} PlayViewModel;

/** Jar in screen space (128x64): rounded glass, water band, pit, toothpicks. */
enum {
    JarX = 34,
    JarY = 14,
    JarW = 60,
    JarH = 38,
    JarR = 3,
};

static void play_on_action(PlayScreen *screen);

static void draw_toothpicks(Canvas *canvas) {
    const int y0 = JarY + 1;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, JarX + 14, y0 - 1, JarX + 8, y0 + 5);
    canvas_draw_line(canvas, JarX + JarW / 2, y0 - 2, JarX + JarW / 2, y0 + 4);
    canvas_draw_line(canvas, JarX + JarW - 14, y0 - 1, JarX + JarW - 8, y0 + 5);
}

static void draw_water_fill(Canvas *canvas, int y_surface) {
    const int inner_x = JarX + 2;
    const int inner_w = JarW - 4;
    const int bottom = JarY + JarH - 2;
    if (y_surface >= bottom) {
        return;
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, inner_x, y_surface, inner_w, (size_t)(bottom - y_surface));
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
    const int jar_bottom = JarY + JarH - 2;
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
    canvas_set_color(canvas, ColorBlack);
    for (uint8_t i = 0; i < grime && i < 12u; i++) {
        const int x = JarX + 4 + (int)((i * 17u) % (unsigned)(JarW - 8));
        const int y = JarY + 6 + (int)((i * 13u) % (unsigned)(JarH - 12));
        canvas_draw_dot(canvas, x, y);
        if (i > 3u) {
            canvas_draw_dot(canvas, x + 1, y);
        }
    }
}

static void draw_jar_outline(Canvas *canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, JarX, JarY, JarW, JarH, JarR);
}

static void draw_star_sparkle(Canvas *canvas, int cx, int cy) {
    const int r = 10;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, cx, cy - r, cx, cy + r);
    canvas_draw_line(canvas, cx - r, cy, cx + r, cy);
    canvas_draw_line(canvas, cx - 7, cy - 7, cx + 7, cy + 7);
    canvas_draw_line(canvas, cx - 7, cy + 7, cx + 7, cy - 7);
}

/**
 * Victory-screen mascot: glass + water + pit (reads at 1 bpp).
 * To tweak art: adjust the constants below, or add a baked icon under assets/
 * (PNG → .icon via fbt) and draw it with canvas_draw_icon after including the
 * generated header; fap_icon in application.fam is only the launcher 10×10.
 */
static void draw_mini_avocado(Canvas *canvas, int cx, int cy) {
    /* Jar: open top, U + rim (screen space relative to center). */
    const int rim_l = cx - 14;
    const int rim_r = cx + 14;
    const int rim_y = cy - 12;
    const int side_bot = cy + 10;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, rim_l, rim_y, rim_r, rim_y);
    canvas_draw_line(canvas, rim_l, rim_y, rim_l, side_bot);
    canvas_draw_line(canvas, rim_r, rim_y, rim_r, side_bot);
    canvas_draw_line(canvas, rim_l, side_bot, rim_r, side_bot);

    /* “Water”: two lines in the lower jar (below the pit). */
    const int y_w1 = cy + 4;
    const int y_w2 = cy + 6;
    canvas_draw_line(canvas, rim_l + 2, y_w1, rim_r - 2, y_w1);
    canvas_draw_line(canvas, rim_l + 2, y_w2, rim_r - 2, y_w2);

    /* Pit: outer peel as stroke (not a solid disc — reads clearer small). */
    const int pit_cx = cx;
    const int pit_cy = cy - 1;
    const size_t peel_r = 7;
    canvas_draw_circle(canvas, pit_cx, pit_cy, peel_r);
    canvas_draw_disc(canvas, pit_cx, pit_cy + 1, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, pit_cx - 2, pit_cy - 2);
    canvas_set_color(canvas, ColorBlack);
    /* Tiny sprout so it’s “avocado pit”, not a plain ball. */
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
    /* Pit center: mostly above water; only ~3 px of the pit dips below the surface. */
    const int pit_cy = game_over ? (JarY + 22) : (JarY + 21);
    const int y_surface_normal = pit_cy + (int)pit_r - 3;
    const int y_surface = game_over ? (JarY + JarH - 9) : y_surface_normal;

    if (game_over) {
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "GAME OVER");
    } else {
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Avocado pit");
    }

    draw_water_fill(canvas, y_surface);

    draw_pit(canvas, cx, pit_cy, pit_r, game_over);

    if (!game_over) {
        draw_roots(canvas, cx, pit_cy + (int)pit_r, d->roots_length);
        draw_grime(canvas, d->dirty_level);
    }

    draw_toothpicks(canvas);
    draw_jar_outline(canvas);

    canvas_set_font(canvas, FontSecondary);
    char line[48];
    if (game_over) {
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, "Too dry!");
        snprintf(line, sizeof(line), "Press Start: new game");
    } else {
        snprintf(line, sizeof(line), "Roots %u  Grime %u/%u", (unsigned)d->roots_length,
                 (unsigned)d->dirty_level, (unsigned)AVOCADO_GRIME_GAME_OVER);
    }
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, line);

    if (game_over) {
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
