/* A native workspace with dockable tabs and resizable panes. */
#include <cui/cui_app.h>

#include <stdio.h>

typedef struct {
    bool hide_inspector, dark, reduced_motion;
    char text[2048];
} app_state;

static void frame(cui_ctx *ui, void *user) {
    app_state *s = user;
    const char *layout = "dock1 h0.20([*Files],h0.74(v0.72([*Editor],[*Output]),[*Inspector]))";
    bool show_inspector = !s->hide_inspector;
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .bg = CUI_COLOR_BACKGROUND) {
        cui_row(ui, .width = CUI_FILL, .pad = 12, .gap = 12, .wrap = true) {
            if (cui_button(ui, "Reset layout", .size = CUI_SIZE_SM)) cui_dock_load(ui, "workspace", layout);
            cui_checkbox(ui, "Inspector", &show_inspector);
            if (cui_checkbox(ui, "Dark theme", &s->dark)) cui_set_theme(ui, s->dark ? CUI_THEME_DARK : CUI_THEME_LIGHT);
            if (cui_checkbox(ui, "Reduce motion", &s->reduced_motion)) cui_set_reduced_motion(ui, s->reduced_motion);
        }
        cui_dockspace(ui, "workspace", .layout = layout, .shortcuts = true) {
            cui_dock_panel(ui, "Files", NULL, .icon = CUI_ICON_FOLDER) {
                cui_column(ui, .width = CUI_FILL, .pad = 12, .gap = 8) {
                    cui_label(ui, "Example project");
                    cui_secondary(ui, "main.c");
                    cui_secondary(ui, "README.md");
                }
            }
            cui_dock_panel(ui, "Editor", NULL, .icon = CUI_ICON_CODE) {
                cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 12, .gap = 8) {
                    cui_secondary(ui, "Drag a tab to dock it. Drag a divider to resize.");
                    cui_textarea(ui, "source", s->text, sizeof s->text, .width = CUI_FILL, .height = -1,
                                 .placeholder = "Type here; this demo keeps edits in memory.");
                }
            }
            cui_dock_panel(ui, "Output", NULL, .icon = CUI_ICON_SQUARE_TERMINAL) {
                cui_column(ui, .width = CUI_FILL, .pad = 12, .gap = 8) {
                    cui_text(ui, "Workspace ready.");
                    cui_secondary(ui, "Focus a pane, then use Ctrl+Tab to switch tabs or Ctrl+W to close the Inspector.");
                }
            }
            cui_dock_panel(ui, "Inspector", &show_inspector, .icon = CUI_ICON_SETTINGS) {
                cui_column(ui, .width = CUI_FILL, .pad = 12, .gap = 12) {
                    cui_label(ui, "Panel controls");
                    cui_secondary(ui, "Close this tab and use the Inspector checkbox to reopen it.");
                }
            }
        }
    }
    s->hide_inspector = !show_inspector;
}

int main(void) {
    app_state state = { 0 };
    return cui_app_run(&(cui_app_desc){ .title = "Docking", .width = 1100, .height = 720 }, frame, &state);
}
