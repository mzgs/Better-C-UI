/* Local styles must reach measurement, editing, popups and selection feedback. */
#include "cui_internal.h"

static int style_checks, style_failed;
#define CHECK(c) do { style_checks++; if (!(c)) { style_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool style_equal(cui_color a, cui_color b) { return memcmp(&a, &b, sizeof a) == 0; }

static cui_node *style_node(cui_ctx *ui, cui_id id) {
    for (int i = 0; i < ui->node_count; i++) if (ui->nodes[i].id == id) return &ui->nodes[i];
    CHECK(false);
    return &ui->nodes[0];
}

static bool style_drawn(cui_ctx *ui, cui_color color, int mode) {
    const cui_draw_data *dd = cui_get_draw_data(ui);
    for (int i = 0; i < dd->vertex_count; i++)
        if (dd->vertices[i].mode == mode && style_equal(dd->vertices[i].color, color)) return true;
    return false;
}

static cui_ctx *style_context(int dark, int rtl) {
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .theme = dark ? CUI_THEME_DARK : CUI_THEME_LIGHT,
        .direction = rtl ? CUI_DIRECTION_RTL : CUI_DIRECTION_LTR });
    cui_set_viewport(ui, 360, 900, 1);
    cui_set_reduced_motion(ui, true);
    cui_theme_get(ui)->font_scale = 1.25f;
    cui_input_mouse_leave(ui);
    return ui;
}

static float style_luminance(cui_color c) {
    float channels[] = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f };
    for (int i = 0; i < 3; i++)
        channels[i] = channels[i] <= 0.04045f ? channels[i] / 12.92f : powf((channels[i] + 0.055f) / 1.055f, 2.4f);
    return 0.2126f * channels[0] + 0.7152f * channels[1] + 0.0722f * channels[2];
}

static void style_button_frame(cui_ctx *ui, int gray, bool icon, cui_color color) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_color bg = cui__rgba(gray, gray, gray, 255);
    cui_column(ui, .pad = 20) {
        if (icon) cui_icon_button(ui, CUI_ICON_HEART, .bg = bg, .color = color, .tooltip = "Styled icon");
        else cui_button(ui, "Styled button", .bg = bg, .color = color);
    }
    cui_end_frame(ui);
    cui_node *button = &ui->nodes[ui->last_node];
    bool found = false;
    for (int i = ui->last_node + 1; i < ui->node_count; i++) {
        cui_node *n = &ui->nodes[i];
        if (!n->text) continue;
        found = true;
        if (cui__color_set(color)) CHECK(style_equal(n->text_color, color));
        else {
            float fg_lum = style_luminance(n->text_color), bg_lum = style_luminance(button->bg);
            CHECK((CUI_MAX(fg_lum, bg_lum) + 0.05f) / (CUI_MIN(fg_lum, bg_lum) + 0.05f) >= 4.5f);
        }
    }
    CHECK(found);
}

static void test_style_button_contrast(void) {
    for (int dark = 0; dark < 2; dark++) for (int reduced = 0; reduced < 2; reduced++) {
        cui_ctx *ui = style_context(dark, false);
        cui_set_reduced_motion(ui, reduced != 0);
        for (int icon = 0; icon < 2; icon++) for (int gray = 0; gray < 256; gray++) {
            cui_input_mouse_leave(ui);
            style_button_frame(ui, gray, icon != 0, CUI_TRANSPARENT);
            style_button_frame(ui, gray, icon != 0, CUI_TRANSPARENT);
            cui_rect r = ui->nodes[ui->last_node].rect;
            cui_input_mouse_move(ui, r.x + r.w / 2, r.y + r.h / 2);
            for (int frame = 0; frame < 10; frame++) style_button_frame(ui, gray, icon != 0, CUI_TRANSPARENT);
            cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
            style_button_frame(ui, gray, icon != 0, CUI_TRANSPARENT);
            cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
            style_button_frame(ui, gray, icon != 0, CUI_TRANSPARENT);
            cui_input_mouse_leave(ui);
            for (int frame = 0; frame < 10; frame++) style_button_frame(ui, gray, icon != 0, CUI_TRANSPARENT);
        }
        style_button_frame(ui, 118, false, CUI_RGB(0xff0000));
        cui_destroy(ui);
    }
}

