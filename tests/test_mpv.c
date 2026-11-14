/* Real libmpv + software renderer, with no native window or audio device. */
#include <mpv/client.h>
static int mpv_test_option(mpv_handle *mpv, const char *name, const char *value);
#define mpv_set_option_string mpv_test_option
#define cui_app_wakeup cui_mpv_test_wakeup
#include "../addons/video/cui_mpv.c"
#undef cui_app_wakeup
#undef mpv_set_option_string
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <time.h>
#include <unistd.h>
#endif

void cui_mpv_test_wakeup(cui_app *app) { (void)app; }
static int mpv_test_option(mpv_handle *mpv, const char *name, const char *value) {
    int error = mpv_set_option_string(mpv, name, value);
    /* Exercise builds without Lua/plugins, while disabling them in real libmpv. */
    if (!strcmp(name, "osc") || !strcmp(name, "ytdl") || !strcmp(name, "load-scripts"))
        return MPV_ERROR_OPTION_NOT_FOUND;
    return error;
}
static int mpv_checks, mpv_failed;
#define CHECK(c) do { mpv_checks++; if (!(c)) { mpv_failed++; fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); } } while (0)
static void mpv_test_delay(void) {
#ifdef _WIN32
    Sleep(10);
#else
    struct timespec delay = { 0, 10000000 };
    nanosleep(&delay, NULL);
#endif
}

static bool mpv_test_fixture(const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool ok = fprintf(file, "YUV4MPEG2 W16 H16 F30:1 Ip A1:1 C420jpeg\n") > 0;
    unsigned char pixels[16 * 16 * 3 / 2];
    memset(pixels, 81, 256); memset(pixels + 256, 90, 64); memset(pixels + 320, 240, 64);
    for (int i = 0; i < 60 && ok; i++)
        ok = fwrite("FRAME\n", 1, 6, file) == 6 && fwrite(pixels, 1, sizeof pixels, file) == sizeof pixels;
    return fclose(file) == 0 && ok;
}

