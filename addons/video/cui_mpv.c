/* libmpv owns decoding, audio and the clock; CUI owns the output texture. */
#include "cui/cui_mpv.h"
#include "../../src/platform/cui_platform.h"
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <limits.h>
#include <float.h>

#if MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2, 0)
#error "CUI video requires libmpv client API 2.0 or newer (mpv 0.35+)"
#endif

#ifdef _WIN32
#define CUI__MPV_GLAPI __stdcall
#else
#define CUI__MPV_GLAPI
#endif

typedef struct {
    void (CUI__MPV_GLAPI *GenFramebuffers)(int, unsigned int *);
    void (CUI__MPV_GLAPI *DeleteFramebuffers)(int, const unsigned int *);
    void (CUI__MPV_GLAPI *BindFramebuffer)(unsigned int, unsigned int);
    void (CUI__MPV_GLAPI *FramebufferTexture2D)(unsigned int, unsigned int, unsigned int, unsigned int, int);
    unsigned int (CUI__MPV_GLAPI *CheckFramebufferStatus)(unsigned int);
    void (CUI__MPV_GLAPI *GetIntegerv)(unsigned int, int *);
    void (CUI__MPV_GLAPI *Disable)(unsigned int);
    void (CUI__MPV_GLAPI *BindBuffer)(unsigned int, unsigned int);
    void (CUI__MPV_GLAPI *BindTexture)(unsigned int, unsigned int);
    void (CUI__MPV_GLAPI *ActiveTexture)(unsigned int);
    void (CUI__MPV_GLAPI *PixelStorei)(unsigned int, int);
} cui__mpv_gl;

struct cui_mpv {
    cui_app *app;
    mpv_handle *mpv;
    mpv_render_context *render;
    cui_video video;
    cui__mpv_gl gl;
    unsigned int fbo;
    unsigned char *pixels;
    void *pixel_storage;
    int width, height, error;
    bool paused, idle, eof, seekable;
    float duration;
    cui_mpv_track *tracks;
    int track_count[2];
    int64_t selected_track[2];
};

static void cui__mpv_wakeup(void *user) {
    /* libmpv calls on worker threads; no UI, GL or libmpv calls here. */
    cui_mpv *p = user;
    cui_app_wakeup(p->app);
}

static void *cui__mpv_proc(void *user, const char *name) {
    (void)user;
    return cui__plat_gl_proc(name);
}

static void cui__mpv_gl_defaults(cui_mpv *p) {
    /* CUI leaves these bindings set; libmpv requires GL defaults at entry. */
    p->gl.Disable(0x0BE2);                     /* GL_BLEND */
    p->gl.Disable(0x0C11);                     /* GL_SCISSOR_TEST */
    p->gl.BindBuffer(0x8892, 0);               /* GL_ARRAY_BUFFER */
    p->gl.BindBuffer(0x88EC, 0);               /* GL_PIXEL_UNPACK_BUFFER */
    p->gl.ActiveTexture(0x84C0);               /* GL_TEXTURE0 */
    p->gl.BindTexture(0x0DE1, 0);              /* GL_TEXTURE_2D */
    p->gl.PixelStorei(0x0CF5, 4);              /* GL_UNPACK_ALIGNMENT */
    p->gl.PixelStorei(0x0CF2, 0);              /* GL_UNPACK_ROW_LENGTH */
}

static void cui__mpv_release_output(cui_mpv *p) {
    if (p->fbo) p->gl.DeleteFramebuffers(1, &p->fbo);
    p->fbo = 0;
    if (p->video.frame) cui_image_destroy(p->app->ui, p->video.frame);
    p->video.frame = 0;
    free(p->pixel_storage);
    p->pixel_storage = NULL;
    p->pixels = NULL;
    p->width = p->height = 0;
}