typedef struct { cui_input_opts opts; char text[256]; bool multiline; int flags; } style_field;
static void style_field_frame(cui_ctx *ui, style_field *f) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_column(ui, .width = CUI_FILL, .pad = 20, .gap = 12) {
        f->flags = f->multiline ? cui_textarea_ex(ui, "styled", f->text, sizeof f->text, &f->opts)
                               : cui_input_ex(ui, "styled", f->text, sizeof f->text, &f->opts);
        char plain[16] = "Neighbor";
        cui_input(ui, "plain", plain, sizeof plain);
    }
    cui_end_frame(ui);
}

static void style_field_key(cui_ctx *ui, style_field *f, cui_key key, int mods) {
    cui_input_key(ui, key, true, mods); cui_input_key(ui, key, false, mods);
    style_field_frame(ui, f);
}

static void test_style_fields(int dark, int rtl) {
    for (int multiline = 0; multiline < 2; multiline++) {
        cui_ctx *ui = style_context(dark, rtl);
        style_field f = { .opts = { .bg = CUI_RGB(0x24374a), .color = CUI_RGB(0xfafafa),
            .border_color = CUI_RGB(0x5383b3), .radius = -1, .font_size = 24, .height = 88 },
            .text = "abcdef", .multiline = multiline };
        style_field_frame(ui, &f); style_field_frame(ui, &f);
        cui_id id = cui_make_id(ui, "styled"), aid = id ^ 0xED17u;
        cui_node *n = style_node(ui, id), *plain = style_node(ui, cui_make_id(ui, "plain"));
        CHECK(style_equal(n->bg, f.opts.bg) && style_equal(n->border_color, f.opts.border_color));
        CHECK(n->radius.tl == 0 && n->rect.w <= 320);
        CHECK(n->rect.h >= (multiline ? 88 : 30 * 1.3f + 12));
        CHECK(plain->rect.h == ui->theme.control_md && plain->radius.tl == ui->theme.radius_control);
        CHECK(style_equal(plain->bg, CUI_C(ui, SURFACE)));
        CHECK(style_drawn(ui, f.opts.color, CUI_SHADE_TEXT));
        cui_rect a = style_node(ui, aid)->rect;
        float x = a.x + cui_measure_text(ui, CUI_FONT_BODY, 30, "ab", -1).width;
        float y = a.y + (multiline ? 15 : a.h / 2);
        cui_input_mouse_move(ui, x, y); style_field_frame(ui, &f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true); style_field_frame(ui, &f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false); style_field_frame(ui, &f);
        cui_edit_state *edit = cui_state(ui, id, sizeof *edit);
        CHECK(edit->cursor == 2 && ui->focus == id);
        cui_input_text(ui, "Z"); style_field_frame(ui, &f);
        CHECK((f.flags & CUI_CHANGED) && strcmp(f.text, "abZcdef") == 0);
        style_field_key(ui, &f, CUI_KEY_Z, CUI_MOD_PRIMARY);
        CHECK(strcmp(f.text, "abcdef") == 0);
        f.opts.read_only = true;
        cui_input_text(ui, "X"); style_field_frame(ui, &f);
        CHECK(strcmp(f.text, "abcdef") == 0);
        f.opts.read_only = false;
        f.opts.disabled = true;
        cui_input_text(ui, "X"); style_field_frame(ui, &f);
        CHECK(strcmp(f.text, "abcdef") == 0 && style_node(ui, id)->opacity == CUI__DISABLED);
        f.opts.disabled = false;
        f.opts.error = "Invalid value";
        style_field_frame(ui, &f);
        CHECK(style_equal(style_node(ui, id)->border_color, CUI_C(ui, DANGER)));
        f.opts.error = NULL;
        f.opts.radius = 6;
        ui->focus = id; ui->focus_visible = true;
        style_field_frame(ui, &f);
        CHECK(style_equal(style_node(ui, id)->border_color, cui__edge_on(ui)));
        CHECK(style_node(ui, id)->paint && style_node(ui, id)->radius.tl == 6);
        if (multiline) {
            strcpy(f.text, "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight");
            style_field_frame(ui, &f); style_field_frame(ui, &f);
            a = style_node(ui, aid)->rect;
            cui_input_mouse_move(ui, a.x + 10, a.y + 10);
            cui_input_scroll(ui, 0, -2); style_field_frame(ui, &f);
            CHECK(edit->scroll_y > 0);
        }
        f.opts = (cui_input_opts){ .font_size = -1 };
        f.multiline = false;
        ui->focus = 0; cui_input_mouse_leave(ui);
        style_field_frame(ui, &f);
        CHECK(style_node(ui, id)->rect.h == ui->theme.control_md);
        CHECK(style_node(ui, id)->radius.tl == ui->theme.radius_control);
        cui_destroy(ui);
    }
}