int main(void) {
    char directory[4096], path[4200];
#ifdef _WIN32
    CHECK(_getcwd(directory, sizeof directory) != NULL);
#else
    CHECK(getcwd(directory, sizeof directory) != NULL);
#endif
    snprintf(path, sizeof path, "%s/cui-test-mpv.y4m", directory);
    CHECK(mpv_test_fixture(path));
    cui_app app = {0};
    app.sw = cui_sw_create(); app.software = true;
    app.ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(app.sw) });
    int error = 1;
    cui_mpv *p = cui_mpv_create(&app, &error);
    CHECK(p && error == 0);
    if (!p) { fprintf(stderr, "libmpv init: %s\n", cui_mpv_error_string(error)); return 1; }
    CHECK(!cui_mpv_update(p, 31, 19));   /* no output allocation before a file */
    CHECK(cui_mpv_state(p)->frame == 0);
    CHECK(cui_mpv_load(p, NULL) < 0 && cui_mpv_load(p, "") < 0);
    CHECK(cui_mpv_load(p, path) == 0);
    bool rendered = false;
    for (int i = 0; i < 1000; i++) {
        rendered |= cui_mpv_update(p, 31, 19);
        if (rendered && p->video.position > 0.05f && p->video.duration > 0) break;
        mpv_test_delay();
    }
    CHECK(rendered && cui_mpv_last_error(p) == 0);
    if (!p->pixels) {
        fprintf(stderr, "playback: %s; position=%g duration=%g playing=%d\n",
            cui_mpv_error_string(cui_mpv_last_error(p)), p->video.position, p->video.duration, p->video.playing);
        goto cleanup;
    }
    CHECK(p->video.frame != 0 && p->video.playing && p->video.duration > 1);
    CHECK(p->video.aspect > 0.99f && p->video.aspect < 1.01f);
    CHECK((uintptr_t)p->pixels % 64 == 0);
    /* The actual decoded image must be red, including its last unaligned row. */
    CHECK(p->pixels[0] > 180 && p->pixels[1] < 80 && p->pixels[2] < 80);
    CHECK(p->pixels[18 * 128] > 180);
    cui_texture texture = p->video.frame;
    void *storage = p->pixel_storage;
    cui_video edited = *cui_mpv_state(p);
    edited.playing = false; edited.volume = 0.25f; edited.muted = true;
    CHECK(cui_mpv_apply(p, &edited, CUI_PLAYER_TOGGLE | CUI_PLAYER_VOLUME | CUI_PLAYER_MUTE) == 0);
    for (int i = 0; i < 200; i++) {
        cui_mpv_update(p, 31, 19);
        if (!p->video.playing && p->video.muted && fabsf(p->video.volume - 0.25f) < 0.001f) break;
        mpv_test_delay();
    }
    CHECK(!p->video.playing && p->video.muted && fabsf(p->video.volume - 0.25f) < 0.001f);
    CHECK(texture == p->video.frame && storage == p->pixel_storage);
    for (int i = 0; i < 20; i++) { mpv_test_delay(); cui_mpv_update(p, 31, 19); }
    CHECK(!cui_mpv_update(p, 31, 19));  /* paused frames do not upload again */
    edited.position = 0.8f;
    CHECK(cui_mpv_apply(p, &edited, CUI_PLAYER_SEEK) == 0);
    for (int i = 0; i < 200; i++) {
        cui_mpv_update(p, 31, 19);
        if (p->video.position > 0.7f) break;
        mpv_test_delay();
    }
    CHECK(p->video.position > 0.7f && !p->video.playing);
    CHECK(cui_mpv_update(p, 17, 11));
    CHECK(p->width == 17 && p->height == 11 && (uintptr_t)p->pixels % 64 == 0);
    CHECK(cui_mpv_update(p, 0, 0));
    CHECK(!p->video.frame && !p->pixel_storage);
    CHECK(cui_mpv_update(p, 31, 19));  /* paused frame comes back after hiding */
    edited.position = 1.95f; edited.playing = true;
    CHECK(cui_mpv_apply(p, &edited, CUI_PLAYER_SEEK | CUI_PLAYER_TOGGLE) == 0);
    for (int i = 0; i < 200 && !p->eof; i++) { cui_mpv_update(p, 31, 19); mpv_test_delay(); }
    CHECK(p->eof && !p->video.playing);
    CHECK(cui_mpv_apply(p, &edited, CUI_PLAYER_TOGGLE) == 0);
    for (int i = 0; i < 200; i++) {
        cui_mpv_update(p, 31, 19);
        if (!p->eof && p->video.playing && p->video.position < 0.5f) break;
        mpv_test_delay();
    }
    CHECK(!p->eof && p->video.playing && p->video.position < 0.5f);
    float before_hidden = p->video.position;
    for (int i = 0; i < 20; i++) { cui_mpv_update(p, 0, 0); mpv_test_delay(); }
    CHECK(p->video.position > before_hidden && !p->video.frame && cui_mpv_last_error(p) == 0);
    CHECK(cui_mpv_update(p, 31, 19));
    edited.position = NAN;
    CHECK(cui_mpv_apply(p, &edited, CUI_PLAYER_SEEK) < 0);
    edited.volume = INFINITY;
    CHECK(cui_mpv_apply(p, &edited, CUI_PLAYER_VOLUME) < 0);
    CHECK(!cui_mpv_update(p, INT_MAX, INT_MAX));
    CHECK(cui_mpv_last_error(p) < 0);
    CHECK(cui_mpv_load(p, "cui-mpv-missing-file.invalid") == 0);
    for (int i = 0; i < 200 && !cui_mpv_last_error(p); i++) { cui_mpv_update(p, 0, 0); mpv_test_delay(); }
    CHECK(cui_mpv_last_error(p) < 0);
    CHECK(cui_mpv_load(p, "cui-mpv-missing-file.invalid") == 0);
    CHECK(cui_mpv_load(p, path) == 0);
    for (int i = 0; i < 300; i++) {
        cui_mpv_update(p, 31, 19);
        if (p->video.playing && p->video.position > 0.05f) break;
        mpv_test_delay();
    }
    CHECK(p->video.playing && p->video.position > 0.05f && cui_mpv_last_error(p) == 0);
    /* Reload clears the error, and destruction while decoding joins callbacks. */
    CHECK(cui_mpv_load(p, path) == 0 && cui_mpv_last_error(p) == 0);
cleanup:
    cui_mpv_destroy(p);
    cui_mpv_destroy(NULL);
    cui_destroy(app.ui); cui_sw_destroy(app.sw);
    CHECK(remove(path) == 0);
    printf("libmpv: %d checks, %d failed\n", mpv_checks, mpv_failed);
    return mpv_failed != 0;
}
