/* Interaction regressions: long lines, idle wakeups, large repaints and folder loading. */
#include "cui_internal.h"
#include "cui/cui_render.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

static int latency_failed, latency_checks;
#define CHECK(c) do { latency_checks++; if (!(c)) { latency_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static cui_ctx *latency_context(void) {
    cui_ctx *ui = cui_create(NULL);
    cui_set_viewport(ui, 800, 600, 1);
    cui_begin_frame(ui, 1.0f / 60); cui_end_frame(ui);
    return ui;
}

static void test_timed_transitions(void) {
    CHECK(fabsf(cui__ease_value(0.275f, CUI__EASE_ENTER) - 0.8f) < 0.0001f);
    CHECK(fabsf(cui__ease_value(0.275f, CUI__EASE_EXIT) - 0.8f) < 0.0001f);
    CHECK(cui__ease_value((1.0f / 60) / CUI__T_HOVER, CUI__EASE_ENTER) > 0.25f);
    CHECK(cui__ease_value(0.05f / CUI__T_MODAL_EXIT, CUI__EASE_EXIT) > 0.5f);
    CHECK(CUI__T_TAB == 0.15f && CUI__T_MENU == 0.15f);
    CHECK(fabsf(cui__ease_value(0.35f, CUI__EASE_STATE) - 0.5f) < 0.0001f);
    const float durations[] = { CUI__T_PRESS, CUI__T_HOVER, CUI__T_STATE, CUI__T_ICON,
        CUI__T_ENTER, CUI__T_MENU, CUI__T_TOOLTIP, CUI__T_TAB, CUI__T_DIALOG,
        CUI__T_MODAL_EXIT, CUI__T_EXIT, CUI__T_SIDEBAR, CUI__T_TOAST, CUI__T_EXPAND, CUI__T_PANEL };
    const int rates[] = { 30, 60, 120, 144 };
    cui_ctx *ui = latency_context();
    for (int ease = CUI__EASE_ENTER; ease <= CUI__EASE_STATE; ease++) {
        float previous = 0;
        for (int i = 0; i <= 100; i++) {
            float value = cui__ease_value(i / 100.0f, (cui__ease)ease);
            CHECK(value >= previous && value <= 1);
            previous = value;
        }
        for (int rate = 0; rate < CUI_COUNTOF(rates); rate++) {
            float dt = 1.0f / rates[rate];
            for (int job = 0; job < CUI_COUNTOF(durations); job++) {
                cui_transition s = {0};
                int frames = (int)ceilf(durations[job] / dt);
                for (int frame = 0; frame < frames; frame++) {
                    previous = s.x;
                    cui_begin_frame(ui, dt);
                    cui__transition(ui, &s, 1, durations[job], (cui__ease)ease);
                    CHECK(s.x >= previous && s.x <= 1);
                    cui_end_frame(ui);
                }
                CHECK(s.x == 1 && !cui_needs_redraw(ui));
                /* Reverse repeatedly before completion: never jump or queue targets. */
                for (int i = 0; i < 8; i++) {
                    float target = i % 2 ? 1 : 0;
                    previous = s.x;
                    cui_begin_frame(ui, 0);
                    CHECK(cui__transition(ui, &s, target, durations[job], (cui__ease)ease) == previous);
                    cui_end_frame(ui);
                    cui_begin_frame(ui, dt);
                    cui__transition(ui, &s, target, durations[job], (cui__ease)ease);
                    CHECK(s.x >= 0 && s.x <= 1);
                    cui_end_frame(ui);
                }
                cui_begin_frame(ui, dt);
                CHECK(cui__transition(ui, &s, 0, 0, (cui__ease)ease) == 0);
                cui_end_frame(ui);
            }
        }
    }
    cui_transition s = { .x = 1 };
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(cui__transition(ui, &s, 0, CUI__T_STATE, CUI__EASE_STATE) > 0);
    cui_end_frame(ui);
    s = (cui_transition){0};
    cui_begin_frame(ui, 5);
    CHECK(cui__transition(ui, &s, 1, CUI__T_DIALOG, CUI__EASE_ENTER) > 0 && s.x < 1);
    CHECK(cui_delta_time(ui) == 5);
    cui_end_frame(ui);
    cui_theme_get(ui)->motion = 2;
    s = (cui_transition){0};
    for (int i = 0; i < 2; i++) {
        cui_begin_frame(ui, 0.1f);
        cui__transition(ui, &s, 1, 0.2f, CUI__EASE_STATE);
        cui_end_frame(ui);
    }
    CHECK(s.x > 0 && s.x < 1);
    cui_set_reduced_motion(ui, true);
    cui_begin_frame(ui, 0);
    CHECK(cui__transition(ui, &s, 1, CUI__T_DIALOG, CUI__EASE_ENTER) == 1);
    CHECK(cui__transition(ui, &s, 0, CUI__T_MODAL_EXIT, CUI__EASE_EXIT) == 0);
    cui_end_frame(ui);
    cui_destroy(ui);
}

