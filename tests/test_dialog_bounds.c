/* Exercise native dialog builders with process I/O stubbed: no OS dialogs open. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *dialog_output = "/tmp/report.txt\n";
static int launched, exit_status;
static FILE *dialog_popen(const char *cmd, const char *mode) {
    (void)cmd; (void)mode;
    launched++;
    FILE *f = tmpfile();
    if (f) { fputs(dialog_output, f); rewind(f); }
    return f;
}
static int dialog_pclose(FILE *f) { fclose(f); return exit_status; }
#if !defined(__APPLE__)
static int dialog_system(const char *cmd) { (void)cmd; return 0; }
#define system dialog_system
#endif
#define popen dialog_popen
#define pclose dialog_pclose
#include "../src/platform/cui_dialogs.c"
#undef popen
#undef pclose
#if !defined(__APPLE__)
#undef system
#endif

static int failed, checks;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void) {
    char out[128] = "", filter[1024] = "";
    char quoted[9];
    CHECK(cui__shell_quote(quoted, sizeof quoted, "a'b") && !strcmp(quoted, "'a'\\''b'"));
    CHECK(!cui__shell_quote(quoted, 8, "a'b"));
    CHECK(!cui__shell_quote(quoted, 0, ""));
    cui_file_dialog_opts opts = { .kind = CUI_FILE_OPEN, .filter = "txt;md" };
    CHECK(cui__plat_file_dialog(NULL, &opts, out, sizeof out) && !strcmp(out, "/tmp/report.txt"));
    char exact[sizeof "/tmp/report.txt"];
    CHECK(cui__plat_file_dialog(NULL, &opts, exact, sizeof exact) && !strcmp(exact, "/tmp/report.txt"));
    CHECK(!cui__plat_file_dialog(NULL, &opts, out, 8) && out[0] == 0);
    dialog_output = "/tmp/report.txt";
    CHECK(cui__plat_file_dialog(NULL, &opts, exact, sizeof exact) && !strcmp(exact, dialog_output));
    dialog_output = "";
    CHECK(!cui__plat_file_dialog(NULL, &opts, out, sizeof out));
    dialog_output = "/tmp/report.txt\n"; exit_status = 1;
    CHECK(!cui__plat_file_dialog(NULL, &opts, out, sizeof out) && out[0] == 0);
    exit_status = 0; launched = 0;
    for (int i = 0; i < 60; i++) strcat(filter, i ? ";extension0" : "extension0");
    opts.filter = filter;
    CHECK(!cui__plat_file_dialog(NULL, &opts, out, sizeof out) && launched == 0);
    memset(filter, 'x', sizeof filter - 1); filter[sizeof filter - 1] = 0;
    CHECK(!cui__plat_file_dialog(NULL, &opts, out, sizeof out) && launched == 0);
    printf("dialog bounds: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
