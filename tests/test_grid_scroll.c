/* Inspect CUI's actual offset to catch stale ranges on the movement frame. */
#include "cui_internal.h"
#include <stdio.h>

enum { ROWS = 1000000 };
static int first, last, cursor;

static void frame(cui_ctx *ui) {
    static const cui_grid_column columns[] = { { .title = "ID", .width = 200 } };
    cui_begin_frame(ui, 1.0f / 60.0f);
    if (cui_data_grid_begin(ui, "grid", columns, 1, ROWS, &first, &last,
                            .height = CUI_PX(500), .row_height = 32, .cursor = &cursor)) {
        for (int row = first; row < last; row++) {
            cui_data_grid_row(ui, row);
            cui_data_grid_cell_text(ui, "row");
        }
        cui_data_grid_end(ui);
    }
    cui_end_frame(ui);
}

static int check_frame(cui_ctx *ui) {
    frame(ui);
    cui_id sid = cui_make_id(ui, "grid") ^ 0x5C011u;
    cui_scroll_state *scroll = cui_state(ui, sid, sizeof *scroll);
    int visible_first = (int)floorf(scroll->y / 32);
    int visible_last = (int)ceilf((scroll->y + scroll->view_h - 40) / 32);
    if (visible_last > ROWS) visible_last = ROWS;
    if (first < 0 || first > visible_first || last < visible_last ||
        last > ROWS || last - first > 100) {
        fprintf(stderr, "FAIL: built [%d,%d), viewport needs [%d,%d)\n",
                first, last, visible_first, visible_last);
        return 1;
    }
    return 0;
}

int main(void) {
    int result = 0;
    for (int reduced = 0; reduced < 2; reduced++) {
        cui_ctx *ui = cui_create(NULL);
        if (!ui) return 1;
        cui_set_viewport(ui, 800, 600, 1);
        cui_set_reduced_motion(ui, reduced != 0);
        cursor = 0;
        frame(ui); frame(ui);
        cui_rect rect = cui_get_rect(ui, cui_make_id(ui, "grid") ^ 0x5C011u);
        float x = rect.x + rect.w - 4;
        cui_input_mouse_move(ui, x, rect.y + 12); frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true); frame(ui);
        const float positions[] = { 0.5f, 0.95f, 0.1f, 1.0f, 0.0f };
        for (int p = 0; p < 5; p++) {
            cui_input_mouse_move(ui, x, rect.y + rect.h * positions[p]);
            result |= check_frame(ui);
        }
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false); frame(ui);
        cui_input_mouse_move(ui, 100, 200); frame(ui);
        cui_input_scroll(ui, 0, -100000);
        for (int f = 0; f < 10; f++) result |= check_frame(ui);
        const int targets[] = { ROWS - 1, 0, ROWS / 2 };
        for (int t = 0; t < 3; t++) {
            cursor = targets[t];
            for (int f = 0; f < 10; f++) result |= check_frame(ui);
        }
        cui_destroy(ui);
    }
    if (!result) puts("PASS: drag, wheel and cursor jumps cover the viewport on every frame.");
    return result;
}