static void test_popup_interruption(void) {
    cui_ctx *ui = latency_context();
    cui_id id = cui_make_id(ui, "popup");
    for (int frame = 0; frame < 20; frame++) {
        cui_begin_frame(ui, 1.0f / 144);
        bool open = frame % 2 == 0;
        cui__popup_set(ui, id, open);
        if (cui_popover_begin(ui, "popup", NULL)) { cui_text(ui, "Ready"); cui_popover_end(ui); }
        cui_popup *p = cui__popup_get(ui, id, false);
        CHECK(p && p->open == open && p->s.x >= 0 && p->s.x <= 1);
        cui_end_frame(ui);
    }
    cui_input_key(ui, CUI_KEY_ENTER, true, 0);
    cui_input_key(ui, CUI_KEY_ENTER, false, 0);
    cui_begin_frame(ui, 1.0f / 144);
    cui__popup_set(ui, id, true);
    CHECK(cui_popover_begin(ui, "popup", NULL));
    cui_text(ui, "Ready"); cui_popover_end(ui);
    CHECK(cui__popup_get(ui, id, false)->s.x == 1);
    cui_end_frame(ui);
    cui_input_key(ui, CUI_KEY_ESCAPE, true, 0);
    cui_begin_frame(ui, 1.0f / 144);
    CHECK(!cui_popover_begin(ui, "popup", NULL));
    CHECK(!cui__popup_get(ui, id, false)->open);
    cui_end_frame(ui);
    bool open = true;
    const cui_command commands[] = { { .title = "Open project" } };
    cui_begin_frame(ui, 1.0f / 144);
    CHECK(cui_command_palette(ui, &open, commands, 1) == -1);
    for (int i = 0; i < ui->node_count; i++) CHECK(ui->nodes[i].scale == 1 && ui->nodes[i].ty == 0);
    cui_end_frame(ui);
    cui_input_key(ui, CUI_KEY_ENTER, true, 0);
    cui_begin_frame(ui, 1.0f / 144);
    CHECK(cui_command_palette(ui, &open, commands, 1) == 0 && !open);
    cui_end_frame(ui);
    cui_destroy(ui);
}

static void test_mouse_motion_after_keyboard(void) {
    cui_ctx *ui = latency_context();
    cui_input_mouse_leave(ui);
    for (int i = 0; i < 2; i++) {
        cui_begin_frame(ui, 1.0f / 60);
        cui_button(ui, "Focus target");
        cui_end_frame(ui);
    }
    cui_input_key(ui, CUI_KEY_TAB, true, 0);
    cui_input_key(ui, CUI_KEY_TAB, false, 0);
    cui_begin_frame(ui, 1.0f / 60);
    cui_button(ui, "Focus target");
    cui_end_frame(ui);
    CHECK(ui->focus_visible);
    cui_input_mouse_move(ui, 400, 300);
    cui_begin_frame(ui, 1.0f / 60);
    float hover = cui__hover_fade(ui, cui_make_id(ui, "new hover"), true, CUI__T_HOVER);
    CHECK(hover > 0.25f && hover < 1);
    CHECK(ui->focus_visible);   /* moving the pointer must not hide keyboard focus */
    cui__popup_set(ui, cui_make_id(ui, "pointer popup"), true);
    CHECK(cui_popover_begin(ui, "pointer popup", NULL));
    cui_text(ui, "Ready"); cui_popover_end(ui);
    cui_popup *p = cui__popup_get(ui, cui_make_id(ui, "pointer popup"), false);
    CHECK(p && p->s.x > 0 && p->s.x < 1);
    cui_end_frame(ui);
    cui_destroy(ui);
}