static int cui__mpv_init_gl(cui_mpv *p) {
#define CUI__MPV_LOAD(name) do { *(void **)(&p->gl.name) = cui__plat_gl_proc("gl" #name); \
    if (!p->gl.name) return MPV_ERROR_UNSUPPORTED; } while (0)
    CUI__MPV_LOAD(GenFramebuffers); CUI__MPV_LOAD(DeleteFramebuffers);
    CUI__MPV_LOAD(BindFramebuffer); CUI__MPV_LOAD(FramebufferTexture2D);
    CUI__MPV_LOAD(CheckFramebufferStatus); CUI__MPV_LOAD(GetIntegerv);
    CUI__MPV_LOAD(Disable); CUI__MPV_LOAD(BindBuffer); CUI__MPV_LOAD(BindTexture);
    CUI__MPV_LOAD(ActiveTexture); CUI__MPV_LOAD(PixelStorei);
#undef CUI__MPV_LOAD
    mpv_opengl_init_params init = { cui__mpv_proc, NULL };
    int advanced = 1;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init },
        { MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced }, { 0, NULL }
    };
    cui__mpv_gl_defaults(p);
    return mpv_render_context_create(&p->render, p->mpv, params);
}

static int cui__mpv_init(cui_mpv *p) {
    /* Bound demuxer RAM and read-ahead without adding a second frame queue.
     * Decoder/reference surfaces and network protocol buffers are additional. */
    /* shortcut: fixed cache budgets; expose tuning when high-bitrate network playback needs it. */
    static const char *const options[][2] = {
        { "config", "no" }, { "terminal", "no" },
        { "input-default-bindings", "no" }, { "input-vo-keyboard", "no" },
        { "osd-level", "0" }, { "vo", "libmpv" }, { "keepaspect", "no" },
        { "idle", "yes" }, { "keep-open", "yes" }, { "pause", "yes" },
        { "video-sync", "audio" }, { "video-timing-offset", "0" },
        { "demuxer-max-bytes", "16MiB" }, { "demuxer-max-back-bytes", "4MiB" },
        { "demuxer-readahead-secs", "2" }, { "cache-secs", "2" }
    };
    for (size_t i = 0; i < sizeof options / sizeof *options; i++) {
        int err = mpv_set_option_string(p->mpv, options[i][0], options[i][1]);
        if (err < 0) return err;
    }
    /* Minimal libmpv builds omit the scripting options altogether. */
    static const char *const scripts[] = { "load-scripts", "osc", "ytdl" };
    for (size_t i = 0; i < sizeof scripts / sizeof *scripts; i++) {
        int err = mpv_set_option_string(p->mpv, scripts[i], "no");
        if (err < 0 && err != MPV_ERROR_OPTION_NOT_FOUND) return err;
    }
    int err = mpv_set_option_string(p->mpv, "hwdec", p->app->gl ? "auto" : "auto-copy");
    if (err < 0) return err;
    err = mpv_initialize(p->mpv);
    if (err < 0) return err;
    static const struct { const char *name; mpv_format format; } properties[] = {
        { "pause", MPV_FORMAT_FLAG }, { "idle-active", MPV_FORMAT_FLAG },
        { "eof-reached", MPV_FORMAT_FLAG }, { "seekable", MPV_FORMAT_FLAG },
        { "time-pos", MPV_FORMAT_DOUBLE }, { "duration", MPV_FORMAT_DOUBLE },
        { "volume", MPV_FORMAT_DOUBLE }, { "mute", MPV_FORMAT_FLAG },
        { "video-params/aspect", MPV_FORMAT_DOUBLE }, { "track-list", MPV_FORMAT_NODE },
        { "aid", MPV_FORMAT_NODE }, { "sid", MPV_FORMAT_NODE }
    };
    for (size_t i = 0; i < sizeof properties / sizeof *properties; i++) {
        err = mpv_observe_property(p->mpv, i + 1, properties[i].name, properties[i].format);
        if (err < 0) return err;
    }
    if (p->app->gl) return cui__mpv_init_gl(p);
    int advanced = 1;
    mpv_render_param params[] = { { MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_SW },
        { MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced }, { 0, NULL } };
    return mpv_render_context_create(&p->render, p->mpv, params);
}