typedef struct { cui_select_opts opts; int index; } style_select;
static void style_select_frame(cui_ctx *ui, style_select *s) {
    static const char *items[] = { "Alpha", "Beta", "Gamma" };
    cui_begin_frame(ui, 1.0f / 60);
    cui_column(ui, .pad = 20, .width = CUI_FILL) {
        cui_select_ex(ui, "select", items, 3, &s->index, &s->opts);
    }
    cui_end_frame(ui);
}

static void style_select_key(cui_ctx *ui, style_select *s, cui_key key) {
    cui_input_key(ui, key, true, 0); cui_input_key(ui, key, false, 0);
    style_select_frame(ui, s);
}

static void test_style_select(int dark, int rtl) {
    cui_ctx *ui = style_context(dark, rtl);
    style_select s = { .opts = { .bg = CUI_RGB(0x24374a), .color = CUI_RGB(0xfafafa),
        .border_color = CUI_RGB(0x5383b3), .radius = 6, .font_size = 24, .width = CUI_FILL } };
    style_select_frame(ui, &s); style_select_frame(ui, &s);
    cui_id id = cui_make_id(ui, "select"), pid = id ^ 0x5E1Eu;
    CHECK(style_node(ui, id)->rect.h >= 30 * 1.3f + 12);
    CHECK(style_equal(style_node(ui, id)->bg, s.opts.bg));
    ui->focus = id; ui->focus_visible = true;
    style_select_key(ui, &s, CUI_KEY_DOWN);
    style_select_frame(ui, &s);
    CHECK(cui__popup_get(ui, pid, false)->open);
    CHECK(style_equal(style_node(ui, pid)->bg, s.opts.bg));
    CHECK(style_node(ui, pid)->radius.tl == 6);
    bool option = false;
    for (int i = 0; i < ui->node_count; i++) {
        cui_node *n = &ui->nodes[i];
        if (n->text && strcmp(n->text, "Beta") == 0) {
            option = true;
            CHECK(n->font_size == 30 && style_equal(n->text_color, s.opts.color));
            CHECK(ui->nodes[n->parent].rect.h >= 30 * 1.3f + 12);
        }
    }
    CHECK(option);
    s.opts.bg = CUI_RGB(0xffffff); s.opts.color = CUI_TRANSPARENT;
    style_select_frame(ui, &s);
    for (int i = 0; i < ui->node_count; i++) {
        cui_node *n = &ui->nodes[i];
        if (n->text && strcmp(n->text, "Alpha") == 0 && ui->nodes[n->parent].id != id) {
            CHECK(n->text_color.r < 120);
            CHECK(ui->nodes[n->parent].bg.r < 120 && ui->nodes[n->parent].bg.a > 0);
        }
    }
    s.opts.bg = CUI_RGB(0x24374a); s.opts.color = CUI_RGB(0xfafafa);
    style_select_frame(ui, &s);
    style_select_key(ui, &s, CUI_KEY_DOWN);
    style_select_key(ui, &s, CUI_KEY_ENTER);
    CHECK(s.index == 1 && !cui__popup_get(ui, pid, false)->open);
    s.opts.searchable = true;
    ui->focus = id; ui->focus_visible = true;
    style_select_key(ui, &s, CUI_KEY_DOWN);
    style_select_frame(ui, &s); style_select_frame(ui, &s);
    cui_input_text(ui, "Gamma"); style_select_frame(ui, &s);
    bool gamma = false, alpha = false;
    for (int i = 0; i < ui->node_count; i++) {
        cui_node *n = &ui->nodes[i];
        if (n->text && strcmp(n->text, "Gamma") == 0 && n->font_size == 30) gamma = true;
        if (n->text && strcmp(n->text, "Alpha") == 0) alpha = true;
    }
    CHECK(gamma && !alpha);
    style_select_key(ui, &s, CUI_KEY_ESCAPE);
    CHECK(!cui__popup_get(ui, pid, false)->open);
    s.opts.disabled = true;
    style_select_key(ui, &s, CUI_KEY_DOWN);
    CHECK(!cui__popup_get(ui, pid, false)->open);
    cui_destroy(ui);
}