static void test_key_motion_source(void) {
    cui_ctx *ui = latency_context();
    cui_input_key(ui, CUI_KEY_ENTER, true, 0);
    cui_input_key(ui, CUI_KEY_ENTER, false, 0);
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(cui__key_action(ui));
    cui__popup_set(ui, cui_make_id(ui, "keys"), true);
    ui->kpressed[CUI_KEY_ENTER] = 0;   /* a select consumes the key after opening */
    CHECK(cui_popover_begin(ui, "keys", NULL));
    cui_text(ui, "Ready"); cui_popover_end(ui);
    CHECK(cui__popup_get(ui, cui_make_id(ui, "keys"), false)->s.x == 1);
    cui_end_frame(ui);
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(!cui__key_action(ui));
    cui_end_frame(ui);
    cui_input_key(ui, CUI_KEY_ENTER, true, 0);
    cui_input_mouse_button(ui, CUI_MOUSE_RIGHT, true);
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(!cui__key_action(ui));
    cui_end_frame(ui);
    cui_destroy(ui);

    ui = latency_context();
    bool open = true;
    cui_input_key(ui, CUI_KEY_ENTER, true, 0);
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(cui_dialog_begin(ui, "Modal", &open, NULL));
    open = false;   /* a keyboard-activated footer button changes this inside the body */
    cui_dialog_end(ui);
    cui_end_frame(ui);
    cui_input_key(ui, CUI_KEY_ENTER, false, 0);
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(!cui_dialog_begin(ui, "Modal", &open, NULL));
    cui_end_frame(ui);
    cui_destroy(ui);
}

static void test_dropdown_duration(void) {
    cui_ctx *ui = latency_context();
    cui_id id = cui_make_id(ui, "dropdown");
    cui__popup_set(ui, id, true);
    for (int i = 0; i < 9; i++) {
        cui_begin_frame(ui, 1.0f / 60);
        CHECK(cui__popup_begin(ui, id, 0, CUI_PLACE_BOTTOM_START, 200, 6, 0, true));
        cui_text(ui, "Ready"); cui__popup_end(ui);
        cui_end_frame(ui);
    }
    CHECK(cui__popup_get(ui, id, false)->s.x == 1);
    cui_destroy(ui);
}

static void latency_nav_frame(cui_ctx *ui, int *selected, cui_id *second) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_column(ui, .width = CUI_PX(240)) {
        if (cui_nav_item(ui, "One", .selected = *selected == 0)) *selected = 0;
        if (cui_nav_item(ui, "Two", .selected = *selected == 1)) *selected = 1;
        *second = cui_last_id(ui);
    }
    cui_end_frame(ui);
}

static void test_keyboard_nav_motion(void) {
    cui_ctx *ui = latency_context();
    int selected = 0;
    cui_id second;
    for (int i = 0; i < 3; i++) latency_nav_frame(ui, &selected, &second);
    ui->focus = second; ui->focus_visible = true;
    cui_input_key(ui, CUI_KEY_ENTER, true, 0);
    cui_input_key(ui, CUI_KEY_ENTER, false, 0);
    latency_nav_frame(ui, &selected, &second);
    CHECK(selected == 1);
    for (int i = 0; i < 2; i++) latency_nav_frame(ui, &selected, &second);
    cui_color active = CUI_C(ui, ACTIVE);
    const cui_draw_data *dd = cui_get_draw_data(ui);
    bool found = false;
    for (int i = 0; i < dd->vertex_count; i++) {
        const cui_vertex *v = &dd->vertices[i];
        if (v->mode != CUI_SHADE_RRECT || memcmp(&v->color, &active, sizeof active)) continue;
        CHECK(fabsf(v->y - v->ly - v->hh - cui_get_rect(ui, second).y) < 0.01f);
        found = true;
    }
    CHECK(found);
    for (int i = 0; i < 3; i++) latency_nav_frame(ui, &selected, &second);
    selected = 0;
    latency_nav_frame(ui, &selected, &second);
    dd = cui_get_draw_data(ui);
    for (int i = 0; i < dd->vertex_count; i++) {
        const cui_vertex *v = &dd->vertices[i];
        if (v->mode != CUI_SHADE_RRECT || memcmp(&v->color, &active, sizeof active)) continue;
        float top = v->y - v->ly - v->hh;
        CHECK(top > 0 && top < cui_get_rect(ui, second).y);
    }
    cui_destroy(ui);
}