cui_mpv *cui_mpv_create(cui_app *app, int *error) {
    if (error) *error = 0;
    app = cui__app_or_current(app);
    if (!app || !app->ui || !app->ui->renderer.texture_create || !app->ui->renderer.texture_update ||
        !app->ui->renderer.texture_destroy) { if (error) *error = MPV_ERROR_INVALID_PARAMETER; return NULL; }
    cui_mpv *p = calloc(1, sizeof *p);
    if (!p) { if (error) *error = MPV_ERROR_NOMEM; return NULL; }
    p->app = app;
    p->paused = p->idle = true;
    p->video.volume = 1;
    p->mpv = mpv_create();
    int err = p->mpv ? cui__mpv_init(p) : MPV_ERROR_NOMEM;
    if (err < 0) {
        if (error) *error = err;
        cui_mpv_destroy(p);
        return NULL;
    }
    mpv_set_wakeup_callback(p->mpv, cui__mpv_wakeup, p);
    mpv_render_context_set_update_callback(p->render, cui__mpv_wakeup, p);
    return p;
}

void cui_mpv_destroy(cui_mpv *p) {
    if (!p) return;
    if (p->mpv) mpv_set_wakeup_callback(p->mpv, NULL, NULL);
    if (p->render) {
        mpv_render_context_set_update_callback(p->render, NULL, NULL);
        if (p->app->gl) cui__mpv_gl_defaults(p);
        mpv_render_context_free(p->render);
    }
    if (p->mpv) mpv_terminate_destroy(p->mpv);
    cui__mpv_release_output(p);
    free(p->tracks);
    free(p);
}

int cui_mpv_load(cui_mpv *p, const char *path) {
    if (!p || !path || !*path) return MPV_ERROR_INVALID_PARAMETER;
    const char *args[] = { "loadfile", path, "replace", NULL };
    int err = mpv_command_async(p->mpv, 0, args);
    if (err < 0) return p->error = err;
    int pause = 0;
    p->error = mpv_set_property_async(p->mpv, 0, "pause", MPV_FORMAT_FLAG, &pause);
    return p->error;
}

typedef struct {
    int64_t id;
    int type;
    const char *title, *language, *filename;
    bool external;
} cui__mpv_track_info;

static bool cui__mpv_read_track(const mpv_node *node, cui__mpv_track_info *info) {
    memset(info, 0, sizeof *info);
    info->type = -1;
    info->title = info->language = info->filename = "";
    if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list || node->u.list->num < 0) return false;
    const mpv_node_list *map = node->u.list;
    if (map->num && (!map->keys || !map->values)) return false;
    for (int i = 0; i < map->num; i++) {
        const char *key = map->keys[i];
        const mpv_node *value = &map->values[i];
        if (!key) continue;
        if (!strcmp(key, "id") && value->format == MPV_FORMAT_INT64) info->id = value->u.int64;
        else if (!strcmp(key, "external") && value->format == MPV_FORMAT_FLAG) info->external = value->u.flag != 0;
        else if (value->format == MPV_FORMAT_STRING && value->u.string) {
            if (!strcmp(key, "type")) {
                if (!strcmp(value->u.string, "audio")) info->type = CUI_MPV_AUDIO;
                else if (!strcmp(value->u.string, "sub")) info->type = CUI_MPV_SUBTITLE;
            } else if (!strcmp(key, "title")) info->title = value->u.string;
            else if (!strcmp(key, "lang")) info->language = value->u.string;
            else if (!strcmp(key, "external-filename")) info->filename = value->u.string;
        }
    }
    if (!*info->title && info->external) {
        const char *slash = strrchr(info->filename, '/'), *backslash = strrchr(info->filename, '\\');
        if (backslash && (!slash || backslash > slash)) slash = backslash;
        info->title = slash ? slash + 1 : info->filename;
    }
    return info->id > 0 && info->type >= 0;
}