typedef struct {
    cui_card_opts card;
    cui_badge_opts badge;
    cui_chip_opts chip;
    cui_tabs_opts tabs;
    bool selected;
    int tab, nav, badge_node, card_clicks;
    cui_id chip_id, nav_id[2];
} style_surfaces;

static void style_surface_frame(cui_ctx *ui, style_surfaces *s) {
    static const char *items[] = { "First", "Second", "Third" };
    cui_begin_frame(ui, 1.0f / 60);
    cui_column(ui, .width = CUI_FILL, .pad = 20, .gap = 12) {
        if (cui_card_begin(ui, &s->card)) s->card_clicks++;
        cui_text(ui, "Card content"); cui_card_end(ui);
        s->badge_node = ui->node_count;
        cui_badge_ex(ui, "Badge", &s->badge);
        cui_chip_ex(ui, "Chip", &s->selected, &s->chip); s->chip_id = cui_last_id(ui);
        cui_tabs_ex(ui, "tabs", items, 3, &s->tab, &s->tabs);
        for (int k = 0; k < 2; k++) {
            if (cui_nav_item(ui, items[k], .group = "custom-nav", .selected = s->nav == k,
                .bg = k == 0 ? CUI_RGB(0x345678) : CUI_RGB(0x24684a), .font_size = 24)) s->nav = k;
            s->nav_id[k] = cui_last_id(ui);
        }
    }
    cui_end_frame(ui);
}