static cui_id latency_range_frame(cui_ctx *ui, float *lo, float *hi) {
    cui_begin_frame(ui, 1.0f / 144);
    cui_column(ui, .width = CUI_PX(300), .pad = 20) {
        ui->slider_no_header = true;
        cui_range_slider(ui, "range", lo, hi, 0, 100);
    }
    cui_id id = cui_last_id(ui);
    cui_end_frame(ui);
    return id;
}

static void test_drag_tracks_pointer(void) {
    for (int rtl = 0; rtl < 2; rtl++) {
        cui_ctx *ui = cui_create(&(cui_ctx_desc){ .direction = rtl ? CUI_DIRECTION_RTL : CUI_DIRECTION_LTR });
        cui_set_viewport(ui, 800, 600, 1);
        float lo = 25, hi = 75;
        latency_range_frame(ui, &lo, &hi);
        cui_id id = latency_range_frame(ui, &lo, &hi);
        cui_rect track = cui_get_rect(ui, id);
        float span = track.w - 18;
        cui_input_mouse_move(ui, track.x + 9 + span * (rtl ? 0.75f : 0.25f), track.y + track.h / 2);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        latency_range_frame(ui, &lo, &hi);
        const float positions[] = { 0.1f, 0.6f, 0.2f };
        for (int i = 0; i < CUI_COUNTOF(positions); i++) {
            cui_input_mouse_move(ui, track.x + 9 + span * (rtl ? 1 - positions[i] : positions[i]), track.y + track.h / 2);
            latency_range_frame(ui, &lo, &hi);
            CHECK(fabsf(lo - positions[i] * 100) < 0.001f);
            float left = INFINITY, right = -INFINITY;
            cui_color thumb = CUI_C(ui, CONTROL_THUMB);
            const cui_draw_data *dd = cui_get_draw_data(ui);
            for (int v = 0; v < dd->vertex_count; v++) {
                const cui_vertex *p = &dd->vertices[v];
                if (p->mode != CUI_SHADE_RRECT || memcmp(&p->color, &thumb, sizeof thumb)) continue;
                float center = p->x - p->lx;
                left = CUI_MIN(left, center); right = CUI_MAX(right, center);
            }
            CHECK(fabsf(left - (track.x + 9 + span * (rtl ? 1 - hi / 100 : lo / 100))) < 0.01f);
            CHECK(fabsf(right - (track.x + 9 + span * (rtl ? 1 - lo / 100 : hi / 100))) < 0.01f);
        }
        cui_destroy(ui);
    }
}

static void test_motion_response(void) {
    const struct { float duration, bounce, budget; bool tween; } jobs[] = {
        { CUI__T_HIGHLIGHT, 0, 0.08f, true },
        { CUI__T_HOVER, 0, 0.10f, true },
        { CUI__T_FOCUS, 0, 0.10f, true },
        { CUI__T_STATE, 0, 0.12f, true },
        { CUI_SPRING_SMOOTH, 0.24f, false },
        { CUI_SPRING_SNAPPY, 0.16f, false },
        { CUI_SPRING_MORPH, 0.26f, false },
        { CUI__SPRING_EXPAND, 0.20f, false },
        { CUI__SPRING_ENTER, 0.18f, false },
    };
    const int rates[] = { 30, 60, 144 };
    cui_ctx *ui = latency_context();
    for (int rate = 0; rate < CUI_COUNTOF(rates); rate++) {
        float dt = 1.0f / rates[rate];
        for (int job = 0; job < CUI_COUNTOF(jobs); job++) {
            cui_spring s = {0};
            for (int direction = 0; direction < 2; direction++) {
                float target = direction ? 0 : 1;
                int frames = (int)ceilf(jobs[job].budget / dt);
                for (int frame = 0; frame < frames; frame++) {
                    cui_begin_frame(ui, dt);
                    if (jobs[job].tween) cui__tween(ui, &s.x, target, jobs[job].duration);
                    else cui__spring(ui, &s, target, jobs[job].duration, jobs[job].bounce);
                    CHECK(isfinite(s.x) && isfinite(s.v));
                    if (!jobs[job].bounce) CHECK(s.x >= 0 && s.x <= 1);
                    cui_end_frame(ui);
                }
                CHECK(fabsf(s.x - target) < 0.05f);
            }
            for (int frame = 0; frame < rates[rate] / 2; frame++) {
                cui_begin_frame(ui, dt);
                if (jobs[job].tween) cui__tween(ui, &s.x, 0, jobs[job].duration);
                else cui__spring(ui, &s, 0, jobs[job].duration, jobs[job].bounce);
                cui_end_frame(ui);
            }
            CHECK(s.x == 0 && s.v == 0 && !cui_needs_redraw(ui));
            cui_set_reduced_motion(ui, true);
            cui_begin_frame(ui, dt);
            if (jobs[job].tween) cui__tween(ui, &s.x, 1, jobs[job].duration);
            else cui__spring(ui, &s, 1, jobs[job].duration, jobs[job].bounce);
            CHECK(s.x == 1 && s.v == 0);
            cui_end_frame(ui);
            cui_set_reduced_motion(ui, false);
        }
    }
    cui_destroy(ui);
}