static void cui__mpv_clear_tracks(cui_mpv *p) {
    free(p->tracks); p->tracks = NULL;
    p->track_count[0] = p->track_count[1] = 0;
}

static void cui__mpv_track_list(cui_mpv *p, const mpv_node *node) {
    if (!node) { cui__mpv_clear_tracks(p); return; }
    if (node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list || node->u.list->num < 0 ||
        (node->u.list->num && !node->u.list->values)) { p->error = MPV_ERROR_PROPERTY_FORMAT; return; }
    const mpv_node_list *list = node->u.list;
    int count[2] = { 0, 0 };
    size_t bytes = 0;
    for (int i = 0; i < list->num; i++) {
        cui__mpv_track_info info;
        if (!cui__mpv_read_track(&list->values[i], &info)) continue;
        size_t title = strlen(info.title), language = strlen(info.language);
        if (title > SIZE_MAX - bytes || language > (SIZE_MAX - bytes - title) / 2 ||
            SIZE_MAX - bytes - title - language * 2 < 65) { p->error = MPV_ERROR_NOMEM; return; }
        bytes += title + language * 2 + 65;
        count[info.type]++;
    }
    size_t total = (size_t)count[0] + (size_t)count[1];
    if (!total) {
        free(p->tracks); p->tracks = NULL;
        p->track_count[0] = p->track_count[1] = 0;
        return;
    }
    if (total > (SIZE_MAX - bytes) / sizeof(cui_mpv_track)) { p->error = MPV_ERROR_NOMEM; return; }
    cui_mpv_track *tracks = malloc(total * sizeof *tracks + bytes);
    if (!tracks) { p->error = MPV_ERROR_NOMEM; return; }
    char *text = (char *)(tracks + total);
    int cursor[2] = { 0, count[0] };
    for (int i = 0; i < list->num; i++) {
        cui__mpv_track_info info;
        if (!cui__mpv_read_track(&list->values[i], &info)) continue;
        cui_mpv_track *track = &tracks[cursor[info.type]++];
        track->id = info.id; track->external = info.external; track->selected = false;
        track->language = text;
        size_t language = strlen(info.language) + 1;
        memcpy(text, info.language, language); text += language;
        track->label = text;
        size_t cap = strlen(info.title) + language + 63;
        const char *prefix = *info.language ? " (" : "", *suffix = *info.language ? ")" : "";
        int length;
        if (*info.title) length = snprintf(text, cap, "%s%s%s%s", info.title, prefix, info.language, suffix);
        else length = snprintf(text, cap, "%s %lld%s%s%s", info.type == CUI_MPV_AUDIO ? "Audio track" : "Subtitle track",
            (long long)info.id, prefix, info.language, suffix);
        if (length < 0 || (size_t)length >= cap) { free(tracks); p->error = MPV_ERROR_PROPERTY_FORMAT; return; }
        text += (size_t)length + 1;
    }
    free(p->tracks);
    p->tracks = tracks; p->track_count[0] = count[0]; p->track_count[1] = count[1];
}