static void test_style_surfaces(int dark, int rtl) {
    cui_ctx *ui = style_context(dark, rtl);
    style_surfaces s = {
        .card = { .id = "card", .bg = CUI_RGB(0x203044), .border_color = CUI_RGB(0x5383b3), .border = 3, .radius = -1, .hoverable = true },
        .badge = { .bg = CUI_RGB(0x446688), .color = CUI_RGB(0xfafafa), .radius = -1, .font_size = 24 },
        .chip = { .bg = CUI_RGB(0x446688), .color = CUI_RGB(0xfafafa), .radius = 6, .font_size = 24 },
        .tabs = { .variant = CUI_TABS_PILL, .bg = CUI_RGB(0x476523), .font_size = 24 }, .nav = 1 };
    for (int i = 0; i < 4; i++) style_surface_frame(ui, &s);
    cui_node *card = style_node(ui, cui_make_id(ui, "card"));
    CHECK(style_equal(card->bg, s.card.bg) && style_equal(card->border_color, s.card.border_color));
    CHECK(card->border == 3 && card->radius.tl == 0);
    cui_node *badge = &ui->nodes[s.badge_node];
    CHECK(badge->radius.tl == 0 && badge->rect.h >= 30 * 1.2f + 8);
    CHECK(ui->nodes[badge->first].font_size == 30);
    CHECK(style_node(ui, s.chip_id)->rect.h >= 30 * 1.3f + 10);
    CHECK(style_node(ui, s.chip_id)->radius.tl == 6);
    CHECK(style_drawn(ui, s.tabs.bg, CUI_SHADE_RRECT));
    CHECK(style_drawn(ui, CUI_RGB(0x24684a), CUI_SHADE_RRECT));
    CHECK(!style_drawn(ui, CUI_RGB(0x345678), CUI_SHADE_RRECT));
    CHECK(style_node(ui, s.nav_id[1])->rect.h >= 30 * 1.3f + 10);
    int zero = 0;
    ui->focus = cui__hash(&zero, sizeof zero, cui_make_id(ui, "tabs")); ui->focus_visible = true;
    cui_input_key(ui, CUI_KEY_RIGHT, true, 0); cui_input_key(ui, CUI_KEY_RIGHT, false, 0);
    style_surface_frame(ui, &s);
    CHECK(s.tab == 1);
    ui->focus = s.chip_id; ui->focus_visible = true;
    cui_input_key(ui, CUI_KEY_SPACE, true, 0); cui_input_key(ui, CUI_KEY_SPACE, false, 0);
    style_surface_frame(ui, &s);
    CHECK(s.selected && style_node(ui, s.chip_id)->paint);
    bool check_icon = false;
    for (int i = 0; i < ui->node_count; i++)
        if (ui->nodes[i].text && strcmp(ui->nodes[i].text, CUI_ICON_CHECK) == 0) check_icon = true;
    CHECK(check_icon);
    s.chip.disabled = true;
    cui_input_key(ui, CUI_KEY_SPACE, true, 0); cui_input_key(ui, CUI_KEY_SPACE, false, 0);
    style_surface_frame(ui, &s);
    CHECK(s.selected && style_node(ui, s.chip_id)->opacity == CUI__DISABLED);
    s.nav = 0;
    cui_set_reduced_motion(ui, false);
    for (int i = 0; i < 30; i++) style_surface_frame(ui, &s);
    CHECK(style_drawn(ui, CUI_RGB(0x345678), CUI_SHADE_RRECT));
    s.card.selected = true;
    s.tabs.variant = CUI_TABS_UNDERLINE;
    style_surface_frame(ui, &s);
    CHECK(style_equal(style_node(ui, cui_make_id(ui, "card"))->border_color, CUI_C(ui, ACCENT)));
    CHECK(style_drawn(ui, s.tabs.bg, CUI_SHADE_RRECT));
    s.tabs.variant = CUI_TABS_PILL; s.tabs.full_width = true;
    style_surface_frame(ui, &s);
    CHECK(style_node(ui, cui_make_id(ui, "tabs"))->rect.w <= 320);
    ui->focus = cui_make_id(ui, "card"); ui->focus_visible = true;
    cui_input_key(ui, CUI_KEY_ENTER, true, 0); cui_input_key(ui, CUI_KEY_ENTER, false, 0);
    style_surface_frame(ui, &s);
    CHECK(s.card_clicks == 1 && style_node(ui, ui->focus)->paint);
    s.card.border = -1;
    style_surface_frame(ui, &s);
    CHECK(style_node(ui, cui_make_id(ui, "card"))->border == 0);
    cui_destroy(ui);
}

int main(void) {
    test_style_button_contrast();
    for (int dark = 0; dark < 2; dark++) for (int rtl = 0; rtl < 2; rtl++) {
        test_style_fields(dark, rtl);
        test_style_select(dark, rtl);
        test_style_surfaces(dark, rtl);
    }
    printf("component styles: %d checks, %d failed\n", style_checks, style_failed);
    return style_failed ? 1 : 0;
}