static void test_idle_clock(void) {
    cui_ctx *ui = latency_context();
    double before = cui_time(ui);
    cui_begin_frame(ui, 2);
    CHECK(fabs(cui_time(ui) - before - 2) < 1e-6);
    CHECK(cui_delta_time(ui) == 2);
    float hover = 0;
    cui_transition dialog = {0};
    cui__tween(ui, &hover, 1, CUI__T_HOVER);
    cui__transition(ui, &dialog, 1, CUI__T_DIALOG, CUI__EASE_ENTER);
    CHECK(hover > 0 && hover < 0.75f);
    CHECK(dialog.x > 0 && dialog.x < 0.5f);
    cui_toast(ui, "Saved", .duration = 4);
    cui_end_frame(ui);
    CHECK(ui->toast_count == 1 && ui->toasts[0].age == 0);
    double elapsed = 0;
    for (int frames = 0; ui->toast_count && frames < 1000; frames++) {
        float dt = cui_needs_redraw(ui) ? 1.0f / 60 : cui_next_wakeup(ui);
        CHECK(dt >= 0);
        if (dt < 0) break;
        elapsed += dt;
        cui_begin_frame(ui, dt); cui_end_frame(ui);
    }
    CHECK(ui->toast_count == 0 && elapsed >= 4 && elapsed < 4.6);
    before = cui_time(ui);
    cui_begin_frame(ui, NAN); cui_end_frame(ui);
    cui_begin_frame(ui, INFINITY); cui_end_frame(ui);
    cui_begin_frame(ui, -1); cui_end_frame(ui);
    CHECK(cui_time(ui) == before);
    cui_set_reduced_motion(ui, true);
    cui_begin_frame(ui, 2);
    hover = 0; dialog = (cui_transition){0};
    cui__tween(ui, &hover, 1, CUI__T_HOVER);
    cui__transition(ui, &dialog, 1, CUI__T_DIALOG, CUI__EASE_ENTER);
    CHECK(hover == 1 && dialog.x == 1);
    cui_end_frame(ui);
    cui_destroy(ui);
}

static void test_hold_after_idle(void) {
    cui_ctx *ui = latency_context();
    cui_id id = cui_make_id(ui, "Hold");
    ui->focus = id; ui->focus_visible = true;
    cui_input_key(ui, CUI_KEY_SPACE, true, 0);
    cui_begin_frame(ui, 5);
    CHECK(!cui_hold_button(ui, "Hold", .duration = 1));
    cui_end_frame(ui);
    bool fired = false;
    for (int i = 0; i < 61; i++) {
        cui_input_key(ui, CUI_KEY_SPACE, true, 0);   /* repeat must not restart the hold */
        cui_begin_frame(ui, 1.0f / 60);
        fired |= cui_hold_button(ui, "Hold", .duration = 1);
        cui_end_frame(ui);
    }
    CHECK(fired);
    cui_destroy(ui);
}

static void test_clicks_after_idle(void) {
    cui_ctx *ui = latency_context();
    cui_input_mouse_move(ui, 20, 20);
    for (int click = 0; click < 4; click++) {
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        cui_begin_frame(ui, click == 3 ? 2 : 0.1f);
        CHECK(ui->click_count == (click == 3 ? 1 : click + 1));
        cui_end_frame(ui);
    }
    cui_input_mouse_move(ui, 100, 100);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    cui_begin_frame(ui, 0.1f);
    CHECK(ui->click_count == 1);
    cui_end_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
    cui_begin_frame(ui, 0.1f);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    CHECK(ui->click_count == 2);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    CHECK(ui->click_count == 2);
    cui_end_frame(ui);
    cui_destroy(ui);
}