static void cui__mpv_property(cui_mpv *p, const mpv_event_property *prop) {
    if (!prop) return;
    if (!strcmp(prop->name, "track-list")) {
        if (prop->format == MPV_FORMAT_NODE) cui__mpv_track_list(p, prop->data);
        else if (prop->format == MPV_FORMAT_NONE) cui__mpv_track_list(p, NULL);
        return;
    }
    if (!strcmp(prop->name, "aid") || !strcmp(prop->name, "sid")) {
        int type = !strcmp(prop->name, "aid") ? CUI_MPV_AUDIO : CUI_MPV_SUBTITLE;
        const mpv_node *node = prop->format == MPV_FORMAT_NODE ? prop->data : NULL;
        p->selected_track[type] = node && node->format == MPV_FORMAT_INT64 ? node->u.int64 : CUI_MPV_TRACK_OFF;
        return;
    }
    if (!prop->data) return;
    bool flag = prop->format == MPV_FORMAT_FLAG && *(int *)prop->data != 0;
    double value = prop->format == MPV_FORMAT_DOUBLE ? *(double *)prop->data : 0;
    if (!isfinite(value) || value > (double)FLT_MAX || value < -(double)FLT_MAX) value = 0;
    if (!strcmp(prop->name, "pause")) p->paused = flag;
    else if (!strcmp(prop->name, "idle-active")) p->idle = flag;
    else if (!strcmp(prop->name, "eof-reached")) p->eof = flag;
    else if (!strcmp(prop->name, "seekable")) p->seekable = flag;
    else if (!strcmp(prop->name, "time-pos")) p->video.position = (float)CUI_MAX(0, value);
    else if (!strcmp(prop->name, "duration")) p->duration = (float)CUI_MAX(0, value);
    else if (!strcmp(prop->name, "volume")) p->video.volume = (float)CUI_CLAMP(value / 100, 0, 1);
    else if (!strcmp(prop->name, "mute")) p->video.muted = flag;
    else if (!strcmp(prop->name, "video-params/aspect")) p->video.aspect = (float)CUI_MAX(0, value);
}

static void cui__mpv_events(cui_mpv *p) {
    for (;;) {
        mpv_event *event = mpv_wait_event(p->mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        if (event->error < 0) p->error = event->error;
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) cui__mpv_property(p, event->data);
        else if (event->event_id == MPV_EVENT_START_FILE) {
            cui__mpv_clear_tracks(p);
            p->error = 0;  /* a replacement starts after the previous file's end/error event */
            p->duration = p->video.position = 0;
            p->seekable = p->eof = false;
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            mpv_event_end_file *end = event->data;
            if (end->error < 0) p->error = end->error;
        } else if (event->event_id == MPV_EVENT_QUEUE_OVERFLOW) p->error = MPV_ERROR_EVENT_QUEUE_FULL;
    }
    p->video.playing = !p->paused && !p->idle && !p->eof;
    p->video.duration = p->seekable ? p->duration : 0;
    for (int type = 0, index = 0; type < 2; type++)
        for (int i = 0; i < p->track_count[type]; i++, index++)
            p->tracks[index].selected = p->tracks[index].id == p->selected_track[type];
}

static bool cui__mpv_output(cui_mpv *p, int width, int height) {
    if (width == p->width && height == p->height) return true;
    size_t stride = width > 0 ? ((size_t)width * 4 + 63) & ~(size_t)63 : 0;
    if (width <= 0 || height <= 0 || width > (INT_MAX - 63) / 4 ||
        (size_t)height > (SIZE_MAX - 63) / stride) {
        p->error = MPV_ERROR_INVALID_PARAMETER;
        return false;
    }
    cui__mpv_release_output(p);
    cui_ctx *ui = p->app->ui;
    if (!p->app->gl) {
        /* Align both pointer and stride: otherwise libmpv may copy the frame. */
        p->pixel_storage = malloc(stride * (size_t)height + 63);
        if (!p->pixel_storage) { p->error = MPV_ERROR_NOMEM; return false; }
        p->pixels = (unsigned char *)(((uintptr_t)p->pixel_storage + 63) & ~(uintptr_t)63);
    } else {
        int maximum = 0;
        p->gl.GetIntegerv(0x0D33, &maximum);     /* GL_MAX_TEXTURE_SIZE */
        if (width > maximum || height > maximum) { p->error = MPV_ERROR_INVALID_PARAMETER; return false; }
    }
    p->video.frame = ui->renderer.texture_create(ui->renderer.user, width, height, CUI_TEXTURE_RGBA8, NULL);
    if (!p->video.frame) { cui__mpv_release_output(p); p->error = MPV_ERROR_NOMEM; return false; }
    if (p->app->gl) {
        p->gl.GenFramebuffers(1, &p->fbo);
        p->gl.BindFramebuffer(0x8D40, p->fbo);   /* GL_FRAMEBUFFER */
        p->gl.FramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1,
            cui_gl_texture_name(p->app->gl, p->video.frame), 0);
        if (!p->fbo || p->gl.CheckFramebufferStatus(0x8D40) != 0x8CD5) {
            cui__mpv_release_output(p); p->error = MPV_ERROR_UNSUPPORTED; return false;
        }
    }
    p->width = width; p->height = height;
    return true;
}