static int latency_mark_vertices(cui_theme_mode theme, int start) {
    char text[10001];
    for (int i = 0; i < 2000; i++) memcpy(text + i * 5, "aaaa\n", 5);
    text[10000] = 0;
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .theme = theme });
    cui_set_viewport(ui, 800, 600, 1);
    cui_set_reduced_motion(ui, true);
    cui_text_mark marks[] = { { start, 10000, 0 }, { 7000, 7010, 1 } };
    for (int frame = 0; frame < 4; frame++) {
        if (frame == 2) cui_code_editor_goto(ui, "code", 1500, 1);
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_editor(ui, "code", text, sizeof text, .lang = "text", .marks = marks, .mark_count = start < 0 ? 0 : 2);
        cui_end_frame(ui);
    }
    int count = cui_get_draw_data(ui)->vertex_count;
    cui_destroy(ui);
    return count;
}

static void test_long_marks(void) {
    for (int dark = 0; dark < 2; dark++) {
        int none = latency_mark_vertices((cui_theme_mode)dark, -1);
        int whole = latency_mark_vertices((cui_theme_mode)dark, 0);
        int clipped = latency_mark_vertices((cui_theme_mode)dark, 6000);
        CHECK(whole > none);
        CHECK(whole == clipped);
    }
}

static void test_long_line_highlights(void) {
    enum { LEN = 20000 };
    char *text = malloc(LEN + 1);
    cui_text_mark *marks = malloc(sizeof *marks * LEN);
    CHECK(text && marks);
    if (!text || !marks) { free(text); free(marks); return; }
    memset(text, 'a', LEN); text[LEN] = 0;
    for (int i = 0; i < LEN; i++) marks[i] = (cui_text_mark){ i, i + 1, 0 };
    for (int rtl = 0; rtl < 2; rtl++) {
        cui_ctx *ui = latency_context();
        ui->direction = rtl ? CUI_DIRECTION_RTL : CUI_DIRECTION_LTR;
        cui_code_editor_find(ui, "code", "a");
        for (int frame = 0; frame < 3; frame++) {
            cui_begin_frame(ui, 1.0f / 60);
            if (frame == 2) cui_code_editor_select(ui, "code", LEN - 1, LEN);
            cui_code_editor_result r = cui_code_editor(ui, "code", text, LEN + 1,
                .lang = "text", .marks = marks, .mark_count = LEN);
            cui_end_frame(ui);
            CHECK(r.length == LEN && r.find_matches == LEN);
            CHECK(cui_get_draw_data(ui)->vertex_count < 5000);
            if (frame == 2) CHECK(r.cursor == LEN && r.find_index == LEN);
        }
        cui_destroy(ui);
    }
    free(text); free(marks);
}

static void test_parallel_repaint(void) {
    enum { W = 800, H = 600, BYTES = W * H * 4 };
    uint8_t *one = malloc(BYTES), *many = malloc(BYTES);
    CHECK(one && many);
    if (!one || !many) { free(one); free(many); return; }
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw) });
    cui_set_viewport(ui, W, H, 1);
    for (int frame = 0; frame < 3; frame++) {
        cui_begin_frame(ui, 1.0f / 60);
        cui_box(ui, .width = CUI_FILL, .height = CUI_FILL, .bg = CUI_RGB(0x405060)) {
            cui_draw_rect(ui, (cui_rect){ 100, 100, 600, 300 }, CUI_CORNERS(18), CUI_RGBA(0x80a0c080));
        }
        cui_end_frame(ui);
        const cui_draw_data *dd = cui_get_draw_data(ui);
        CHECK(dd->index_count < 600);
        cui_sw_set_threads(sw, 1);
        cui_sw_render(sw, dd, one, W, H, W * 4, CUI_SW_RGBA);
        cui_sw_set_threads(sw, 4);
        cui_sw_render(sw, dd, many, W, H, W * 4, CUI_SW_RGBA);
        CHECK(memcmp(one, many, BYTES) == 0);
    }
    cui_destroy(ui); cui_sw_destroy(sw); free(one); free(many);
}