bool cui_mpv_update(cui_mpv *p, int width, int height) {
    if (!p) return false;
    cui__mpv_events(p);
    int framebuffer = 0;
    if (p->app->gl) {
        p->gl.GetIntegerv(0x8CA6, &framebuffer); /* GL_DRAW_FRAMEBUFFER_BINDING */
        cui__mpv_gl_defaults(p);
    }
    uint64_t flags = mpv_render_context_update(p->render);
    if (width <= 0 || height <= 0) {
        bool changed = p->video.frame != 0;
        cui__mpv_release_output(p);
        if (flags & MPV_RENDER_UPDATE_FRAME) {
            int skip = 1;
            mpv_render_param params[] = { { MPV_RENDER_PARAM_SKIP_RENDERING, &skip }, { 0, NULL } };
            int err = mpv_render_context_render(p->render, params);
            if (err < 0) p->error = err;
        }
        if (p->app->gl) p->gl.BindFramebuffer(0x8D40, (unsigned int)framebuffer);
        return changed;
    }
    if (p->idle && !p->video.frame && !(flags & MPV_RENDER_UPDATE_FRAME)) {
        if (p->app->gl) p->gl.BindFramebuffer(0x8D40, (unsigned int)framebuffer);
        return false;
    }
    bool resized = width != p->width || height != p->height;
    if (!resized && !(flags & MPV_RENDER_UPDATE_FRAME)) {
        if (p->app->gl) p->gl.BindFramebuffer(0x8D40, (unsigned int)framebuffer);
        return false;
    }
    bool ok = cui__mpv_output(p, width, height);
    /* CUI samples texture v=0 at the top; an offscreen FBO needs no flip. */
    int block = 0, flip = 0;
    int size[2] = { width, height };
    size_t stride = ((size_t)width * 4 + 63) & ~(size_t)63;
    mpv_opengl_fbo fbo = { (int)p->fbo, width, height, 0 };
    mpv_render_param gl_params[] = { { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flip }, { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block }, { 0, NULL } };
    mpv_render_param sw_params[] = { { MPV_RENDER_PARAM_SW_SIZE, size }, { MPV_RENDER_PARAM_SW_FORMAT, "rgba" },
        { MPV_RENDER_PARAM_SW_STRIDE, &stride }, { MPV_RENDER_PARAM_SW_POINTER, p->pixels },
        { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block }, { 0, NULL } };
    if (ok) {
        if (p->app->gl) cui__mpv_gl_defaults(p);
        int err = mpv_render_context_render(p->render, p->app->gl ? gl_params : sw_params);
        if (err < 0) { p->error = err; ok = false; }
        else if (!p->app->gl) p->app->ui->renderer.texture_update(p->app->ui->renderer.user,
            p->video.frame, 0, 0, width, height, p->pixels, (int)stride);
    } else if (flags & MPV_RENDER_UPDATE_FRAME) {
        int skip = 1;
        mpv_render_param params[] = { { MPV_RENDER_PARAM_SKIP_RENDERING, &skip }, { 0, NULL } };
        if (p->app->gl) cui__mpv_gl_defaults(p);
        int err = mpv_render_context_render(p->render, params);
        if (err < 0 && !p->error) p->error = err;
    }
    if (p->app->gl) p->gl.BindFramebuffer(0x8D40, (unsigned int)framebuffer);
    return ok;
}

const cui_video *cui_mpv_state(const cui_mpv *p) { return p ? &p->video : NULL; }
int cui_mpv_last_error(const cui_mpv *p) { return p ? p->error : MPV_ERROR_INVALID_PARAMETER; }
const char *cui_mpv_error_string(int error) { return mpv_error_string(error); }

const cui_mpv_track *cui_mpv_tracks(const cui_mpv *p, cui_mpv_track_type type, int *count) {
    if (count) *count = 0;
    if (!p || (type != CUI_MPV_AUDIO && type != CUI_MPV_SUBTITLE) || !p->track_count[type]) return NULL;
    if (count) *count = p->track_count[type];
    return p->tracks + (type == CUI_MPV_SUBTITLE ? p->track_count[CUI_MPV_AUDIO] : 0);
}

int cui_mpv_select_track(cui_mpv *p, cui_mpv_track_type type, int64_t id) {
    if (!p || (type != CUI_MPV_AUDIO && type != CUI_MPV_SUBTITLE) || id < CUI_MPV_TRACK_AUTO)
        return MPV_ERROR_INVALID_PARAMETER;
    int count = 0;
    const cui_mpv_track *tracks = cui_mpv_tracks(p, type, &count);
    if (id > 0) {
        int i = 0;
        while (i < count && tracks[i].id != id) i++;
        if (i == count) return MPV_ERROR_INVALID_PARAMETER;
    }
    const char *property = type == CUI_MPV_AUDIO ? "aid" : "sid";
    int error;
    if (id > 0) error = mpv_set_property_async(p->mpv, 0, property, MPV_FORMAT_INT64, &id);
    else {
        const char *value = id == CUI_MPV_TRACK_AUTO ? "auto" : "no";
        error = mpv_set_property_async(p->mpv, 0, property, MPV_FORMAT_STRING, &value);
    }
    if (error < 0) p->error = error;
    return error;
}

int cui_mpv_load_subtitle(cui_mpv *p, const char *path) {
    if (!p || !path || !*path) return MPV_ERROR_INVALID_PARAMETER;
    const char *args[] = { "sub-add", path, "cached", NULL };
    int error = mpv_command_async(p->mpv, 0, args);
    p->error = error;
    return error;
}

static int cui__mpv_seek(cui_mpv *p, double position) {
    mpv_node values[3] = { { .u.string = "seek", .format = MPV_FORMAT_STRING },
        { .u.double_ = position, .format = MPV_FORMAT_DOUBLE },
        { .u.string = "absolute", .format = MPV_FORMAT_STRING } };
    mpv_node_list list = { 3, values, NULL };
    mpv_node command = { .u.list = &list, .format = MPV_FORMAT_NODE_ARRAY };
    return mpv_command_node_async(p->mpv, 0, &command);
}

int cui_mpv_apply(cui_mpv *p, const cui_video *v, int events) {
    if (!p || !v || ((events & CUI_PLAYER_SEEK) && (!isfinite(v->position) || v->position < 0)) ||
        ((events & CUI_PLAYER_VOLUME) && (!isfinite(v->volume) || v->volume < 0 || v->volume > 1)))
        return MPV_ERROR_INVALID_PARAMETER;
    int err = 0;
    if (events & CUI_PLAYER_TOGGLE) {
        if (v->playing && p->eof && !(events & CUI_PLAYER_SEEK)) err = cui__mpv_seek(p, 0);
        int pause = !v->playing;
        if (err >= 0) err = mpv_set_property_async(p->mpv, 0, "pause", MPV_FORMAT_FLAG, &pause);
    }
    if (err >= 0 && (events & CUI_PLAYER_SEEK)) {
        err = cui__mpv_seek(p, v->position);
    }
    if (err >= 0 && (events & CUI_PLAYER_VOLUME)) {
        double volume = v->volume * 100.0;
        err = mpv_set_property_async(p->mpv, 0, "volume", MPV_FORMAT_DOUBLE, &volume);
    }
    if (err >= 0 && (events & CUI_PLAYER_MUTE)) {
        int mute = v->muted;
        err = mpv_set_property_async(p->mpv, 0, "mute", MPV_FORMAT_FLAG, &mute);
    }
    if (err < 0) p->error = err;
    return err;
}