static int latency_listing_ready;
static int latency_listing(void *user, const char *path, cui_file_entry *out, int cap, char *names, size_t size, bool hidden) {
    (void)user; (void)path; (void)hidden;
    if (!latency_listing_ready) return CUI_FILE_LIST_PENDING;
    if (latency_listing_ready < 0) return -1;
    if (cap < 1 || size < 6) return CUI_FILE_LIST_NAMES_FULL;
    memcpy(names, "a.txt", 6);
    out[0] = (cui_file_entry){ .name = names };
    return 1;
}

static cui_file_browser_result latency_folder(cui_ctx *ui, char *path, cui_file_list_fn list) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_file_browser_result r = cui_file_browser(ui, "files", path, 256, .list = list, .no_places = true);
    cui_end_frame(ui);
    return r;
}

static void test_pending_listing(void) {
    cui_ctx *ui = latency_context();
    char path[256] = "/files";
    latency_listing_ready = 0;
    cui_file_browser_result r = latency_folder(ui, path, latency_listing);
    CHECK(r.loading && !r.error && !r.selected && cui_next_wakeup(ui) >= 0);
    cui_rect before = cui_get_rect(ui, cui_make_id(ui, "files"));
    latency_listing_ready = 1;
    r = latency_folder(ui, path, latency_listing);
    CHECK(!r.loading && !r.error);
    cui_rect after = cui_get_rect(ui, cui_make_id(ui, "files"));
    CHECK(before.h == after.h && before.w == after.w);
    snprintf(path, sizeof path, "/missing");
    latency_listing_ready = -1;
    r = latency_folder(ui, path, latency_listing);
    CHECK(r.error && !r.loading);
    cui_destroy(ui);
}

#if !defined(_WIN32) && !defined(__wasm__) && !defined(__EMSCRIPTEN__) && !defined(CUI_NO_FILESYSTEM)
static void test_native_listing(void) {
    char dir[] = "/tmp/cui-latency-XXXXXX";
    char *created = mkdtemp(dir);
    CHECK(created != NULL);
    if (!created) return;
    char path[256], file[256];
    snprintf(file, sizeof file, "%s/a.txt", dir);
    FILE *f = fopen(file, "w"); CHECK(f != NULL); if (f) fclose(f);
    for (int close_early = 0; close_early < 8; close_early++) {
        cui_ctx *ui = latency_context();
        snprintf(path, sizeof path, "%s", dir);
        cui_file_browser_result r = latency_folder(ui, path, NULL);
        CHECK(r.loading && !r.error);
        if (close_early == 0) {
            /* Change paths while the previous read is still owned by the browser. */
            snprintf(path, sizeof path, "%s/missing", dir);
            r = latency_folder(ui, path, NULL);
            CHECK(r.loading && !r.selected);
            for (int i = 0; r.loading && i < 1000; i++) {
                struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL);
                r = latency_folder(ui, path, NULL);
            }
            CHECK(!r.loading && r.error);
            snprintf(path, sizeof path, "%s", dir);
            r = latency_folder(ui, path, NULL);
            for (int i = 0; r.loading && i < 1000; i++) {
                struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL);
                r = latency_folder(ui, path, NULL);
            }
            CHECK(!r.loading && !r.error);
            cui_push_id(ui, "files");
            cui_scroll_state *rows = cui_state(ui, cui_make_id(ui, "rows"), sizeof *rows);
            CHECK(rows->content_h == 30);
            cui_pop_id(ui);
        }
        cui_destroy(ui);
    }
    remove(file); rmdir(dir);
}
#endif

int main(void) {
    test_timed_transitions(); test_popup_interruption(); test_mouse_motion_after_keyboard();
    test_key_motion_source(); test_dropdown_duration(); test_keyboard_nav_motion(); test_drag_tracks_pointer();
    test_motion_response();
    test_idle_clock(); test_hold_after_idle(); test_clicks_after_idle(); test_long_marks();
    test_long_line_highlights(); test_parallel_repaint(); test_pending_listing();
#if !defined(_WIN32) && !defined(__wasm__) && !defined(__EMSCRIPTEN__) && !defined(CUI_NO_FILESYSTEM)
    test_native_listing();
#endif
    printf("latency: %d checks, %d failed\n", latency_checks, latency_failed);
    return latency_failed ? 1 : 0;
}
